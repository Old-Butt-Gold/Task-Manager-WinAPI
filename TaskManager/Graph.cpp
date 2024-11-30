#include "Graph.h"

int GetCurrentRAMUsage() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memInfo)) {
        DWORDLONG totalRAM = memInfo.ullTotalPhys;
        DWORDLONG freeRAM = memInfo.ullAvailPhys;

        int ramUsage = static_cast<int>((1.0 - (double)freeRAM / static_cast<double>(totalRAM)) * 100);
        return ramUsage;
    }

    return 0;
}

int GetCurrentCPUUsage(SYSTEM_INFO sysInfo, SYSTEM_PROCESSOR_TIMES* CurrentSysProcTimes, SYSTEM_PROCESSOR_TIMES* PreviousSysProcTimes, ZwQuerySystemInformationFunc ZwQuerySystemInformation)
{
    static ULONGLONG oldTime = GetTickCount64();
    ULONGLONG nowTime = GetTickCount64();
    ULONGLONG perTime = nowTime - oldTime;
    oldTime = nowTime;

    ZwQuerySystemInformation(SystemProcessorTimesCLASS, &CurrentSysProcTimes[0], sizeof(SYSTEM_PROCESSOR_TIMES) * sysInfo.dwNumberOfProcessors, nullptr);

    ULONGLONG totalIdleTime = 0;
    for (int j = 0; j < static_cast<int>(sysInfo.dwNumberOfProcessors); j++) {
        totalIdleTime += CurrentSysProcTimes[j].IdleTime - PreviousSysProcTimes[j].IdleTime;
    }

    ULONGLONG averageIdleTime = totalIdleTime / sysInfo.dwNumberOfProcessors;

    double idleTimeMs = static_cast<double>(averageIdleTime) / 10000.0;

    double cpuUsage = 100.0 * (1.0 - (idleTimeMs / perTime));

    if (cpuUsage < 0 || cpuUsage > 100) {
        cpuUsage = 0;
    }

    memcpy(&PreviousSysProcTimes[0], &CurrentSysProcTimes[0], sizeof(SYSTEM_PROCESSOR_TIMES) * sysInfo.dwNumberOfProcessors);
    return static_cast<int>(cpuUsage);
}

std::wstring GetSystemUptime() {
    ULONGLONG systemUptime = GetTickCount64();
    ULONGLONG seconds = (systemUptime / 1000) % 60;
    ULONGLONG minutes = (systemUptime / (static_cast<ULONGLONG>(60) * 1000)) % 60;
    ULONGLONG hours = (systemUptime / (static_cast<ULONGLONG>(60) * 1000 * 60)) % 24;
    ULONGLONG days = (systemUptime / (static_cast<ULONGLONG>(60) * 1000 * 60 * 24));

    return L"Время работы: " + std::to_wstring(days) + L" дн. " +
           std::to_wstring(hours) + L" ч. " +
           std::to_wstring(minutes) + L" мин. " +
           std::to_wstring(seconds) + L" сек.";
}

std::wstring GetMemoryInfo()
{
    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memoryStatus))
    {
        ULONGLONG totalMemoryMB = memoryStatus.ullTotalPhys / (static_cast<ULONGLONG>(1024 * 1024));
        ULONGLONG freeMemoryMB = memoryStatus.ullAvailPhys / (static_cast<ULONGLONG>(1024 * 1024));

        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        std::wstring architecture;
        WORD arch = sysInfo.wProcessorArchitecture;

        switch (arch) {
        case PROCESSOR_ARCHITECTURE_AMD64: architecture = L"x64 (AMD or Intel)"; break;
        case PROCESSOR_ARCHITECTURE_ARM:   architecture = L"ARM"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: architecture = L"ARM64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: architecture = L"x86 (32-bit)"; break;
        default: architecture = L"Неизвестная архитектура"; break;
        }

        DWORD logicalCores = sysInfo.dwNumberOfProcessors;

        ULONG physicalCores = 0;
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &physicalCores);
        physicalCores /= sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);

        HKEY hKey;
        DWORD data, dataSize = sizeof(data);
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                         L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 
                         0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            RegQueryValueEx(hKey, L"~MHz", nullptr, nullptr, (LPBYTE)&data, &dataSize);
            RegCloseKey(hKey);
        }
        else {
            data = 0;
        }
        
        std::wstring result = L"Общий объем памяти: " + std::to_wstring(totalMemoryMB) + L" МБ; " +
                              L"Свободно: " + std::to_wstring(freeMemoryMB) + L" МБ; \r\n" +
                              L"Количество логических ядер: " + std::to_wstring(logicalCores) + L"; " +
                              L"Количество физических ядер: " + std::to_wstring(physicalCores) + L"; \r\n" +
                              L"Архитектура процессора: " + architecture + L"; " +
                              L"Частота процессора: " + std::to_wstring(data) + L" МГц\r\n";
        return result;
    }
    return L"Ошибка получения информации о памяти";
}

void DrawGraph(HWND hPerfomance, std::vector<int>& history, const int maxHistory)
{
    HDC hdc = GetDC(hPerfomance);
    RECT rect;
    GetClientRect(hPerfomance, &rect);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HDC memDC = CreateCompatibleDC(hdc);
    
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    HBRUSH backgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(memDC, &rect, backgroundBrush);
    DeleteObject(backgroundBrush);

    SetBkMode(memDC, TRANSPARENT);
    SetStretchBltMode(memDC, HALFTONE);

    HPEN gridPen = CreatePen(PS_DOT, 1, RGB(60, 60, 60));
    SelectObject(memDC, gridPen);
    for (int i = 1; i <= 10; ++i) {
        int y = height - 20 - (height - 40) * i / 10;
        MoveToEx(memDC, 0, y, NULL);
        LineTo(memDC, width, y);
    }

    if (!history.empty()) {
        HPEN graphPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 127));
        SelectObject(memDC, graphPen);

        int spacing = width / maxHistory;
        int prevX = 0;
        int prevY = height - 20 - (history[0] * (height - 20) / 100);

        for (int i = 1; i < static_cast<int>(history.size()); ++i) {
            int x = i * spacing;
            int y = height - 20 - (history[i] * (height - 20) / 100);

            MoveToEx(memDC, prevX, prevY, NULL);
            LineTo(memDC, x, y);

            prevX = x;
            prevY = y;
        }
        DeleteObject(graphPen);
    }

    DeleteObject(gridPen);
    
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(hPerfomance, hdc);
}

void PushValue(std::vector<int>& history, const int maxHistory, int newValue)
{
    history.push_back(newValue);
    if (static_cast<int>(history.size()) > maxHistory) {
        history.erase(history.begin()); 
    }
}