#include <windows.h>
#include <tchar.h>
#include <time.h>
#include <stdio.h>

#define WINDOW_CLASS_NAME           L"TimeDisplayUIClass"
#define TIMER_ID 1
#define WINDOW_WIDTH 200
#define WINDOW_HEIGHT 60

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void UpdateTimeDisplay(HWND hwnd);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc = {0};
    HWND hwnd;
    MSG msg;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIconSm = LoadIconW(NULL, IDI_APPLICATION);

    if (!RegisterClassExW(&wc))
    {
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int xPos = 2;
    int yPos = screenHeight - WINDOW_HEIGHT - 50;

    hwnd = CreateWindowExW( WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, WINDOW_CLASS_NAME, L"Time Display", WS_POPUP, xPos, yPos, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, hInstance, NULL );

    if (hwnd == NULL)
    {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    SetLayeredWindowAttributes(hwnd, 0, 233, LWA_ALPHA);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    SetTimer(hwnd, TIMER_ID, 1000, NULL);

    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        UpdateTimeDisplay(hwnd);
        break;

    case WM_TIMER:
        if (wParam == TIMER_ID)
        {
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rect;
            GetClientRect(hwnd, &rect);

            HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);

            time_t now = time(NULL);
            struct tm timeInfo;
            localtime_s(&timeInfo, &now);

            TCHAR timeStr[64];
            _stprintf_s(timeStr, 64, L"%04d-%02d-%02d %02d:%02d:%02d",
                timeInfo.tm_year + 1900,
                timeInfo.tm_mon + 1,
                timeInfo.tm_mday,
                timeInfo.tm_hour,
                timeInfo.tm_min,
                timeInfo.tm_sec );

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0, 255, 0));

            HFONT hFont = CreateFontW( 18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");

            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            DrawTextW(hdc, timeStr, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);

            EndPaint(hwnd, &ps);
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        PostQuitMessage(0);
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void UpdateTimeDisplay(HWND hwnd)
{
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}
