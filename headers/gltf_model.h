#ifndef PART2_GLTF_MODEL_H
#define PART2_GLTF_MODEL_H

#define NOMINMAX
#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <vector>
#include <unordered_map>

#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include"..\\external\\tinygltf-2.9.6\\tiny_gltf.h"

class GltfModel
{
public:
	struct Node
	{
		std::string name;
		int skin{ -1 };  // index of skin referenced by this node
		int mesh{ -1 };  // index of mesh referenced by this node

		std::vector<int> children; // An array of indices of child nodes of this node

		// Local transforms
		DirectX::XMFLOAT4 rotation{ 0, 0, 0, 1 };
		DirectX::XMFLOAT3 scale{ 1, 1, 1 };
		DirectX::XMFLOAT3 translation{ 0, 0, 0 };

		DirectX::XMFLOAT4X4 global_transform{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	};
	struct Animation
	{
		std::string name;
		float duration{ 0.0f };

		struct Channel
		{
			int sampler{ -1 }; // required
			int target_node{ -1 }; // required (index of the node to target)
			std::string target_path; // required in ["translation", "rotation", "scale", "weights"]
		};
		std::vector<Channel> channels;

		struct Sampler
		{
			int input{ -1 };
			int output{ -1 };
			std::string interpolation;
		};
		std::vector<Sampler> samplers;

		std::unordered_map<int/*sampler.input*/, std::vector<float>> timelines;
		std::unordered_map<int/*sampler.output*/, std::vector<DirectX::XMFLOAT3>> scales;
		std::unordered_map<int/*sampler.output*/, std::vector<DirectX::XMFLOAT4>> rotations;
		std::unordered_map<int/*sampler.output*/, std::vector<DirectX::XMFLOAT3>> translations;
	};

	GltfModel(ID3D11Device* device, const std::string& filename);
	virtual ~GltfModel() = default;

	void Render(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world);
	void Animate(size_t animation_index, float time, std::vector<Node>& animated_nodes);
	void UpdateAnimation(float elapsed_time);
	const std::vector<GltfModel::Node>& GetNodes()const;
	const std::vector<GltfModel::Animation>& GetAnimations()const;
private:
	struct Scene
	{
		std::string name;
		std::vector<int> nodes; // Array of 'root' nodes
	};
	struct BufferView
	{
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		int buffer = -1;
		size_t stride_in_bytes{ 0 };
		size_t byte_offset{ 0 };
		size_t count{ 0 };
	};
	struct Mesh
	{
		std::string name;
		struct primitive
		{
			int material;
			std::map<std::string, BufferView> vertex_buffer_views;
			BufferView index_buffer_view;

			bool has(const char* attribute) const
			{
				return vertex_buffer_views.find(attribute) !=
					vertex_buffer_views.end() 
					&& vertex_buffer_views.at(attribute).buffer > -1;
			}
		};
		std::vector<primitive> primitives;
	};
	struct TextureInfo
	{
		int index = -1; // required.
		int texcoord = 0; // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
	};
	struct NormalTextureInfo
	{
		int index = -1;  // required
		int texcoord = 0;    // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
		float scale = 1;    // scaledNormal = normalize((<sampled normal texture value> * 2.0 - 1.0) * vec3(<normal scale>, <normal scale>, 1.0))
	};
	struct OcclusionTextureInfo
	{
		int index = -1;   // required
		int texcoord = 0;     // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
		float strength = 1;  // A scalar parameter controlling the amount of occlusion applied. A value of `0.0` means no occlusion. A value of `1.0` means full occlusion. This value affects the final occlusion value as: `1.0 + strength * (<sampled occlusion texture value> - 1.0)`.
	};
	struct PbrMetallicRoughness
	{
		float basecolor_factor[4] = { 1, 1, 1, 1 };  // len = 4. default [1,1,1,1]
		TextureInfo basecolor_texture;
		float metallic_factor = 1;   // default 1
		float roughness_factor = 1;  // default 1
		TextureInfo metallic_roughness_texture;
	};
	struct Material
	{
		std::string name;
		struct Cbuffer
		{
			float emissive_factor[3] = { 0, 0, 0 };  // length 3. default [0, 0, 0]
			int alpha_mode = 0;	// "OPAQUE" : 0, "MASK" : 1, "BLEND" : 2
			float alpha_cutoff = 0.5f; // default 0.5
			int double_sided = 0; // default false;

			PbrMetallicRoughness pbr_metallic_roughness;

			NormalTextureInfo normal_texture;
			OcclusionTextureInfo occlusion_texture;
			TextureInfo emissive_texture;
		};
		Cbuffer data;
	};
	struct Texture
	{
		std::string name;
		int source{ -1 };
	};
	struct Image
	{
		std::string name;
		int width{ -1 };
		int height{ -1 };
		int component{ -1 };
		int bits{ -1 };			// bit depth per channel. 8(byte), 16 or 32.
		int pixel_type{ -1 };	// pixel type(TINYGLTF_COMPONENT_TYPE_***). usually UBYTE(bits = 8) or USHORT(bits = 16)
		int buffer_view;		// (required if no uri)
		std::string mime_type;	// (required if no uri) ["image/jpeg", "image/png", "image/bmp", "image/gif"]
		std::string uri;		

		bool as_is{ false };
	};
	struct Skin
	{
		std::vector<DirectX::XMFLOAT4X4> inverse_bind_matrices;
		std::vector<int> joints;
	};
	struct PrimitiveConstants
	{
		DirectX::XMFLOAT4X4 world;
		int material{ -1 };
		int has_tangent{ 0 };
		int skin{ -1 };
		int pad;
	};
	static const size_t PRIMITIVE_MAX_JOINTS = 512;
	struct PrimitiveJointConstants
	{
		DirectX::XMFLOAT4X4 matrices[PRIMITIVE_MAX_JOINTS];
	};

	void FetchNodes(const tinygltf::Model& gltf_model);
	void FetchMeshes(ID3D11Device* device, const tinygltf::Model& gltf_model);
	void FetchMaterials(ID3D11Device* device, const tinygltf::Model& gltf_model);
	void FetchTextures(ID3D11Device* device, const tinygltf::Model& gltf_model);
	void FetchAnimations(const tinygltf::Model& gltf_model);
	void CumulateTransforms(std::vector<Node>& nodes);


	std::string filename_;
	int default_scene_ = 0;
	float animation_time_ = 0;

	std::vector<Node> nodes_;
	std::vector<Node> animated_nodes_;	//アニメーションで変更されたノード
	std::vector<Scene> scenes;
	std::vector<Mesh> meshes_;
	std::vector<Material> materials_;
	std::vector<Texture> textures_;
	std::vector<Animation> animations_;
	std::vector<Skin> skins_;
	std::vector<Image> images_;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> texture_resource_views_;
	std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> buffers_;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> material_resource_view_;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader_;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_cbuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_joint_cbuffer;
};
#endif // !PART2_GLTF_MODEL_H
