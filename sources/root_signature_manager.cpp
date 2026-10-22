#include"../headers/root_signature_manager.h"

#include"../headers/graphics.h"
#include"../headers/misc.h"


RootSignatureManager::RootSignatureManager()
	:root_signature_2d_(nullptr),root_signature_3d_(nullptr)
{
	//3d用のルートシグネチャを作成する
	{
		//CBVの設定
		D3D12_ROOT_PARAMETER root_parameters[RootSignatureSlot::kDx12RootParameterCount]{};
		root_parameters[RootSignatureSlot::kDx12Camera].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[RootSignatureSlot::kDx12Camera].Descriptor.ShaderRegister = RootSignatureSlot::kDx12Camera;
		root_parameters[RootSignatureSlot::kDx12Camera].Descriptor.RegisterSpace = 0;
		root_parameters[RootSignatureSlot::kDx12Camera].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		root_parameters[RootSignatureSlot::kDx12PassConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[RootSignatureSlot::kDx12PassConstants].Descriptor.ShaderRegister = RootSignatureSlot::kDx12PassConstants;
		root_parameters[RootSignatureSlot::kDx12PassConstants].Descriptor.RegisterSpace = 0;
		root_parameters[RootSignatureSlot::kDx12PassConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		//SRVテーブルの設定
		D3D12_DESCRIPTOR_RANGE texture_range{};
		texture_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		texture_range.NumDescriptors = 9;//現在の最大数(雲シェーダー使用時の数)
		texture_range.BaseShaderRegister = 0;//register(t0)~
		texture_range.RegisterSpace = 0;
		texture_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		//ルート番号をSRVテーブルにする
		root_parameters[RootSignatureSlot::kDx12TextureTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		root_parameters[RootSignatureSlot::kDx12TextureTable].DescriptorTable.NumDescriptorRanges = 1;
		root_parameters[RootSignatureSlot::kDx12TextureTable].DescriptorTable.pDescriptorRanges = &texture_range;
		root_parameters[RootSignatureSlot::kDx12TextureTable].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;


		//Samplerの設定
		D3D12_STATIC_SAMPLER_DESC static_samplers[6]{};

		auto SetSampler = [&static_samplers](
			UINT index,
			D3D12_FILTER filter,
			D3D12_TEXTURE_ADDRESS_MODE address_mode,
			UINT max_anisotropy
			)
			{
				auto& sampler = static_samplers[index];

				sampler.Filter = filter;
				sampler.AddressU = address_mode;
				sampler.AddressV = address_mode;
				sampler.AddressW = address_mode;
				sampler.MipLODBias = 0.0f;
				sampler.MaxAnisotropy = max_anisotropy;
				sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
				sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
				sampler.MinLOD = 0.0f;
				sampler.MaxLOD = D3D12_FLOAT32_MAX;

				//シェーダー側のサンプラー差込口に対応
				sampler.ShaderRegister = index;
				sampler.RegisterSpace = 0;
				sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			};

		SetSampler(0, D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP, 1);
		SetSampler(1, D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 1);
		SetSampler(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP, 1);
		SetSampler(3, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 1);
		SetSampler(4, D3D12_FILTER_ANISOTROPIC,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);
		SetSampler(5, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_MIRROR, 1);

		//ルートシグネチャの設定
		D3D12_ROOT_SIGNATURE_DESC root_signature_desc{};
		root_signature_desc.NumParameters = _countof(root_parameters);
		root_signature_desc.pParameters = root_parameters;
		root_signature_desc.NumStaticSamplers = _countof(static_samplers);
		root_signature_desc.pStaticSamplers = static_samplers;
		root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
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
			IID_PPV_ARGS(root_signature_3d_.GetAddressOf())
		);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		
	}
}

RootSignatureManager::~RootSignatureManager()
{
	root_signature_2d_.Reset();
	root_signature_3d_.Reset();
}