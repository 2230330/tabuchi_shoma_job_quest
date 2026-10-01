#pragma once
#include"i_render_dx12_system.h"

//ゲームの背景を描画するシステム
//DX12用に作成しているので、そのうちDX11 の背景描画と統合するかも
class RenderBackSystem :public IRenderDX12System
{
public:
	RenderBackSystem();
	~RenderBackSystem()override;

	void RenderDX12(ID3D12GraphicsCommandList* cmd_list) override;//DX12用の描画関数
};