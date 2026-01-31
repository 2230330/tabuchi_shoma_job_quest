#ifndef PART2_FRAMEWORK_H
#define PART2_FRAMEWORK_H

#include<windows.h>
#include<tchar.h>
#include<d3d11.h>
#include<wrl.h>

#include<sstream>
#include<memory>

#include"../headers/misc.h"
#include"../headers/high_resolution_timer.h"
#include"../headers/graphics.h"
#include"../headers/scene_constants.h"
#include"../headers/scene/scene.h"
#include"../headers/sprite.h"
#include"../headers/sprite_batch.h"

#ifdef USE_IMGUI
#include "../external/imgui/imgui.h"
#include "../external/imgui/imgui_internal.h"
#include "../external/imgui/imgui_impl_dx11.h"
#include "../external/imgui/imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];
#endif

const long kScreenWidth{ 1280 };
const long kScreenHeight{ 720 };
const bool kFullScreen{ true };
const LPCWSTR kApplicationName{ L"ecc_tabuchi" };

class Framework
{
private:
    const HWND hwnd_;
public:

    Framework(HWND hwnd);
    ~Framework();

    Framework(const Framework&) = delete;
    Framework& operator=(const Framework&) = delete;
    Framework(Framework&&)noexcept = delete;
    Framework& operator=(Framework&&)noexcept = delete;

    int Run();

    LRESULT CALLBACK HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);


    //フレームレート計算
    void CalculateFrameState();

private:
    bool Initialize();
    void Update(float elapsed_time);
    void Render(float elapsed_time);
    bool Uninitialize();

private:
    HighResolutionTimer tictoc_;
    uint32_t frames_{ 0 };
    float    accumulated_time_{ 0.0f };


private:
    std::unique_ptr<Scene>          scene_;
    float wheel{ 0 }; //マウスホイール

};

#endif //!PART2_FRAMEWORK_H