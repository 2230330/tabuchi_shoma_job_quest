#include"../../headers/system/render_golden_ratio_system.h"

#include"../../headers/component/component_manager.h"
#include"../../headers/fullscreen_quad.h"
#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
#include"../../headers/misc.h"
#include"../../headers/constant_buffer_slot.h"

RenderGoldenRatioSystem::RenderGoldenRatioSystem(ComponentManager& comp_mng, RenderPass render_pass)
    :comp_mng_(comp_mng)
    , IRenderSystem(render_pass)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
    //バッファ生成
    {
        HRESULT hr{ S_OK };

        //定数バッファ生成
        {
            D3D11_BUFFER_DESC cb_desc{};
            cb_desc.Usage = D3D11_USAGE_DEFAULT;
            cb_desc.ByteWidth = (sizeof(GoldenRatioConstants) + 15) / 16 * 16;
            cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            hr = device->CreateBuffer(&cb_desc, nullptr, golden_ratio_constant_buffer_.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }
    }

    golden_ratio_ps_= ResourceManager::Instance().LoadPixelShader(Graphics::Instance().GetDevice(),
        L".\\resources\\shader\\golden_spiral_ps.cso");

    fullscreen_quad_ = std::make_unique<FullscreenQuad>(Graphics::Instance().GetDevice());  
}

void RenderGoldenRatioSystem::Render()
{
    comp_mng_.ForEach<ComponentGoldenSpiral>([&](uint32_t entity_id, ComponentGoldenSpiral& golden_spiral)
        {
            golden_ratio_constant_.orientation = golden_spiral.orientation;

            //コンスタントバッファの更新
            Graphics::Instance().GetDeviceContext()
                ->UpdateSubresource(golden_ratio_constant_buffer_.Get(), 0, nullptr, &golden_ratio_constant_, 0, 0);
            Graphics::Instance().SetConstantBuffer(ConstantBufferSlot::kPostEffect, 1, golden_ratio_constant_buffer_.GetAddressOf());
        
        
            fullscreen_quad_->blit(Graphics::Instance().GetDeviceContext(), nullptr, 0, 0, golden_ratio_ps_.Get());
        });
}
