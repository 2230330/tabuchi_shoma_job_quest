#include"../../headers/system/render_back_system.h"

#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
#include"../../headers/misc.h"

RenderBackSystem::RenderBackSystem()
	:IRenderDX12System(RenderPass_Background)
{
	//DX12用のルートシグネチャとパイプラインステートを作成する
	//ここでは、背景描画に必要なシェーダーやリソースを設定する
	//ルートシグネチャは、シェーダーが使用するリソースのレイアウトを定義する
	//パイプラインステートは、レンダリングの設定をまとめたもの

	//シグネチャの生成
	ID3D12Device* dx12_device = Graphics::Instance().GetDX12Device();
	
	D3D12_ROOT_SIGNATURE_DESC root_signature_desc{};
	root_signature_desc.NumParameters = 0;
	root_signature_desc.pParameters = nullptr;
	root_signature_desc.NumStaticSamplers = 0;
	root_signature_desc.pStaticSamplers = nullptr;
	root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob>serialized_signature;
	Microsoft::WRL::ComPtr<ID3DBlob>error_blob;
	
	HRESULT hr{ S_OK };
	hr = D3D12SerializeRootSignature(
		&root_signature_desc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serialized_signature.GetAddressOf(), 
		error_blob.GetAddressOf()
	);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	
	hr = dx12_device->CreateRootSignature(
		0,
		serialized_signature->GetBufferPointer(),
		serialized_signature->GetBufferSize(),
		IID_PPV_ARGS(root_signature_.GetAddressOf())
	);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	//シェーダー、パイプラインの設定
	ID3DBlob* vertex_shader_blob = nullptr;
	ID3D10Blob* pixel_shader_blob = nullptr;

	auto vs_blob = ResourceManager::Instance().LoadShaderBytecode(L"resources/shader/render_back_vs.dxil");
	auto ps_blob = ResourceManager::Instance().LoadShaderBytecode(L"resources/shader/render_back_ps.dxil");

	D3D12_SHADER_BYTECODE vs_bytecode{
		vs_blob->GetBufferPointer(),
		vs_blob->GetBufferSize()
	};
	D3D12_SHADER_BYTECODE ps_bytecode{
		ps_blob->GetBufferPointer(),
		ps_blob->GetBufferSize()
	};

	pipeline_state_ = Graphics::Instance().CreatePipelineState(
		root_signature_.Get(),
		vs_bytecode,
		ps_bytecode,
		PipelineKind::Background
	);
	
}

RenderBackSystem::~RenderBackSystem()
{
	//リソースの解放はComPtrが自動で行うので、特に何もしない
}

void RenderBackSystem::RenderDX12(ID3D12GraphicsCommandList* cmd_list)
{
	//DX12用の描画関数
	//ここで、背景を描画するためのコマンドをコマンドリストに記録する
	//具体的には、ルートシグネチャの設定、パイプラインステートの設定、
	//描画対象の設定、描画コマンドの発行などを行う
	
	//ルートシグネチャの設定
	cmd_list->SetGraphicsRootSignature(root_signature_.Get());

	//パイプラインステートの設定
	cmd_list->SetPipelineState(pipeline_state_.Get());

	//トポロジーの設定
	cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//描画、背景のみなので簡易的に
	cmd_list->DrawInstanced(3, 1, 0, 0); //フルスクリーン三角形を描画

}