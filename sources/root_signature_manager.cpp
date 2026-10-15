#include"../headers/root_signature_manager.h"

#include"../headers/graphics.h"
#include"../headers/misc.h"


RootSignatureManager::RootSignatureManager()
	:root_signature_2d_(nullptr),root_signature_3d_(nullptr)
{
	//3d用のルートシグネチャを作成する
	{
		D3D12_ROOT_PARAMETER root_parameters[2]{};
		root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[0].Descriptor.ShaderRegister = 0;
		root_parameters[0].Descriptor.RegisterSpace = 0;
		root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[1].Descriptor.ShaderRegister = 1;
		root_parameters[1].Descriptor.RegisterSpace = 0;
		root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		D3D12_ROOT_SIGNATURE_DESC root_signature_desc{};
		root_signature_desc.NumParameters = _countof(root_parameters);
		root_signature_desc.pParameters = root_parameters;
		root_signature_desc.NumStaticSamplers = 0;
		root_signature_desc.pStaticSamplers = nullptr;
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