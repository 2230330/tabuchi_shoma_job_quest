#include"../../headers/system/render_sky_dx12_system.h"

#include"../../headers/graphics.h"
#include"../../headers/misc.h"

RenderSkyDX12System::RenderSkyDX12System()
	:IRenderDX12System(RenderPass_Background)
{

	// ルートシグネチャの作成
	{
		D3D12_ROOT_PARAMETER cbv_parameter{};
		cbv_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		cbv_parameter.Descriptor.ShaderRegister = 11; // シェーダー側の登録番号
		cbv_parameter.Descriptor.RegisterSpace = 0;
		cbv_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC root_signature_desc{};
		root_signature_desc.NumParameters = 1;
		root_signature_desc.pParameters = &cbv_parameter;
		root_signature_desc.NumStaticSamplers = 0;
		root_signature_desc.pStaticSamplers = nullptr;
		root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		Microsoft::WRL::ComPtr<ID3DBlob> serialized_signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

		HRESULT hr = D3D12SerializeRootSignature(
			&root_signature_desc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			serialized_signature.GetAddressOf(),
			error_blob.GetAddressOf()
		);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		ID3D12Device* device = Graphics::Instance().GetDX12Device();

		hr = device->CreateRootSignature(
			0,
			serialized_signature->GetBufferPointer(),
			serialized_signature->GetBufferSize(),
			IID_PPV_ARGS(root_signature_.GetAddressOf())
		);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}
	//upload bufferの作成
	{
		ID3D12Device* device = Graphics::Instance().GetDX12Device();

		D3D12_HEAP_PROPERTIES heap_properties{};
		heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heap_properties.CreationNodeMask = 1;
		heap_properties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC buffer_desc{};
		buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffer_desc.Width =// 256バイト境界に揃える
			(static_cast<UINT64>(sizeof(SkyAtmosphereConstants)) + 255) & ~UINT64(255);
		buffer_desc.Height = 1;
		buffer_desc.DepthOrArraySize = 1;
		buffer_desc.MipLevels = 1;
		buffer_desc.SampleDesc.Count = 1;
		buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		HRESULT hr = device->CreateCommittedResource(
			&heap_properties,
			D3D12_HEAP_FLAG_NONE,
			&buffer_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(atmosphere_upload_buffer_.GetAddressOf())
		);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		D3D12_RANGE read_range{ 0, 0 };
		void* mapped_data = nullptr;

		hr = atmosphere_upload_buffer_->Map(0, &read_range, &mapped_data);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		mapped_atmosphere_constants_ =
			static_cast<SkyAtmosphereConstants*>(mapped_data);

		*mapped_atmosphere_constants_ = SkyAtmosphereConstants{};
	}
}

RenderSkyDX12System::~RenderSkyDX12System()
{
	if (mapped_atmosphere_constants_ != nullptr)
	{
		const D3D12_RANGE read_range{ 0, sizeof(SkyAtmosphereConstants) };

		atmosphere_upload_buffer_->Unmap(0, &read_range);
		mapped_atmosphere_constants_ = nullptr;
	}
}

void RenderSkyDX12System::RenderDX12(ID3D12GraphicsCommandList* cmd_list)
{
	cmd_list->SetGraphicsRootSignature(root_signature_.Get());
	cmd_list->SetGraphicsRootConstantBufferView(0, atmosphere_upload_buffer_->GetGPUVirtualAddress());
}
