#include "ProcessInfo.h"

void CloseProcessById(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (hProcess) {
        TerminateProcess(hProcess, 0) ? MessageBox(NULL, L"Процесс успешно завершён.", L"Успех", MB_OK | MB_ICONINFORMATION)
                                               : MessageBox(NULL, L"Не удалось завершить процесс.", L"Ошибка", MB_OK | MB_ICONERROR);
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
            PathRemoveFileSpec(fullPath);
            ShellExecute(NULL, L"open", fullPath, NULL, NULL, SW_SHOWNORMAL);
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
    return L"—";
}

void ChangeIcon(DWORD processID, HWND hListView, LVITEM& lvi, std::map<std::wstring, int>& iconMap)
{
    TCHAR fullPath[MAX_PATH];
    DWORD processNameLength = sizeof(fullPath) / sizeof(TCHAR);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
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
            lvi.cchTextMax = MAX_PATH;
            lvi.lParam = processID;
            
            ChangeIcon(pe32.th32ProcessID, hListView, lvi, iconMap);

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
    
    si.nPos = scrollPos;
    SetScrollInfo(hListView, SB_VERT, &si, TRUE);

    SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
}
