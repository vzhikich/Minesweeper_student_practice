// WindowsMinesweeperMenu.cpp
#include "framework.h"
#include "resource.h"
#include <string>
#include <windows.h>

#define ID_BUTTON_START   101
#define ID_BUTTON_EXIT    102
#define ID_BUTTON_EASY    201
#define ID_BUTTON_MEDIUM  202
#define ID_BUTTON_HARD    203
#define ID_BUTTON_BACK    204

HINSTANCE hInst;               // Глобальна змінна для instance
HWND hMainWnd, hDifficultyWnd; // Вікна меню
HANDLE hSerial = INVALID_HANDLE_VALUE; // COM порт

// Функції
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DifficultyWndProc(HWND, UINT, WPARAM, LPARAM);
bool InitSerial(const wchar_t* portName);
void SendDifficultyCommand(const std::string& cmd);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    hInst = hInstance;

    // --- Головне меню ---
    WNDCLASSW wc = {};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MAIN_MENU_CLASS";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT3));
    RegisterClassW(&wc);

    hMainWnd = CreateWindowW(L"MAIN_MENU_CLASS", L"Main Menu",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, 0, 400, 300,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    // --- Меню вибору складності ---
    WNDCLASSW wc2 = {};
    wc2.lpfnWndProc = DifficultyWndProc;
    wc2.hInstance = hInstance;
    wc2.lpszClassName = L"DIFFICULTY_MENU_CLASS";
    wc2.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc2.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT3));
    RegisterClassW(&wc2);

    hDifficultyWnd = CreateWindowW(L"DIFFICULTY_MENU_CLASS", L"Choose Difficulty",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, 0, 400, 300,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hDifficultyWnd, SW_HIDE);

    // --- COM порт ---
    if (!InitSerial(L"COM5")) // <- зміни на свій COM порт
    {
        MessageBoxW(nullptr, L"Failed to open serial port!", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    // --- Цикл повідомлень ---
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hSerial != INVALID_HANDLE_VALUE)
        CloseHandle(hSerial);

    return (int)msg.wParam;
}

// =========================
// --- Головне меню ---
// =========================
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hBtnStart, hBtnExit;

    switch (message)
    {
    case WM_CREATE:
        hBtnStart = CreateWindowW(L"BUTTON", L"Start",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            140, 80, 120, 40,
            hWnd, (HMENU)ID_BUTTON_START, hInst, nullptr);

        hBtnExit = CreateWindowW(L"BUTTON", L"Exit",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            140, 140, 120, 40,
            hWnd, (HMENU)ID_BUTTON_EXIT, hInst, nullptr);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_BUTTON_START:
            ShowWindow(hWnd, SW_HIDE);
            ShowWindow(hDifficultyWnd, SW_SHOW);
            break;
        case ID_BUTTON_EXIT:
            DestroyWindow(hWnd);
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// =========================
// --- Меню складності ---
// =========================
LRESULT CALLBACK DifficultyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hBtnEasy, hBtnMedium, hBtnHard, hBtnBack;

    switch (message)
    {
    case WM_CREATE:
        hBtnEasy = CreateWindowW(L"BUTTON", L"Easy",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            140, 40, 120, 40, hWnd, (HMENU)ID_BUTTON_EASY, hInst, nullptr);

        hBtnMedium = CreateWindowW(L"BUTTON", L"Medium",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            140, 90, 120, 40, hWnd, (HMENU)ID_BUTTON_MEDIUM, hInst, nullptr);

        hBtnHard = CreateWindowW(L"BUTTON", L"Hard",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            140, 140, 120, 40, hWnd, (HMENU)ID_BUTTON_HARD, hInst, nullptr);

        hBtnBack = CreateWindowW(L"BUTTON", L"Back",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            140, 190, 120, 40, hWnd, (HMENU)ID_BUTTON_BACK, hInst, nullptr);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_BUTTON_EASY:
            SendDifficultyCommand("MINE EASY");
            break;
        case ID_BUTTON_MEDIUM:
            SendDifficultyCommand("MINE MEDIUM");
            break;
        case ID_BUTTON_HARD:
            SendDifficultyCommand("MINE HARD");
            break;
        case ID_BUTTON_BACK:
            ShowWindow(hWnd, SW_HIDE);
            ShowWindow(hMainWnd, SW_SHOW);
            break;
        }
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// =========================
// --- COM порт ---
// =========================
bool InitSerial(const wchar_t* portName)
{
    hSerial = CreateFileW(portName,
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr,
        OPEN_EXISTING,
        0, nullptr);

    if (hSerial == INVALID_HANDLE_VALUE) return false;

    DCB dcbSerial = { 0 };
    dcbSerial.DCBlength = sizeof(dcbSerial);
    if (!GetCommState(hSerial, &dcbSerial)) return false;

    dcbSerial.BaudRate = CBR_115200;
    dcbSerial.ByteSize = 8;
    dcbSerial.StopBits = ONESTOPBIT;
    dcbSerial.Parity = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerial)) return false;

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = MAXDWORD; // чекаємо на байт
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(hSerial, &timeouts)) return false;

    return true;
}

// Відправка команди на STM32
void SendDifficultyCommand(const std::string& cmd)
{
    if (hSerial == INVALID_HANDLE_VALUE) return;

    std::string tx = cmd + "\r\n"; // завершення рядка
    DWORD written;
    WriteFile(hSerial, tx.c_str(), (DWORD)tx.size(), &written, nullptr);
}
