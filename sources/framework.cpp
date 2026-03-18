#include"../headers/framework.h"

#include"../headers/graphics.h"
#include"../headers/scene/scene_test.h"


Framework::Framework(HWND hwnd):hwnd_(hwnd)
{

}

bool Framework::Initialize()
{
    Graphics::Instance().Initialize(hwnd_);

    this->scene_ = std::make_unique<SceneTest>(hwnd_);

    if (scene_)return scene_->Initialize();
    return true;
}

void Framework::Update(float elapsed_time)
{
#ifdef USE_IMGUI
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
#endif
    
    //シーン更新処理
    Graphics::Instance().SetWheel(wheel);
    wheel = 0;
    scene_->Update(elapsed_time);

#ifdef USE_IMGUI

#endif
}

void Framework::Render(float elapsed_time)
{
    ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
    ID3D11RenderTargetView* null_render_target_views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    dc->OMSetRenderTargets(_countof(null_render_target_views), null_render_target_views, 0);
    ID3D11ShaderResourceView* null_shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
    dc->VSSetShaderResources(0, _countof(null_shader_resource_views), null_shader_resource_views);
    dc->PSSetShaderResources(0, _countof(null_shader_resource_views), null_shader_resource_views);

    //シーン描画処理
    {
        scene_->Render(elapsed_time);
    }



#ifdef USE_IMGUI
    scene_->DrawGui();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif

    UINT sync_interval{ 0 };
    Graphics::Instance().Present(sync_interval);

}

bool Framework::Uninitialize()
{
    if (scene_)
    {
        bool r = scene_->Uninitialize();
        Graphics::Instance().Shutdown();
    }

    return true;
}

Framework::~Framework()
{
}

void Framework::CalculateFrameState()
{
    float dt = tictoc_.TimeInterval();
    accumulated_time_ += dt;
    frames_++;

    if (accumulated_time_>=1.0f)
    {
        float fps = static_cast<float>(frames_)/accumulated_time_;
        float avg_frame_time_ms = 1000.0f / fps;

        std::wostringstream outs;
        outs.precision(6);
        outs << kApplicationName << L":FPS:" << fps << L"/" 
            << L"FrameTime:" << avg_frame_time_ms << L"(ms)";
        SetWindowTextW(hwnd_, outs.str().c_str());

        frames_ = 0;
        accumulated_time_ = 0.0f;
    }
}

int Framework::Run()
{
    MSG msg{};

    if (!Initialize())
    {
        return 0;
    }

#ifdef USE_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f, nullptr, glyphRangesJapanese);
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());
    ImGui::StyleColorsDark();
#endif

    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            tictoc_.Tick();
            CalculateFrameState();
            Update(tictoc_.TimeInterval());
            Render(tictoc_.TimeInterval());

        }
    }

#ifdef USE_IMGUI
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif

#if 1
    //BOOL fullscreen = 0;
    //swap_chain_.Get()->GetFullscreenState(&fullscreen, 0);
    //if (fullscreen)
    //{
    //    swap_chain_.Get()->SetFullscreenState(FALSE, 0);
    //}
#endif


    return Uninitialize() ? static_cast<int>(msg.wParam) : 0;
}

LRESULT CALLBACK Framework::HandleMessage(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{

    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);

        EndPaint(hwnd, &ps);
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CREATE:
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        break;
    case WM_ENTERSIZEMOVE:
        tictoc_.Stop();
        break;
    case WM_EXITSIZEMOVE:
        tictoc_.Start();
        break;
    case WM_MOUSEWHEEL:
        //マウスホイールの動きを捉えるよう
        wheel = GET_WHEEL_DELTA_WPARAM(wParam);
    default:
        /*タイトルバーが一文字しか表示されない問題でWに変更しました*/
        /*return DefWindowProc(hwnd, msg, wparam, lparam); */
        
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
            return true;

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}