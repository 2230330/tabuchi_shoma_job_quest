#include<time.h>
#define _CRTDBG_MAP_ALLOC
#include<crtdbg.h>

#include"../headers/framework.h"

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    Framework* p{ reinterpret_cast<Framework*>(GetWindowLongPtr(hwnd,GWLP_USERDATA)) };
    return p ? p->HandleMessage(hwnd, msg, wparam, lparam) : DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance, _In_ LPSTR cmd_line, _In_ int cmd_show)
{
#if defined(DEBUG)|defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
    //ÉÅÉÇÉäÉäÅ[ÉNåüçı
    //_CrtSetBreakAlloc(321173);

#endif

    srand(static_cast<unsigned int>(time(nullptr)));

    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProcedure;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = instance;
    wcex.hIcon = 0;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName=NULL;
    wcex.lpszClassName = kApplicationName;
    wcex.hIconSm = 0;
    RegisterClassExW(&wcex);

    RECT rc{ 0,0,kScreenWidth,kScreenHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(0, kApplicationName, L"", WS_OVERLAPPEDWINDOW ^ WS_MAXIMIZEBOX ^ WS_THICKFRAME | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, instance, NULL);
    ShowWindow(hwnd, cmd_show);

    Framework framework(hwnd);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&framework));


    return framework.Run();
}