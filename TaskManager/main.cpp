#ifndef UNICODE
#define UNICODE
#endif

#define MainName L"Task Manager"
#define TaskName L"Run Task"

#define SCREEN_WIDTH  GetSystemMetrics(SM_CXSCREEN)
#define SCREEN_HEIGHT GetSystemMetrics(SM_CYSCREEN)

#define ID_MENU_RUN_TASK 0x2010
#define ID_MENU_EXIT 0x2011
#define ID_MENU_ABOUT 0x2012
#define ID_MENU_RARE 0x3010
#define ID_MENU_NORMAL 0x3011
#define ID_MENU_OFTEN 0x3012
#define ID_MENU_NONE 0x3013
#define ID_MENU_UPDATE 0x4010

#define IDC_TABCONTROL 0x5010
#define IDC_LISTVIEW_PROCESSES 0x6010
#define IDC_PROGRESSBAR_CPU 0x7010
#define IDC_PROGRESSBAR_RAM 0x8010
#define IDM_CLOSE_PROCESS 0x5011
#define IDM_OPEN_LOCATION 0x5012

#define TIMER_PROCESSES 0x1
#define TIMER_GRAPH 0x2

#define IDC_CREATETASKWINDOW 0x9010

#define IDC_EDIT_TASK 0x9011
#define IDC_CHECK_ADMIN 0x9012
#define IDC_BTN_OK 0x9013
#define IDC_BTN_CANCEL 0x9014
#define IDC_BTN_BROWSE 0x9015

#include <Windows.h>
#include <iomanip>
#include <CommCtrl.h>
#include <wingdi.h>
#include <string>
#include "resource.h"
#include <TlHelp32.h> // For process enumeration
#include <Psapi.h>    // For process memory information
#include <shellapi.h>
#include <map>
#include <sstream>
#include <set>
#include <fstream>
#include <shlobj.h>
#include <vector>
#include <shlwapi.h>
#include "dwmapi.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")

typedef enum _PreferredAppMode
{
    Default,
    AllowDark,
    ForceDark,
    ForceLight,
    Max
} PreferredAppMode;

typedef PreferredAppMode(WINAPI* fnSetPreferredAppMode)(PreferredAppMode appMode);

typedef struct system_processor_times {
    ULONGLONG IdleTime;
    ULONGLONG KernelTime;
    ULONGLONG UserTime;
    ULONGLONG DpcTime;
    ULONGLONG InterruptTime;
    ULONG     InterruptCount;
} SYSTEM_PROCESSOR_TIMES;

typedef NTSTATUS (NTAPI *ZwQuerySystemInformationFunc)(
    ULONG SystemInformationClass, 
    PVOID SystemInformation, 
    ULONG SystemInformationLength, 
    PULONG ReturnLength
);

#define SystemProcessorTimesCLASS 8

#define ProcessListViewX 0
#define ProcessListViewY 25

#define TabControlX 10
#define TabControlY 10

void EnableDarkMode(HWND hwnd)
{
    HMODULE hUxTheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUxTheme)
    {
        auto SetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxTheme, "SetPreferredAppMode");
        if (SetPreferredAppMode)
        {
            SetPreferredAppMode(AllowDark);
        }
        FreeLibrary(hUxTheme);
    }

    BOOL isDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &isDarkMode, sizeof(isDarkMode));
}

HMENU CreateAppMenu() {
    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    HMENU hRefreshMenu = CreatePopupMenu();
    
    AppendMenu(hFileMenu, MF_STRING, ID_MENU_RUN_TASK, L"Запустить новую задачу");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, ID_MENU_EXIT, L"Выход");

    AppendMenu(hRefreshMenu, MF_STRING, ID_MENU_RARE, L"Редко (5 сек)");
    AppendMenu(hRefreshMenu, MF_STRING, ID_MENU_NORMAL, L"Нормально (3 сек)");
    AppendMenu(hRefreshMenu, MF_STRING, ID_MENU_OFTEN, L"Часто (1 сек)");
    AppendMenu(hRefreshMenu, MF_STRING, ID_MENU_NONE, L"Отключить");

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"Файл");
    AppendMenu(hMenu, MF_STRING, ID_MENU_ABOUT, L"О разработчике");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hRefreshMenu, L"Частота обновления");
    AppendMenu(hMenu, MF_STRING, ID_MENU_UPDATE, L"Обновить сейчас");

    return hMenu;
}

HIMAGELIST CreateTabImageList() {
    HIMAGELIST hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);
    ImageList_SetBkColor(hImageList, CLR_NONE);
    HICON hIcon1 = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_PROCESSICON));
    HICON hIcon2 = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_GRAPHICON));

    ImageList_AddIcon(hImageList, hIcon1);
    ImageList_AddIcon(hImageList, hIcon2);

    DestroyIcon(hIcon1);
    DestroyIcon(hIcon2);
    return hImageList;
}

void AddTabItems(HWND hTab) {
    HIMAGELIST hImageList = CreateTabImageList();
    TabCtrl_SetImageList(hTab, hImageList);

    TCITEM tie = { };
    tie.mask = TCIF_TEXT | TCIF_IMAGE;

    tie.pszText = (LPWSTR)L"Процессы";
    tie.iImage = 0;
    TabCtrl_InsertItem(hTab, 0, &tie);

    tie.pszText = (LPWSTR)L"Производительность";
    tie.iImage = 1;
    TabCtrl_InsertItem(hTab, 1, &tie);
}

HMENU CreateContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, IDM_CLOSE_PROCESS, L"Закрыть процесс");
    AppendMenu(hMenu, MF_STRING, IDM_OPEN_LOCATION, L"Открыть расположение файла");
    return hMenu;
}

void HandleContextMenu(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreateContextMenu();
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

void CloseProcessById(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (hProcess) {
        TerminateProcess(hProcess, 0) ? MessageBox(NULL, L"Процесс успешно завершён.", L"Успех", MB_OK | MB_ICONINFORMATION) : MessageBox(NULL, L"Не удалось завершить процесс.", L"Ошибка", MB_OK | MB_ICONERROR);
        CloseHandle(hProcess);
    } else {
        MessageBox(NULL, L"Не удалось открыть процесс для завершения.", L"Ошибка", MB_OK | MB_ICONERROR);
    }
}

void OpenProcessLocation(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        TCHAR fullPath[MAX_PATH] = {0};
        DWORD size = MAX_PATH;

        if (QueryFullProcessImageName(hProcess, 0, fullPath, &size)) {
            std::wstring folderPath(fullPath);
            folderPath = folderPath.substr(0, folderPath.find_last_of(L"\\/"));

            ShellExecute(NULL, L"open", folderPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        } else {
            MessageBox(NULL, L"Не удалось получить путь процесса.", L"Ошибка", MB_OK | MB_ICONERROR);
        }

        CloseHandle(hProcess);
    } else {
        MessageBox(NULL, L"Не удалось открыть процесс для получения пути.", L"Ошибка", MB_OK | MB_ICONERROR);
    }
}

std::wstring GetPriority(DWORD processId)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess)
    {
        DWORD priorityClass = GetPriorityClass(hProcess);
        if (priorityClass == 0) {
            CloseHandle(hProcess);
            return L"Неизвестно";
        }

        std::wstring priorityString;
        switch (priorityClass) {
            case IDLE_PRIORITY_CLASS: priorityString = L"Низкий"; break;
            case BELOW_NORMAL_PRIORITY_CLASS: priorityString = L"Ниже нормального"; break;
            case NORMAL_PRIORITY_CLASS: priorityString = L"Нормальный"; break;
            case ABOVE_NORMAL_PRIORITY_CLASS: priorityString = L"Выше нормального"; break;
            case HIGH_PRIORITY_CLASS: priorityString = L"Высокий"; break;
            case REALTIME_PRIORITY_CLASS: priorityString = L"Реального времени"; break;
            default: priorityString = L"Неизвестно"; break;
        }
        
        CloseHandle(hProcess);
        return priorityString;
    }
    return L"Нет доступа";
}

void ChangeIcon(PROCESSENTRY32 pe32, HWND hListView, LVITEM& lvi, std::map<std::wstring, int>& iconMap)
{
    TCHAR fullPath[MAX_PATH];
    DWORD processNameLength = sizeof(fullPath) / sizeof(TCHAR);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
    if (hProcess) {
        if (QueryFullProcessImageName(hProcess, 0, fullPath, &processNameLength)) {
            std::wstring processPath(fullPath);
            auto it = iconMap.find(processPath);
            if (it != iconMap.end())
            {
                lvi.iImage = it->second;
            } else
            {
                HICON hIcon = ExtractIcon(NULL, fullPath, 0);
                if (hIcon) {
                    int imageIndex = ImageList_AddIcon(ListView_GetImageList(hListView, LVSIL_SMALL), hIcon);
                    if (imageIndex != -1) {
                        lvi.iImage = imageIndex;
                        iconMap[processPath] = imageIndex;
                    }
                    DestroyIcon(hIcon);
                }
            }
        }
        CloseHandle(hProcess);
    }
}

bool Is64BitProcess(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processID);
    if (hProcess) {
        BOOL is64Bit = FALSE;
        IsWow64Process(hProcess, &is64Bit);
        CloseHandle(hProcess);
        return is64Bit == FALSE;
    }
    return false;
}

std::wstring GetProcessTimeCreation(DWORD processId)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
            SYSTEMTIME st;
            FileTimeToSystemTime(&creationTime, &st);
            st.wHour += 3; //For UTC
            wchar_t timeString[100];
            (void)swprintf_s(timeString, 100, L"%02d/%02d/%04d %02d:%02d:%02d",
                       st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
            std::wstring result(timeString);
            return result;
        }
        CloseHandle(hProcess);
    }
    return L"—";
}

std::wstring FormatFloat(int precision, double value) {
    std::wstringstream stream;
    stream.precision(precision);
    stream << std::fixed << value;
    return stream.str();
}

std::wstring GetMemoryUsage(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    std::wstring result = L"—";
    if (hProcess)
    {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {

            double memoryUsageMB = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
            result = FormatFloat(3, memoryUsageMB) + L" MB";
        }
        CloseHandle(hProcess);
    }
    
    return result;
}

HWND CreateProcessListView(HWND hParent) {
    HWND hListView = CreateWindowEx(0, WC_LISTVIEW, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        ProcessListViewX, ProcessListViewY, SCREEN_WIDTH - 40, SCREEN_HEIGHT - 70,
        hParent, (HMENU)IDC_LISTVIEW_PROCESSES, (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE), NULL);

    LVCOLUMN lvColumn;
    
    HIMAGELIST hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);
    ImageList_SetBkColor(hImageList, CLR_NONE);
    
    HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAINICON));
    ImageList_AddIcon(hImageList, LoadIcon(NULL, IDI_WINLOGO));
    ImageList_AddIcon(hImageList, hIcon);
    
    DestroyIcon(hIcon);
    ListView_SetImageList(hListView, hImageList, LVSIL_SMALL);
    ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP | LVS_EX_DOUBLEBUFFER);

    const std::wstring columnHeaders[] = {
        L"Имя процесса", L"ID процесса", L"ID родительского процесса", L"Приоритет",
        L"Загрузка ЦП", L"Загрузка Памяти", L"Время создания", L"Путь к файлу", L"Битность"
    };

    int width = SCREEN_WIDTH - 40;
    
    const float columnWidths[] = { width * 0.15f, width * 0.12f, width * 0.12f, width * 0.11f, width * 0.1f, width * 0.10f, width * 0.092f, width * 0.15f, width * 0.05f };
    
    for (int i = 0; i < 9; i++) {
        lvColumn.mask = i == 0 
                        ? LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT | LVCF_IMAGE
                        : LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
        
        lvColumn.fmt = LVCFMT_CENTER;
        lvColumn.cchTextMax = MAX_PATH;
        lvColumn.pszText = const_cast<LPWSTR>(columnHeaders[i].c_str());
        lvColumn.cx = columnWidths[i];
        lvColumn.iImage = 1;
        lvColumn.iSubItem = i;
        
        ListView_InsertColumn(hListView, i, &lvColumn);
    }

    return hListView;
}

int FindProcessInListViewByPID(HWND hListView, DWORD processID) {
    LVFINDINFO findInfo = { };
    findInfo.flags = LVFI_PARAM;
    findInfo.lParam = processID;
    return ListView_FindItem(hListView, -1, &findInfo);
}

void RemoveStaleProcesses(HWND hListView, const std::set<DWORD>& existingProcesses) {
    int itemCount = ListView_GetItemCount(hListView);
    for (int i = itemCount - 1; i >= 0; --i) {
        LVITEM lvi = { };
        lvi.iItem = i;
        lvi.mask = LVIF_PARAM;

        if (ListView_GetItem(hListView, &lvi)) {
            DWORD processID = static_cast<DWORD>(lvi.lParam);

            if (existingProcesses.find(processID) == existingProcesses.end()) {
                ListView_DeleteItem(hListView, i);
            }
        }
    }
}

std::wstring GetCpuUsage(DWORD processID) {
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    ULARGE_INTEGER uKernel, uUser, uNow;
    
    if (HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processID)) {
        if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
            uKernel.LowPart = ftKernel.dwLowDateTime;
            uKernel.HighPart = ftKernel.dwHighDateTime;

            uUser.LowPart = ftUser.dwLowDateTime;
            uUser.HighPart = ftUser.dwHighDateTime;

            FILETIME ftNow;
            GetSystemTimeAsFileTime(&ftNow);
            uNow.LowPart = ftNow.dwLowDateTime;
            uNow.HighPart = ftNow.dwHighDateTime;

            double processTime = (uKernel.QuadPart + uUser.QuadPart) / 1000.0;
            double totalTime = uNow.QuadPart / 10000000.0;

            double cpuUsage = processTime / totalTime * 100.0;

            CloseHandle(hProcess);
            
            return FormatFloat(6, cpuUsage) + L"%";
        }
        CloseHandle(hProcess);
    }
    return L"—";
}

void PopulateProcessListView(HWND hListView, std::map<std::wstring, int>& iconMap)
{
    SCROLLINFO si = { };
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_POS;
    GetScrollInfo(hListView, SB_VERT, &si);
    int scrollPos = si.nPos;

    SendMessage(hListView, WM_SETREDRAW, FALSE, 0);

    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32 pe32 = { };
    pe32.dwSize = sizeof(PROCESSENTRY32);
    std::set<DWORD> existingProcesses;
    
    if (Process32First(hProcessSnap, &pe32)) {
        do {
            std::wstring exeFile = pe32.szExeFile;
            DWORD processID = pe32.th32ProcessID;

            int itemIndex = FindProcessInListViewByPID(hListView, processID);
            
            LVITEM lvi = { };
            lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
            lvi.pszText = const_cast<LPWSTR>(exeFile.c_str());
            //lvi.iItem = (itemIndex == -1) ? 0 : ListView_GetItemCount(hListView);
            lvi.iItem = (itemIndex == -1) ? ListView_GetItemCount(hListView) : itemIndex;
            lvi.iImage = 0;
            lvi.cchTextMax = MAX_PATH;
            lvi.lParam = processID;
            
            ChangeIcon(pe32, hListView, lvi, iconMap);

            if (itemIndex == -1) {
                ListView_InsertItem(hListView, &lvi);
            } else {
                ListView_SetItem(hListView, &lvi);
            }
            
            std::wstring processId = std::to_wstring(pe32.th32ProcessID);
            ListView_SetItemText(hListView, lvi.iItem, 1, const_cast<LPWSTR>(processId.c_str()));

            std::wstring parent = pe32.th32ParentProcessID == 0 ? L"Главный процесс" : std::to_wstring(pe32.th32ParentProcessID);
            ListView_SetItemText(hListView, lvi.iItem, 2, const_cast<LPWSTR>(parent.c_str()));

            std::wstring priority = GetPriority(pe32.th32ProcessID);
            ListView_SetItemText(hListView, lvi.iItem, 3, const_cast<LPWSTR>(priority.c_str()));

            std::wstring cpuUsage = GetCpuUsage(processID);
            ListView_SetItemText(hListView, lvi.iItem, 4, const_cast<LPWSTR>(cpuUsage.c_str()));

            std::wstring memUsage = GetMemoryUsage(processID);
            ListView_SetItemText(hListView, lvi.iItem, 5, const_cast<LPWSTR>(memUsage.c_str()))
            
            std::wstring timeCreation = GetProcessTimeCreation(pe32.th32ProcessID);
            ListView_SetItemText(hListView, lvi.iItem, 6, const_cast<LPWSTR>(timeCreation.c_str()));

            TCHAR fullPath[MAX_PATH];
            DWORD processNameLength = sizeof(fullPath) / sizeof(TCHAR);
            if (QueryFullProcessImageName(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID), 0, fullPath, &processNameLength)) {
                ListView_SetItemText(hListView, lvi.iItem, 7, (LPWSTR)fullPath)
            } else
            {
                ListView_SetItemText(hListView, lvi.iItem, 7, (LPWSTR)L"—");
            }
            
            ListView_SetItemText(hListView, lvi.iItem, 8, Is64BitProcess(pe32.th32ProcessID) ? (LPWSTR)L"64-бит" : (LPWSTR)L"32-бит")

            existingProcesses.insert(processID);
        } while (Process32Next(hProcessSnap, &pe32));
    }
    CloseHandle(hProcessSnap);

    RemoveStaleProcesses(hListView, existingProcesses);

    SendMessage(hListView, WM_SETREDRAW, TRUE, 0);

    si.nPos = scrollPos;
    SetScrollInfo(hListView, SB_VERT, &si, TRUE);
}

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

int GetCurrentCPUUsage(SYSTEM_INFO sysInfo, SYSTEM_PROCESSOR_TIMES* CurrentSysProcTimes,
    SYSTEM_PROCESSOR_TIMES* PreviousSysProcTimes,
    ZwQuerySystemInformationFunc ZwQuerySystemInformation)
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

LRESULT CALLBACK ProcessListViewProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static int selectedItemIndex;
    switch (uMsg)
    {
        case WM_COMMAND:
            {
                int wmId = LOWORD(wParam);
                //int wmEvent = HIWORD(wParam);

                switch (wmId) 
                {
                    case IDM_CLOSE_PROCESS:
                    {
                        wchar_t buffer[MAX_PATH] = { 0 }; 
                        ListView_GetItemText(hwnd, selectedItemIndex, 1, buffer, 31); 
                        buffer[MAX_PATH - 1] = L'\0'; 

                        DWORD processId = _wtoi(buffer);
                        CloseProcessById(processId);
                    }
                    break;
                    
                    case IDM_OPEN_LOCATION:
                    {
                        wchar_t buffer[MAX_PATH] = { 0 }; 
                        ListView_GetItemText(hwnd, selectedItemIndex, 1, buffer, 31);
                        buffer[MAX_PATH - 1] = L'\0';

                        DWORD processId = _wtoi(buffer);
                        OpenProcessLocation(processId);
                    }
                    break;
                default: ;
                }
            }
            break;
        
            case WM_CONTEXTMENU: {
                if (wParam == (WPARAM)hwnd)
                {
                    LVHITTESTINFO hitTestInfo = { };
                    POINT pt;
                    GetCursorPos(&pt);
                    ScreenToClient(hwnd, &pt);
                    hitTestInfo.pt = pt;
                    ListView_HitTest(hwnd, &hitTestInfo);
                    selectedItemIndex = hitTestInfo.iItem;
                    if (selectedItemIndex != -1)
                    {
                        HandleContextMenu(hwnd, wParam, lParam);
                    }
                }
                break;
            }

        case WM_NOTIFY:
            {
                LPNMHDR nmhdr = (LPNMHDR)lParam;

                if (nmhdr->code == HDN_ITEMCHANGING) {
                    LPNMHEADER pnmhHeader = (LPNMHEADER)lParam;
                    if (pnmhHeader->pitem->cxy < 30) {
                        pnmhHeader->pitem->cxy = 30;
                    }
                }


                    if (nmhdr->hwndFrom == ListView_GetHeader(hwnd) && nmhdr->code == NM_CUSTOMDRAW)
                    {
                        LPNMCUSTOMDRAW lpNMCustomDraw = (LPNMCUSTOMDRAW)lParam;
        
                        switch (lpNMCustomDraw->dwDrawStage)
                        {
                        case CDDS_PREPAINT:
                            return CDRF_NOTIFYITEMDRAW;

                        case CDDS_ITEMPREPAINT:
                            {
                                COLORREF rgb;
                                if (lpNMCustomDraw->uItemState & CDIS_SELECTED)
                                {
                                    rgb = RGB(26, 188, 156);  
                                }
                                else
                                {
                                    rgb = RGB(47, 45, 60);
                                }
                                SetBkColor(lpNMCustomDraw->hdc, rgb);
                                SetTextColor(lpNMCustomDraw->hdc, RGB(255, 255, 255));

                                RECT rc = lpNMCustomDraw->rc;
                                HBRUSH hBrush = CreateSolidBrush(RGB(31, 29, 40));
                                FillRect(lpNMCustomDraw->hdc, &rc, hBrush);
                                DeleteObject(hBrush);

                                RECT textRect = rc;
                                InflateRect(&textRect, -1, -1);

                                hBrush = CreateSolidBrush(rgb);
                                FillRect(lpNMCustomDraw->hdc, &textRect, hBrush);
                                DeleteObject(hBrush);
                                
                                wchar_t text[MAX_PATH];
                                HDITEM hdi = { };
                                hdi.mask = HDI_TEXT;
                                hdi.pszText = text;
                                hdi.cchTextMax = std::size(text);

                                Header_GetItem(nmhdr->hwndFrom, lpNMCustomDraw->dwItemSpec, &hdi);

                                DrawText(lpNMCustomDraw->hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                                return CDRF_SKIPDEFAULT;
                            }
                        default: ;
                        }
                    }

            }

        default:
            return CallWindowProc((WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA), hwnd, uMsg, wParam, lParam);
    }
    return CallWindowProc((WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA), hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK TabControlProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_NOTIFY:
        {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            
            if (pnmh->code == NM_CUSTOMDRAW)
            {
                LPNMLVCUSTOMDRAW pLVCD = (LPNMLVCUSTOMDRAW)lParam;
                switch (pLVCD->nmcd.dwDrawStage)
                {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;
     
                    case CDDS_ITEMPREPAINT:
                        pLVCD->clrText = RGB(255, 255, 255);
                        pLVCD->clrTextBk = RGB(57, 66, 100);
                        return CDRF_DODEFAULT;
     
                    default:
                        break;
                }
            }
            break;
        }
 
    case WM_CTLCOLORSTATIC:
        {
            HDC hdcStatic = (HDC)wParam;
            //HWND hStatic = (HWND)lParam;
 
            SetBkColor(hdcStatic, RGB(43, 41, 55));
            SetTextColor(hdcStatic, RGB(255, 255, 255));
 
            HBRUSH hBrush = CreateSolidBrush(RGB(43, 41, 55));
            return (LONG_PTR)hBrush; 
        }
        
 
    default:
        return CallWindowProc((WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA), hwnd, uMsg, wParam, lParam);
    }
    return CallWindowProc((WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA), hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK EditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN) {
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'A') {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
    }
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
        SendMessage(GetParent(hwnd), WM_COMMAND, IDC_BTN_OK, 0);
        return 0;
    }
    return CallWindowProc((WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA), hwnd, uMsg, wParam, lParam);
}

int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) {
    if (uMsg == BFFM_INITIALIZED) {
        RECT rcScreen, rcDialog;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcScreen, 0);
        GetWindowRect(hwnd, &rcDialog);

        int posX = (rcScreen.right - rcDialog.right + rcDialog.left) / 2 - (rcDialog.right - rcDialog.left) / 2;
        SetWindowPos(hwnd, NULL, posX, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }
    return 0;
}

BOOL SelectFolder(HWND hwnd, LPWSTR folderPath, int bufferSize) {
    HWND dialogHwnd = NULL;

    BROWSEINFO bi = { 0 };
    bi.lpszTitle = L"Выберите папку или файл";
    bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_BROWSEINCLUDEFILES;
    bi.hwndOwner = hwnd;
    bi.lParam = (LPARAM)&dialogHwnd;
    bi.lpfn = BrowseCallbackProc;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl != NULL) {
        if (SHGetPathFromIDList(pidl, folderPath)) {
            CoTaskMemFree(pidl);
            return TRUE;
        }
        CoTaskMemFree(pidl);
    }
    return FALSE;
}

LRESULT CALLBACK RunTaskProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hEdit;
    switch (uMsg) {
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HBRUSH hBrushBackground = CreateSolidBrush(RGB(43, 41, 55));
            FillRect(hdc, &ps.rcPaint, hBrushBackground);
            DeleteObject(hBrushBackground);    
            EndPaint(hwnd, &ps);
        }
        break;
    
    case WM_CREATE:
        {
            EnableDarkMode(hwnd);
            const int padding = 10;
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            int windowWidth = clientRect.right - clientRect.left;
            int windowHeight = clientRect.bottom - clientRect.top;

            int staticHeight = SCREEN_HEIGHT * 0.035;
            int editHeight = SCREEN_HEIGHT * 0.04;
            int buttonHeight = SCREEN_HEIGHT * 0.05;
            int checkboxHeight = SCREEN_HEIGHT * 0.03;

            int staticY = padding;
            int editY = staticY + staticHeight + padding;
            int checkboxY = editY + editHeight + padding;
            int buttonsY = checkboxY + checkboxHeight + padding;

            int buttonWidth = (windowWidth - 4 * padding) / 3;

            CreateWindowEx(0, WC_STATIC,
                L"Введите имя программы, папки, документа или ресурса интернета, которые требуется открыть:",
                WS_CHILD | WS_VISIBLE, 
                padding, staticY, windowWidth - 2 * padding, staticHeight,
                hwnd, NULL, NULL, NULL);

            hEdit = CreateWindowEx(0, WC_EDIT, NULL,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                padding, editY, windowWidth - 2 * padding, editHeight,
                hwnd, (HMENU)IDC_EDIT_TASK, NULL, NULL);

            WNDPROC oldEditProc = (WNDPROC)GetWindowLongPtr(hEdit, GWLP_WNDPROC);
            SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);
            SetWindowLongPtr(hEdit, GWLP_USERDATA, (LONG_PTR)oldEditProc);

            SHAutoComplete(GetDlgItem(hwnd, IDC_EDIT_TASK), SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);

            CreateWindowEx(0, WC_BUTTON, L"Создать задачу с правами администратора",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                padding, checkboxY, windowWidth - 2 * padding, checkboxHeight,
                hwnd, (HMENU)IDC_CHECK_ADMIN, NULL, NULL);

            CreateWindowEx(0, WC_BUTTON, L"Обзор",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_PUSHBUTTON | WS_TABSTOP,
                padding, buttonsY, buttonWidth, buttonHeight,
                hwnd, (HMENU)IDC_BTN_BROWSE, NULL, NULL);

            CreateWindowEx(0, WC_BUTTON, L"OK",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_PUSHBUTTON | WS_TABSTOP,
                2 * padding + buttonWidth, buttonsY, buttonWidth, buttonHeight,
                hwnd, (HMENU)IDC_BTN_OK, NULL, NULL);

            CreateWindowEx(0, WC_BUTTON, L"Отмена", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_PUSHBUTTON | WS_TABSTOP,
                3 * padding + 2 * buttonWidth, buttonsY, buttonWidth, buttonHeight,
                hwnd, (HMENU)IDC_BTN_CANCEL, NULL, NULL);
        }
        break;
    
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (pDIS->CtlType == ODT_BUTTON) {
            HDC hdc = pDIS->hDC;

            BOOL isHovered = (pDIS->itemState & ODS_SELECTED) || (pDIS->itemState & ODS_FOCUS);

            HBRUSH hBrush = CreateSolidBrush(isHovered ? RGB(26, 188, 156) : RGB(61, 74, 121));
            FillRect(hdc, &pDIS->rcItem, hBrush);
            DeleteObject(hBrush);

            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);

            wchar_t text[32];
            GetWindowText(pDIS->hwndItem, text, ARRAYSIZE(text));
            DrawText(hdc, text, -1, &pDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    break;
    
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
    case IDC_BTN_OK: {
            wchar_t taskName[MAX_PATH];
            GetDlgItemText(hwnd, IDC_EDIT_TASK, taskName, MAX_PATH);

            if (wcslen(taskName) == 0) {
                MessageBox(hwnd, L"Введите текст перед выполнением задачи!", L"Предупреждение", MB_OK | MB_ICONWARNING);
                SetFocus(hEdit);
                break;
            }
            
            BOOL runAsAdmin = IsDlgButtonChecked(hwnd, IDC_CHECK_ADMIN) == BST_CHECKED;

            if (wcslen(taskName) > 0) {
                SHELLEXECUTEINFO sei = { sizeof(SHELLEXECUTEINFO) };
                sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_UNICODE;
                sei.lpVerb = runAsAdmin ? L"runas" : L"open";
                sei.lpFile = taskName;
                sei.nShow = SW_SHOWNORMAL;

                if (!ShellExecuteEx(&sei)) {
                    MessageBox(hwnd, L"Не удалось запустить задачу", L"Ошибка", MB_OK | MB_ICONERROR);
                }
            }
            break;
        }
        case IDC_BTN_BROWSE: {
            wchar_t folderPath[MAX_PATH] = { 0 };
            if (SelectFolder(GetParent(hwnd), folderPath, MAX_PATH)) {
                SetDlgItemText(hwnd, IDC_EDIT_TASK, folderPath);
            }
            break;
        }
    case IDC_BTN_CANCEL:
        DestroyWindow(hwnd);
            break;
    default:;
        }
        break;

    case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(43, 41, 55));
            SetTextColor(hdc, RGB(255, 255, 255));
            return (LRESULT)CreateSolidBrush(RGB(43, 41, 55));
        }
        
    case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(57, 66, 100));
            SetTextColor(hdc, RGB(255, 255, 255));
            return (LRESULT)CreateSolidBrush(RGB(57, 66, 100));
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY: {
            HWND hwndParent = GetParent(hwnd);
            SetWindowLongPtr(hwndParent, GWLP_USERDATA, NULL);
            break;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hTabControl, hListViewProcesses;
    static std::map<std::wstring, int> iconMap;
    static HWND hProgressBarCPU;
    static HWND hProgressBarRAM;

    static HWND hPerfomanceCPU, hPerfomanceRAM;

    static UINT_PTR timerId = 0;  
    static int updateInterval = 1000;

    static HMODULE lib = nullptr;
    static SYSTEM_PROCESSOR_TIMES* CurrentSysProcTimes;
    static SYSTEM_PROCESSOR_TIMES* PreviousSysProcTimes;
    static ZwQuerySystemInformationFunc ZwQuerySystemInformation = nullptr;
    
    static HWND hLabelCPU, hLabelRAM, hLabelUptime, hLabelMemoryInfo;
    static SYSTEM_INFO sysInfo;

    static std::vector<int> cpuUsageHistory;
    static std::vector<int> ramUsageHistory;
    static const int cpuUsageHistoryMax = 100;
    static const int ramUsageHistoryMax = 100;
    
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == IDC_TABCONTROL)
            {
                TCHAR szTabText[256];
                TCITEM tci;
                tci.mask = TCIF_TEXT;
                tci.pszText = szTabText;
                tci.cchTextMax = sizeof(szTabText) / sizeof(TCHAR);
                TabCtrl_GetItem(hTabControl, lpDrawItem->itemID, &tci);

                RECT rcTab = lpDrawItem->rcItem;

                HBRUSH hBrush = CreateSolidBrush((lpDrawItem->itemState & ODS_SELECTED) ? 
                    RGB(57, 66, 100) : RGB(43, 41, 55));
                FillRect(lpDrawItem->hDC, &rcTab, hBrush);
                DeleteObject(hBrush);

                SetTextColor(lpDrawItem->hDC, RGB(255, 255, 255));
                SetBkMode(lpDrawItem->hDC, TRANSPARENT);
                DrawText(lpDrawItem->hDC, szTabText, -1, &lpDrawItem->rcItem,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                return TRUE;
            }
        }

        case WM_PAINT:
        {
           PAINTSTRUCT ps;
           HDC hdc = BeginPaint(hwnd, &ps);

           HBRUSH darkBrush = CreateSolidBrush(RGB(43, 41, 55));
           FillRect(hdc, &ps.rcPaint, darkBrush);
           DeleteObject(darkBrush);

           EndPaint(hwnd, &ps);
        }
        return 0;

        case WM_CREATE:
        {
            GetSystemInfo(&sysInfo);
            CurrentSysProcTimes = new SYSTEM_PROCESSOR_TIMES[sysInfo.dwNumberOfProcessors];    
            PreviousSysProcTimes = new SYSTEM_PROCESSOR_TIMES[sysInfo.dwNumberOfProcessors];    
            ZeroMemory(CurrentSysProcTimes, sizeof(SYSTEM_PROCESSOR_TIMES) * sysInfo.dwNumberOfProcessors);
            ZeroMemory(PreviousSysProcTimes, sizeof(SYSTEM_PROCESSOR_TIMES) * sysInfo.dwNumberOfProcessors);

            lib = LoadLibrary(L"ntdll.dll");
            if (!lib) {
                MessageBox(NULL, L"Не удалось загрузить ntdll.dll", L"Ошибка", MB_OK | MB_ICONERROR);
                PostQuitMessage(0);
                return -1;
            }

            ZwQuerySystemInformation = (ZwQuerySystemInformationFunc)GetProcAddress(lib, "ZwQuerySystemInformation");
            if (!ZwQuerySystemInformation) {
                MessageBox(NULL, L"Не удалось найти функцию ZwQuerySystemInformation", L"Ошибка", MB_OK | MB_ICONERROR);
                FreeLibrary(lib);
                PostQuitMessage(0);
                return -1;
            }
                
            if (ZwQuerySystemInformation(SystemProcessorTimesCLASS, &PreviousSysProcTimes[0], sizeof(SYSTEM_PROCESSOR_TIMES) * sysInfo.dwNumberOfProcessors, nullptr) != 0) {
                MessageBox(NULL, L"Не удалось получить данные о процессорах", L"Ошибка", MB_OK | MB_ICONERROR);
                FreeLibrary(lib);
                PostQuitMessage(0);
                return -1;
            }     

            hTabControl = CreateWindowEx(WS_EX_TRANSPARENT, WC_TABCONTROL, NULL,
            WS_CHILD | WS_VISIBLE | TCS_BUTTONS | TCS_OWNERDRAWFIXED,
                   TabControlX, TabControlY, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 40,
                   hwnd, (HMENU)IDC_TABCONTROL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            WNDPROC oldTabControlProc = (WNDPROC)GetWindowLongPtr(hTabControl, GWLP_WNDPROC);
            SetWindowLongPtr(hTabControl, GWLP_WNDPROC, (LONG_PTR)TabControlProc);
            SetWindowLongPtr(hTabControl, GWLP_USERDATA, (LONG_PTR)oldTabControlProc);    
            
            SetClassLongPtr(hTabControl, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(43, 41, 55)));

            AddTabItems(hTabControl);
            SetMenu(hwnd, CreateAppMenu());

            hListViewProcesses = CreateProcessListView(hTabControl);
            PopulateProcessListView(hListViewProcesses, iconMap);
            ListView_SetBkColor(hListViewProcesses, RGB(57, 66, 100));
                
            WNDPROC oldListViewProcessesProc = (WNDPROC)SetWindowLongPtr(hListViewProcesses, GWLP_WNDPROC, (LONG_PTR)ProcessListViewProc);
            SetWindowLongPtr(hListViewProcesses, GWLP_USERDATA, (LONG_PTR)oldListViewProcessesProc);
                
            hProgressBarCPU = CreateWindowEx(0, PROGRESS_CLASS, L"",
                WS_CHILD | PBS_SMOOTH | PBS_MARQUEE,
                10, 50, 300, 25, hTabControl, (HMENU)IDC_PROGRESSBAR_CPU, 
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
                
            SendMessage(hProgressBarCPU, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SetClassLongPtr(hProgressBarCPU, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(43, 41, 55)));
                
            hProgressBarRAM = CreateWindowEx(0, PROGRESS_CLASS, L"",
                    WS_CHILD | PBS_SMOOTH | PBS_MARQUEE,
                    SCREEN_WIDTH - 340, 50, 300, 25, hTabControl, (HMENU)IDC_PROGRESSBAR_RAM, 
                    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            hLabelCPU = CreateWindowEx(0, WC_STATIC, L"",
            WS_CHILD,
            10, 100, 150, 20, hTabControl, NULL, 
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

                hLabelRAM = CreateWindowEx(0, WC_STATIC, L"",
                    WS_CHILD,
                    SCREEN_WIDTH - 340, 100, 150, 20, hTabControl, NULL, 
                    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

                hLabelUptime = CreateWindowEx(0, WC_STATIC, L"",
                    WS_CHILD,
                    SCREEN_WIDTH / 2 - 150, 10, 300, 20, hTabControl, NULL, 
                    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);    

                hLabelMemoryInfo = CreateWindowEx(0, WC_STATIC, L"",
                WS_CHILD,
                SCREEN_WIDTH / 2 - 200, 40, 600, 60, hTabControl, NULL, 
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            hPerfomanceCPU = CreateWindowEx(0, WC_STATIC, L"", WS_CHILD,
                10, 140, SCREEN_WIDTH / 2 - 50, static_cast<int>(SCREEN_HEIGHT * 0.65), hTabControl, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            hPerfomanceRAM = CreateWindowEx(0, WC_STATIC, L"", WS_CHILD,
                    SCREEN_WIDTH / 2, 140, SCREEN_WIDTH / 2 - 50, static_cast<int>(SCREEN_HEIGHT * 0.65), hTabControl, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
                      
            SendMessage(hProgressBarRAM, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
                
            SendMessage(hProgressBarCPU, PBM_SETBARCOLOR, 0, RGB(26, 188, 156));
            SendMessage(hProgressBarRAM, PBM_SETBARCOLOR, 0, RGB(26, 188, 156));
                
            SetTimer(hwnd, TIMER_GRAPH, 50, NULL);
            timerId = SetTimer(hwnd, TIMER_PROCESSES, updateInterval, NULL);

            WNDCLASS wc = { };
            wc.lpfnWndProc = RunTaskProc;
            wc.hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            wc.lpszClassName = TaskName;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
                
            wc.hIcon = LoadIcon((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), MAKEINTRESOURCE(IDI_MAINICON));
            RegisterClass(&wc);

            EnableDarkMode(hwnd);    
        }
        break;

        case WM_TIMER:
        {
            if (wParam == TIMER_PROCESSES)
            {
                PopulateProcessListView(hListViewProcesses, iconMap);
            }
                
            if (wParam == TIMER_GRAPH)
            {
                static int counter = 0;
                if (counter >= 20)
                {
                    counter = 0;
                    int cpuUsage = GetCurrentCPUUsage(sysInfo, CurrentSysProcTimes, PreviousSysProcTimes, ZwQuerySystemInformation);
                    int ramUsage = GetCurrentRAMUsage();
                
                    SendMessage(hProgressBarCPU, PBM_SETPOS, cpuUsage, 0);
                    SendMessage(hProgressBarRAM, PBM_SETPOS, ramUsage, 0);

                    std::wstring pcTime = GetSystemUptime();
                    std::wstring cpuLabel = L"Загрузка ЦП: " + std::to_wstring(cpuUsage) + L"%";
                    std::wstring ramLabel = L"Загрузка Памяти: " + std::to_wstring(ramUsage) + L"%";

                    SetWindowText(hLabelCPU, cpuLabel.c_str());
                    SetWindowText(hLabelRAM, ramLabel.c_str());
                    SetWindowText(hLabelUptime, pcTime.c_str());
                    SetWindowText(hLabelMemoryInfo, GetMemoryInfo().c_str());
                    
                    PushValue(cpuUsageHistory, cpuUsageHistoryMax, cpuUsage);
                    PushValue(ramUsageHistory, ramUsageHistoryMax, ramUsage);
                } else
                counter++;
                
                DrawGraph(hPerfomanceCPU, cpuUsageHistory, cpuUsageHistoryMax);
                DrawGraph(hPerfomanceRAM, ramUsageHistory, ramUsageHistoryMax);
            }    
        }

        /*case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == ID_SELECT_BTN)
            {

            }
            break;
        }*/

        case WM_KEYDOWN:
        {
            //int wmId = LOWORD(wParam);
            //int wmEvent = HIWORD(wParam);
        }
        break;

        case WM_NOTIFY:
            {
                NMHDR* nmhdr = (NMHDR*)lParam;
                
                if (nmhdr->hwndFrom == hTabControl && nmhdr->code == TCN_SELCHANGE) {
                    int tabIndex = TabCtrl_GetCurSel(hTabControl);
                    ShowWindow(hListViewProcesses, tabIndex == 0 ? SW_SHOW : SW_HIDE);
                    ShowWindow(hProgressBarCPU, tabIndex == 1 ? SW_SHOW : SW_HIDE);
                    ShowWindow(hProgressBarRAM, tabIndex == 1 ? SW_SHOW : SW_HIDE);
                    ShowWindow(hLabelUptime, tabIndex == 1 ? SW_SHOW : SW_HIDE);
                    ShowWindow(hLabelMemoryInfo, tabIndex == 1 ? SW_SHOW :  SW_HIDE);
                    ShowWindow(hLabelCPU, tabIndex == 1 ? SW_SHOW : SW_HIDE);
                    ShowWindow(hLabelRAM, tabIndex == 1 ? SW_SHOW : SW_HIDE);
                    ShowWindow(hPerfomanceCPU, tabIndex == 1 ? SW_SHOW : SW_HIDE);
                    ShowWindow(hPerfomanceRAM, tabIndex == 1 ? SW_SHOW : SW_HIDE);

                    if (tabIndex == 1)
                    {
                        DrawGraph(hPerfomanceCPU, cpuUsageHistory, cpuUsageHistoryMax);
                        DrawGraph(hPerfomanceRAM, ramUsageHistory, ramUsageHistoryMax);
                    }
                }

                if (nmhdr->hwndFrom == hListViewProcesses && nmhdr->code == LVN_COLUMNCLICK)
                {
                    NMITEMACTIVATE* pnmItem = (NMITEMACTIVATE*)lParam;
                    LVCOLUMN lvColumn;
                    ZeroMemory(&lvColumn, sizeof(lvColumn));

                    static int nSortColumn = 0;
                    static BOOL bSortAscending = TRUE;

                    if (pnmItem->iSubItem == nSortColumn)
                    {
                        bSortAscending = !bSortAscending;
                    }
                    else
                    {
                        nSortColumn = pnmItem->iSubItem;
                        bSortAscending = TRUE;    
                    }
                    //SortListView(hListViewProcesses, nSortColumn, bSortAscending);
                }
            }    
            break;

        case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            //int wmEvent = HIWORD(wParam);

            switch (wmId) 
            {
                case ID_MENU_EXIT:
                {
                    SendMessage(hwnd, WM_CLOSE, wParam, lParam);    
                }
                break;
                case ID_MENU_ABOUT:
                {
                    MessageBox(hwnd, L"Task Manager App\nDeveloped by Krutko Andrey\nBSUIR 2024", L"About", MB_OK | MB_ICONINFORMATION);
                }
                break;
                case ID_MENU_UPDATE:
                {
                    PopulateProcessListView(hListViewProcesses, iconMap);
                }
                break;
                case ID_MENU_RUN_TASK: {
                    HWND hwndTaskWindow = (HWND)GetWindowLongPtr(hwnd, GWLP_USERDATA);

                    if (hwndTaskWindow == NULL) {
                        int width = static_cast<int>(SCREEN_WIDTH * 0.3);
                        int height = static_cast<int>(SCREEN_HEIGHT * 0.27);

                        RECT desktop;
                        GetWindowRect(GetDesktopWindow(), &desktop);

                        int x = (desktop.right - width) / 2;
                        int y = (desktop.bottom - height) / 2;

                        hwndTaskWindow = CreateWindowEx(
                            WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
                            TaskName, L"Создать задачу",
                            WS_VISIBLE | WS_POPUP | WS_SYSMENU | WS_CAPTION,
                            x, y, width, height,
                            hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL
                        );

                        if (hwndTaskWindow) {
                            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)hwndTaskWindow);
                        }
                    } else {
                        SetForegroundWindow(hwndTaskWindow);
                    }
                }
                break;
            
                case ID_MENU_RARE:
                    updateInterval = 5000;
                    break;
                case ID_MENU_NORMAL:
                    updateInterval = 3000;
                    break;
                case ID_MENU_OFTEN:
                    updateInterval = 1000;
                    break;
                case ID_MENU_NONE:
                    KillTimer(hwnd, TIMER_PROCESSES);
                    timerId = 0;
                    return 0;
            default: ;
            }

                if (timerId) {
                    KillTimer(hwnd, TIMER_PROCESSES);
                }
                timerId = SetTimer(hwnd, TIMER_PROCESSES, updateInterval, NULL);
        }
        break;
        
    case WM_CLOSE:
        if (MessageBox(hwnd, L"Вы действительно хотите выйти?", L"Внимание", MB_OKCANCEL) == IDOK)
        {
            DestroyWindow(hwnd);
        }
        break;

    case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            MoveWindow(hTabControl, TabControlY, TabControlY, width - 20, height - 40, TRUE);
            MoveWindow(hListViewProcesses, ProcessListViewX, ProcessListViewY, width - 40, height - 70, TRUE);

            return 0;
        }
        
    case WM_DESTROY:
    {
        if (lib) {
            FreeLibrary(lib);
        }
            
        delete[] CurrentSysProcTimes;
        delete[] PreviousSysProcTimes;
            
        KillTimer(hwnd, TIMER_PROCESSES);
        HIMAGELIST hImageList = ListView_GetImageList(hListViewProcesses, LVSIL_SMALL);
        if (hImageList) ImageList_Destroy(hImageList);
        hImageList = TabCtrl_GetImageList(hTabControl);    
        if (hImageList) ImageList_Destroy(hImageList);
        PostQuitMessage(0);
    }
    return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MainName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON));

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, MainName, MainName, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, SCREEN_WIDTH, SCREEN_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    /*LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME);
    SetWindowLong(hwnd, GWL_STYLE, style);

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);*/

    MONITORINFO mi = { };
    mi.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        SetWindowPos(hwnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    ShowWindow(hwnd, SW_SHOWMAXIMIZED);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}