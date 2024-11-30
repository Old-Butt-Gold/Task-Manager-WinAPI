#ifndef UNICODE
#define UNICODE
#endif

#include "resource.h"
#include "ProcessInfo.h"
#include "Graph.h"
#include <shlobj.h>

typedef enum _PreferredAppMode
{
    Default,
    AllowDark,
    ForceDark,
    ForceLight,
    Max
} PreferredAppMode;

typedef PreferredAppMode(WINAPI* fnSetPreferredAppMode)(PreferredAppMode appMode);

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

void AddTabItems(HWND hTab) {
    TCITEM tie = { };
    tie.mask = TCIF_TEXT;

    tie.pszText = (LPWSTR)L"Процессы";
    TabCtrl_InsertItem(hTab, 0, &tie);

    tie.pszText = (LPWSTR)L"Производительность";
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

HWND CreateProcessListView(HWND hParent) {
    HWND hListView = CreateWindowEx(0, WC_LISTVIEW, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        ProcessListViewX, ProcessListViewY, SCREEN_WIDTH - 30, SCREEN_HEIGHT - 70,
        hParent, (HMENU)IDC_LISTVIEW_PROCESSES, (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE), NULL);

    LVCOLUMN lvColumn;
    
    HIMAGELIST hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);
    ImageList_SetBkColor(hImageList, CLR_NONE);
    
    ImageList_AddIcon(hImageList, LoadIcon(NULL, IDI_WINLOGO));

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
        lvColumn.iSubItem = i;
        
        ListView_InsertColumn(hListView, i, &lvColumn);
    }

    return hListView;
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

        case WM_KEYDOWN:
        {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);
            if (wmId == VK_F1) {
                SendMessage(hwnd, WM_COMMAND, ID_MENU_ABOUT, 0);
            }
            
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
                    MessageBox(hwnd, L"Приложение Диспетчер задач\nРазработчик: Крутько Андрей\nБГУИР 2024", 
                               L"О программе", MB_OK | MB_ICONINFORMATION);
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
        DestroyWindow(hwnd);
        break;

    case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            MoveWindow(hTabControl, TabControlY, TabControlY, width - 20, height - 53, TRUE);
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