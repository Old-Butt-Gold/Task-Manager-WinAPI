#ifndef UNICODE
#define UNICODE
#endif 

#define MainName L"Task Manager"
#define SCREEN_WIDTH  GetSystemMetrics(SM_CXSCREEN)
#define SCREEN_HEIGHT GetSystemMetrics(SM_CYSCREEN)

#define ID_SELECT_BTN 0x1010
#define ID_MENU_RUN_TASK 0x2010
#define ID_MENU_SAVE_INFO 0x2011
#define ID_MENU_EXIT 0x2012
#define ID_MENU_ABOUT 0x2013
#define IDC_TABCONTROL 0x3010

#include <Windows.h>
#include <iomanip>
#include <sstream>
#include <CommCtrl.h>
#include <windowsx.h>
#include <wingdi.h>
#include <string>
#include "resource.h"

HMENU CreateAppMenu() {
    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    
    AppendMenu(hFileMenu, MF_STRING, ID_MENU_RUN_TASK, L"Запустить новую задачу");
    AppendMenu(hFileMenu, MF_STRING, ID_MENU_SAVE_INFO, L"Сохранить информацию");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, ID_MENU_EXIT, L"Выход");

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"Файл");
    AppendMenu(hMenu, MF_STRING, ID_MENU_ABOUT, L"О разработчике");

    return hMenu;
}

void AddTabItems(HWND hTab) {
    TCITEM tie = { 0 };
    tie.mask = TCIF_TEXT;

    tie.pszText = (LPWSTR)L"Процессы";
    TabCtrl_InsertItem(hTab, 0, &tie);

    tie.pszText = (LPWSTR)L"Производительность";
    TabCtrl_InsertItem(hTab, 1, &tie);

    tie.pszText = (LPWSTR)L"Автозагрузка";
    TabCtrl_InsertItem(hTab, 2, &tie);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hFontButton, hTabControl;

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
            hFontButton = CreateWindowEx(0, L"BUTTON", L"Font Settings",
            WS_CHILD | WS_VISIBLE | BS_PUSHBOX | BS_OWNERDRAW,
            1, SCREEN_HEIGHT - 120, 200, 25, hwnd, (HMENU)ID_SELECT_BTN, NULL, NULL);

            hTabControl = CreateWindowEx(0, WC_TABCONTROL, NULL,
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                10, 10, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 150,
                hwnd, (HMENU)IDC_TABCONTROL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            AddTabItems(hTabControl);
            SetMenu(hwnd, CreateAppMenu());
        }
        break;

        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == ID_SELECT_BTN) {
                HDC hdc = lpDrawItem->hDC;

                HBRUSH hBrush = CreateSolidBrush(RGB(30, 144, 255));
                FillRect(hdc, &lpDrawItem->rcItem, hBrush);
                DeleteObject(hBrush);

                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);

                RECT textRect = lpDrawItem->rcItem;
                DrawText(hdc, L"Font Settings", -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                if (lpDrawItem->itemState & ODS_SELECTED) {
                    FrameRect(hdc, &lpDrawItem->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
                }

                return TRUE;
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
            }
        }
        break;

    case WM_CLOSE:
        if (MessageBox(hwnd, L"Really quit?", L"My application", MB_OKCANCEL) == IDOK)
        {
            DestroyWindow(hwnd);
        }
        break;

    case WM_DESTROY:
    {
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