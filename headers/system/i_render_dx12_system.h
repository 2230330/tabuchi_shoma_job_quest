#pragma once

#include<d3d12.h>
#include<wrl.h>

#include"render_pass.h"

//描画システムのインターフェース
class IRenderDX12System
{
public:
    explicit IRenderDX12System(RenderPass pass)
        : pass_(pass)
    {
    }

    virtual ~IRenderDX12System() = default;
    RenderPass GetPass() const { return pass_; }
    //DX12用の描画関数
    virtual void RenderDX12(ID3D12GraphicsCommandList* cmd_list) = 0;

protected:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;

private:
    RenderPass pass_;
};