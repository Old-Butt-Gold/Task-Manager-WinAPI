#ifndef UNICODE
#define UNICODE
#endif

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Psapi.lib")

#define MainName L"Task Manager"
#define SCREEN_WIDTH  GetSystemMetrics(SM_CXSCREEN)
#define SCREEN_HEIGHT GetSystemMetrics(SM_CYSCREEN)

#define ID_SELECT_BTN 0x1010
#define ID_MENU_RUN_TASK 0x2010
#define ID_MENU_SAVE_INFO 0x2011
#define ID_MENU_EXIT 0x2012
#define ID_MENU_ABOUT 0x2013
#define IDC_TABCONTROL 0x3010
#define IDC_LISTVIEW_PROCESSES 0x4010

#define IDM_CLOSE_PROCESS 0x4011
#define IDM_OPEN_LOCATION 0x4012

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
#include <vector>
#include <algorithm>

#include "info.h"

#define ProcessListViewX 10
#define ProcessListViewY 25

#define TabControlX 10
#define TabControlY 10

HWND hProcessList;

HMENU CreateAppMenu() {
    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    
    AppendMenu(hFileMenu, MF_STRING, ID_MENU_RUN_TASK, L"��������� ����� ������");
    AppendMenu(hFileMenu, MF_STRING, ID_MENU_SAVE_INFO, L"��������� ����������");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, ID_MENU_EXIT, L"�����");

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"����");
    AppendMenu(hMenu, MF_STRING, ID_MENU_ABOUT, L"� ������������");

    return hMenu;
}

void AddTabItems(HWND hTab) {
    TCITEM tie = { 0 };
    tie.mask = TCIF_TEXT;

    tie.pszText = (LPWSTR)L"��������";
    TabCtrl_InsertItem(hTab, 0, &tie);

    tie.pszText = (LPWSTR)L"������������������";
    TabCtrl_InsertItem(hTab, 1, &tie);

    tie.pszText = (LPWSTR)L"������������";
    TabCtrl_InsertItem(hTab, 2, &tie);
}

HMENU CreateContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, IDM_CLOSE_PROCESS, L"������� �������");
    AppendMenu(hMenu, MF_STRING, IDM_OPEN_LOCATION, L"������� ������������ �����");
    return hMenu;
}

void HandleContextMenu(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreateContextMenu();
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

HWND CreateProcessListView(HWND hParent) {
    HWND hListView = CreateWindowEx(0, WC_LISTVIEW, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        ProcessListViewX, ProcessListViewY, SCREEN_WIDTH - 40, SCREEN_HEIGHT - 100,
        hParent, (HMENU)IDC_LISTVIEW_PROCESSES, (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE), NULL);

    LVCOLUMN lvColumn;
    
    HIMAGELIST hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);
    ImageList_SetBkColor(hImageList, CLR_NONE);
    
    HICON hIcon = LoadIcon((HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE), MAKEINTRESOURCE(IDI_MAINICON));
    ImageList_AddIcon(hImageList, LoadIcon(NULL, IDI_WINLOGO));
    ImageList_AddIcon(hImageList, hIcon);
    
    DestroyIcon(hIcon);
    ListView_SetImageList(hListView, hImageList, LVSIL_SMALL);
    ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT| LVS_EX_HEADERDRAGDROP);

    const std::wstring columnHeaders[] = {
        L"��� ��������", L"ID ��������", L"ID ������������� ��������", L"���������",
        L"�������� ��", L"�������� ������", L"����� ��������", L"���� � �����", L"��������"
    };
    int columnWidths[] = { 200, 150, 150, 150, 150, 190, 180, 240, 80 };

    for (int i = 0; i < 9; i++) {
        lvColumn.mask = (i == 0) 
                        ? LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT | LVCF_IMAGE
                        : LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
        
        lvColumn.fmt = LVCFMT_CENTER;
        lvColumn.cchTextMax = MAX_PATH;
        lvColumn.pszText = (LPWSTR)columnHeaders[i].c_str();
        lvColumn.cx = columnWidths[i];
        lvColumn.iImage = 1;
        lvColumn.iSubItem = i;
        
        ListView_InsertColumn(hListView, i, &lvColumn);
    }

    return hListView;
}

WNDPROC oldListViewProcessesProc;

void CloseProcessById(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (hProcess) {
        if (TerminateProcess(hProcess, 0)) {
            MessageBox(NULL, L"������� ������� ��������.", L"�����", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBox(NULL, L"�� ������� ��������� �������.", L"������", MB_OK | MB_ICONERROR);
        }
        CloseHandle(hProcess);
    } else {
        MessageBox(NULL, L"�� ������� ������� ������� ��� ����������.", L"������", MB_OK | MB_ICONERROR);
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
            MessageBox(NULL, L"�� ������� �������� ���� ��������.", L"������", MB_OK | MB_ICONERROR);
        }

        CloseHandle(hProcess);
    } else {
        MessageBox(NULL, L"�� ������� ������� ������� ��� ��������� ����.", L"������", MB_OK | MB_ICONERROR);
    }
}

LRESULT CALLBACK ListViewProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static int selectedItemIndex;
    switch (uMsg)
    {
        case WM_COMMAND:
            {
                int wmId = LOWORD(wParam);
                int wmEvent = HIWORD(wParam);

                switch (wmId) 
                {
                    case IDM_CLOSE_PROCESS:
                    {
                       wchar_t buffer[32];
                       ListView_GetItemText(hwnd, selectedItemIndex, 1, buffer, 32);
                       DWORD processId = _wtoi(buffer);
                       CloseProcessById(processId);
                    }
                    break;
                    
                    case IDM_OPEN_LOCATION:
                    {
                       wchar_t buffer[32];
                       ListView_GetItemText(hwnd, selectedItemIndex, 1, buffer, 32);
                       DWORD processId = _wtoi(buffer);
                       OpenProcessLocation(processId);
                    }
                    break;
                }
            }
            break;
        
            case WM_CONTEXTMENU: {
                if ((HWND)wParam == hwnd)
                {
                    LVHITTESTINFO hitTestInfo = { 0 };
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
                
                break;
            }

        default:
            return CallWindowProc(oldListViewProcessesProc, hwnd, uMsg, wParam, lParam);
    }
    return CallWindowProc(oldListViewProcessesProc, hwnd, uMsg, wParam, lParam);
}

std::wstring GetPriority(DWORD processId)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess)
    {
        DWORD priorityClass = GetPriorityClass(hProcess);
        if (priorityClass == 0) {
            CloseHandle(hProcess);
            return L"����������";
        }

        std::wstring priorityString;
        switch (priorityClass) {
        case IDLE_PRIORITY_CLASS: priorityString = L"������"; break;
        case BELOW_NORMAL_PRIORITY_CLASS: priorityString = L"���� �����������"; break;
        case NORMAL_PRIORITY_CLASS: priorityString = L"����������"; break;
        case ABOVE_NORMAL_PRIORITY_CLASS: priorityString = L"���� �����������"; break;
        case HIGH_PRIORITY_CLASS: priorityString = L"�������"; break;
        case REALTIME_PRIORITY_CLASS: priorityString = L"��������� �������"; break;
        default: priorityString = L"����������"; break;
        }
        
        CloseHandle(hProcess);
        return priorityString;
    }
    return L"��� �������";
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
            swprintf_s(timeString, 100, L"%02d/%02d/%04d %02d:%02d:%02d",
            st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
            std::wstring result(timeString);
            return result;
        }
        CloseHandle(hProcess);
    }
    return L"";
}

void PopulateProcessListView(HWND hListView, std::map<std::wstring, int>& iconMap)
{
    ListView_DeleteAllItems(hListView);

    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE)
    {
        return;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hProcessSnap, &pe32)) {
        do {
            LVITEM lvi = {0};
            lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
            lvi.pszText = (LPWSTR)pe32.szExeFile;
            lvi.iImage = 0;
            lvi.cchTextMax = MAX_PATH;
            lvi.lParam = ListView_GetItemCount(hListView);
            ChangeIcon(pe32, hListView, lvi, iconMap);
            int itemIndex = ListView_InsertItem(hListView, &lvi);

            ListView_SetItemText(hListView, itemIndex, 1, (LPWSTR)std::to_wstring(pe32.th32ProcessID).c_str()); // ID
            std::wstring parent = pe32.th32ParentProcessID == 0 ? std::to_wstring(-1) : std::to_wstring(pe32.th32ParentProcessID);
            ListView_SetItemText(hListView, itemIndex, 2, (LPWSTR)parent.c_str())
            ListView_SetItemText(hListView, itemIndex, 3, (LPWSTR)GetPriority(pe32.th32ProcessID).c_str());
            ListView_SetItemText(hListView, itemIndex, 6, (LPWSTR)GetProcessTimeCreation(pe32.th32ProcessID).c_str());
            ListView_SetItemText(hListView, itemIndex, 8, Is64BitProcess(pe32.th32ProcessID) ? (LPWSTR)L"64-���" : (LPWSTR)L"32-���");

            TCHAR fullPath[MAX_PATH];
            DWORD processNameLength = sizeof(fullPath) / sizeof(TCHAR);
            if (QueryFullProcessImageName(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID), 0, fullPath, &processNameLength)) {
                ListView_SetItemText(hListView, itemIndex, 7, (LPWSTR)fullPath);
            }
            

        } while (Process32Next(hProcessSnap, &pe32));
    }

    CloseHandle(hProcessSnap);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hTabControl, hListViewProcesses;
    static std::map<std::wstring, int> iconMap;
    
    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
                
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            EndPaint(hwnd, &ps);
        }
        return 0;

        case WM_CREATE:
        {
            hTabControl = CreateWindowEx(0, WC_TABCONTROL, NULL,
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                TabControlX, TabControlY, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 70,
                hwnd, (HMENU)IDC_TABCONTROL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            AddTabItems(hTabControl);
            SetMenu(hwnd, CreateAppMenu());

            hListViewProcesses = CreateProcessListView(hTabControl);
            hProcessList = hListViewProcesses;    
            PopulateProcessListView(hListViewProcesses, iconMap);
            oldListViewProcessesProc = (WNDPROC)SetWindowLongPtr(hListViewProcesses, GWLP_WNDPROC, (LONG_PTR)ListViewProc);    
        }
        break;

        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == ID_SELECT_BTN)
            {

            }
            break;
        }

        case WM_KEYDOWN:
        {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            /*if (wParam == VK_RETURN) {
                
            }*/
        }
        break;

        case WM_NOTIFY:
            {
                NMHDR* nmhdr = (NMHDR*)lParam;
                if (nmhdr->hwndFrom == hTabControl && nmhdr->code == TCN_SELCHANGE) {
                    int tabIndex = TabCtrl_GetCurSel(hTabControl);
                    ShowWindow(hListViewProcesses, tabIndex == 0 ? SW_SHOW : SW_HIDE);

                    if (tabIndex == 0)
                    {
                        PopulateProcessListView(hListViewProcesses, iconMap);
                    }
                }

                if (nmhdr->hwndFrom == hListViewProcesses && nmhdr->code == LVN_COLUMNCLICK)
                {
                    NMITEMACTIVATE* pnmItem = (NMITEMACTIVATE*)lParam;
                    LVCOLUMN lvColumn;
                    ZeroMemory(&lvColumn, sizeof(lvColumn));

                    static int nSortColumn = 0;
                    static BOOL bSortAscending = TRUE;
                    LPARAM lParamSort;

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
            int wmEvent = HIWORD(wParam);

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
            }
        }
        break;

        
    case WM_CLOSE:
        if (MessageBox(hwnd, L"Really quit?", L"My application", MB_OKCANCEL) == IDOK)
        {
            DestroyWindow(hwnd);
        }
        break;

    case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            MoveWindow(hTabControl, TabControlY, TabControlY, width - 20, height - 70, TRUE);
            MoveWindow(hListViewProcesses, ProcessListViewX, ProcessListViewY, width - 40, height - 100, TRUE);

            return 0;
        }
        
    case WM_DESTROY:
    {
        HIMAGELIST hImageList = ListView_GetImageList(hListViewProcesses, LVSIL_SMALL);
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
    WNDCLASS wc = { 0 };
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

    MONITORINFO mi = { sizeof(mi) };
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