#include"../../headers/system/render_cloud_dx12_system.h"

#include<iostream>

#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
#include"../../headers/misc.h"
#include"../../headers/root_signature_manager.h"

RenderCloudDX12System::RenderCloudDX12System()
	:IRenderDX12System(RenderPass_Background)
{
	//DX12deviceの取得
	ID3D12Device* device = Graphics::Instance().GetDX12Device();
	HRESULT hr{ S_OK };


	//UpLoadBufferの生成
	{
		D3D12_HEAP_PROPERTIES heap_properties{};
		heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heap_properties.CreationNodeMask = 1;
		heap_properties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC buffer_desc{};
		buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffer_desc.Width =//256バイト境界にそろえる
			(static_cast<UINT64>(sizeof(CloudRayMarchingConstants))+255)&~static_cast<UINT64>(255);
		buffer_desc.Height = 1;
		buffer_desc.DepthOrArraySize = 1;
		buffer_desc.MipLevels = 1;
		buffer_desc.SampleDesc.Count = 1;
		buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		//
		hr = device->CreateCommittedResource(
			&heap_properties,
			D3D12_HEAP_FLAG_NONE,
			&buffer_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(cloud_raymarching_constant_buffer_.GetAddressOf())
		);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		//マップで情報更新できるようにする
		D3D12_RANGE read_range{ 0,0 };
		void* mapped_data = nullptr;

		hr = cloud_raymarching_constant_buffer_->Map(0, &read_range, &mapped_data);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		mapped_cloud_raymarching_constants_ =
			static_cast<CloudRayMarchingConstants*>(mapped_data);

		*mapped_cloud_raymarching_constants_ = CloudRayMarchingConstants{};

	}

	//シェーダーの設定
	{
		auto vs_blob = ResourceManager::Instance().LoadShaderBytecode(L"resources/shader/render_back_vs.dxil");
		auto ps_blob = ResourceManager::Instance().LoadShaderBytecode(L"resources/shader/volumetric_cloud_dx12_ps.dxil");

		D3D12_SHADER_BYTECODE vs_bytecode{
			vs_blob->GetBufferPointer(),
			vs_blob->GetBufferSize()
		};
		D3D12_SHADER_BYTECODE ps_bytecode{
			ps_blob->GetBufferPointer(),
			ps_blob->GetBufferSize()
		};

		//パイプラインの生成
		{
			pipeline_state_ = Graphics::Instance().CreatePipelineState(
				RootSignatureManager::Instance().GetRootSignature3D().Get(),
				vs_bytecode,
				ps_bytecode,
				PipelineKind::Background
			);
		}
	}

	//画像データの読み込み
	{
		ID3D12CommandQueue* queue = Graphics::Instance().GetDX12CommandQueue();

		//雲分布データ
		const wchar_t* weather_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\weather.bmp";
		weather_map_srv_ = ResourceManager::Instance().LoadTextureDX12(device, queue, weather_noise_tex_path);
	}
}

RenderCloudDX12System::~RenderCloudDX12System()
{
	if (mapped_cloud_raymarching_constants_!=nullptr)
	{
		const D3D12_RANGE read_range{ 0, sizeof(CloudRayMarchingConstants) };

		cloud_raymarching_constant_buffer_->Unmap(0, &read_range);
		mapped_cloud_raymarching_constants_ = nullptr;
	}
}

void RenderCloudDX12System::RenderDX12(ID3D12GraphicsCommandList* cmd_list)
{
	//ルートシグネチャの設定はRenderSystemManager側で行う
	//ルートシグネチャを複数作らないため

	//定数バッファビューの設定
	cmd_list->SetGraphicsRootConstantBufferView(
		RootSignatureSlot::kDx12PassConstants,
		cloud_raymarching_constant_buffer_->GetGPUVirtualAddress()
	);


	//パイプラインステートの設定
	cmd_list->SetPipelineState(pipeline_state_.Get());

	//トポロジーの設定
	cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//描画、背景のみ
	cmd_list->DrawInstanced(3, 1, 0, 0);

}

void RenderCloudDX12System::GetSkyTexture(ID3D12Resource* sky_texture)
{
	this->sky_texture_ = sky_texture;
}
