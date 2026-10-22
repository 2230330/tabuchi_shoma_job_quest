#pragma once

#include<d3d12.h>
#include<wrl.h>

//ルートシグネチャの差込口の管理
//これは、レジスタの番号とは別物である
enum RootSignatureSlot :unsigned int
{
	kDx12Camera = 0,
	kDx12PassConstants,
	//kDx12Frame,
	//kDx12Lighting,
	kDx12TextureTable,

	kDx12RootParameterCount
};

//ルートシグネチャの数を減らすため、マネージャで管理し、
//同じルートシグネチャを使いまわすようにする
class RootSignatureManager
{
public:
	RootSignatureManager();
	~RootSignatureManager();

	//インスタン
	static RootSignatureManager& Instance()
	{
		static RootSignatureManager instance;
		return instance;
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature2D() const { return root_signature_2d_; }
	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature3D() const { return root_signature_3d_; }

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_2d_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_3d_;
};