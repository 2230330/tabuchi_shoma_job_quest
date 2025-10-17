#include"../headers/gltf_model.h"
#include<stack>
#include<iostream>
#include<crtdbg.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include"..\\external\\tinygltf-2.9.6\\tiny_gltf.h"


#include"../headers/misc.h"
#include"../headers/shader.h"
#include"../headers/texture.h"
#include"../headers/constant_buffer_slot.h"

bool NullLoadImageData
(tinygltf::Image*, const int, std::string*, std::string*, int, int, const unsigned char*, int, void*)
{
	return true;
}
GltfModel::GltfModel(ID3D11Device* device, const std::string& filename) : filename_(filename)
{
	tinygltf::TinyGLTF tiny_gltf;
	tiny_gltf.SetImageLoader(NullLoadImageData, nullptr);

	tinygltf::Model gltf_model;
	std::string error, warning;
	bool succeeded{ false };
	if (filename.find(".glb") != std::string::npos)
	{
		succeeded = tiny_gltf.LoadBinaryFromFile(&gltf_model, &error, &warning, filename.c_str());
	}
	else if (filename.find(".gltf") != std::string::npos)
	{
		succeeded = tiny_gltf.LoadASCIIFromFile(&gltf_model, &error, &warning, filename.c_str(),0);
	}

	_ASSERT_EXPR_A(warning.empty(), warning.c_str());
	_ASSERT_EXPR_A(error.empty(), error.c_str());
	_ASSERT_EXPR_A(succeeded, L"Failed to load glTF file");

	for (std::vector<tinygltf::Scene>::const_reference gltf_scene : gltf_model.scenes)
	{
		Scene& scene{ scenes.emplace_back() };
		scene.name = gltf_scene.name;
		scene.nodes = gltf_scene.nodes;
	}
	//sceneが－1にならないようにガード
	default_scene_ = gltf_model.defaultScene >= 0 ? gltf_model.defaultScene : 0;

	FetchNodes(gltf_model);
	FetchMeshes(device, gltf_model);
	FetchMaterials(device, gltf_model);
	FetchTextures(device, gltf_model);
	FetchAnimations(gltf_model);

	animated_nodes_ = nodes_;

	D3D11_INPUT_ELEMENT_DESC input_element_desc[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "JOINTS", 0, DXGI_FORMAT_R16G16B16A16_UINT, 4, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0,DXGI_FORMAT_R32G32B32A32_FLOAT, 5, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	shader_from_cso::CreateVsFromCso(device, "./resources/shader/gltf_model_vs.cso", vertex_shader_.ReleaseAndGetAddressOf(), 
		input_layout.ReleaseAndGetAddressOf(), input_element_desc, _countof(input_element_desc));

	//インスタンシング描画を埋め込みたい
	D3D11_INPUT_ELEMENT_DESC instancing_input_element_desc[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "JOINTS", 0, DXGI_FORMAT_R16G16B16A16_UINT, 4, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0,DXGI_FORMAT_R32G32B32A32_FLOAT, 5, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "WORLD_MATRIX", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 6,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "WORLD_MATRIX", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 6, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "WORLD_MATRIX", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 6, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "WORLD_MATRIX", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 6, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "PREVIOUS_WORLD_MATRIX", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 7,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "PREVIOUS_WORLD_MATRIX", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 7, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "PREVIOUS_WORLD_MATRIX", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 7, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "PREVIOUS_WORLD_MATRIX", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 7, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};	
	shader_from_cso::CreateVsFromCso(device, "./resources/shader/gltf_model_forward_instancing_vs.cso", instancing_vertex_shader_.ReleaseAndGetAddressOf(),
		instancing_input_layout.ReleaseAndGetAddressOf(), instancing_input_element_desc, _countof(instancing_input_element_desc));

	shader_from_cso::CreatePsFromCso(device, "./resources/shader/gltf_model_ps.cso", pixel_shader.ReleaseAndGetAddressOf());

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(PrimitiveConstants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	HRESULT hr;
	hr = device->CreateBuffer(&buffer_desc, nullptr, primitive_cbuffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	buffer_desc.ByteWidth = sizeof(PrimitiveJointConstants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&buffer_desc, NULL, primitive_joint_cbuffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

}
void GltfModel::FetchNodes(const tinygltf::Model& gltf_model)
{
	for (std::vector<tinygltf::Node>::const_reference gltf_node : gltf_model.nodes)
	{
		Node& node{ nodes_.emplace_back() };
		node.name = gltf_node.name;
		node.skin = gltf_node.skin;
		node.mesh = gltf_node.mesh;
		node.children = gltf_node.children;
		if (!gltf_node.matrix.empty())
		{
			DirectX::XMFLOAT4X4 matrix;
			for (size_t row = 0; row < 4; row++)
			{
				for (size_t column = 0; column < 4; column++)
				{
					matrix(row, column) = static_cast<float>(gltf_node.matrix.at(4 * row + column));
				}
			}

			DirectX::XMVECTOR S, T, R;
			bool succeed = DirectX::XMMatrixDecompose(&S, &R, &T, DirectX::XMLoadFloat4x4(&matrix));
			_ASSERT_EXPR(succeed, L"Failed to decompose matrix.");

			DirectX::XMStoreFloat3(&node.scale, S);
			DirectX::XMStoreFloat4(&node.rotation, R);
			DirectX::XMStoreFloat3(&node.translation, T);
		}
		else
		{
			if (gltf_node.scale.size() > 0)
			{
				node.scale.x = static_cast<float>(gltf_node.scale.at(0));
				node.scale.y = static_cast<float>(gltf_node.scale.at(1));
				node.scale.z = static_cast<float>(gltf_node.scale.at(2));
			}
			if (gltf_node.translation.size() > 0)
			{
				node.translation.x = static_cast<float>(gltf_node.translation.at(0));
				node.translation.y = static_cast<float>(gltf_node.translation.at(1));
				node.translation.z = static_cast<float>(gltf_node.translation.at(2));
			}
			if (gltf_node.rotation.size() > 0)
			{
				node.rotation.x = static_cast<float>(gltf_node.rotation.at(0));
				node.rotation.y = static_cast<float>(gltf_node.rotation.at(1));
				node.rotation.z = static_cast<float>(gltf_node.rotation.at(2));
				node.rotation.w = static_cast<float>(gltf_node.rotation.at(3));
			}
		}
	}
	CumulateTransforms(nodes_);
}
void GltfModel::CumulateTransforms(std::vector<Node>& nodes)
{
	using namespace DirectX;

	std::stack<XMFLOAT4X4> parent_global_transforms;
	std::function<void(int)> traverse{ [&](int node_index)->void
	{
		Node& node{nodes.at(node_index)};
		XMMATRIX S{ XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z) };
		XMMATRIX R{ XMMatrixRotationQuaternion(XMVectorSet(node.rotation.x, node.rotation.y, node.rotation.z, node.rotation.w)) };
		XMMATRIX T{ XMMatrixTranslation(node.translation.x, node.translation.y, node.translation.z) };
		DirectX::XMStoreFloat4x4(&node.global_transform, S * R * T * XMLoadFloat4x4(&parent_global_transforms.top()));
		for (int child_index : node.children)
		{
			parent_global_transforms.push(node.global_transform);
			traverse(child_index);
			parent_global_transforms.pop();
		}
	} };
	for (std::vector<int>::value_type node_index : scenes.at(default_scene_).nodes)
	{
		parent_global_transforms.push({ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 });
		traverse(node_index);
		parent_global_transforms.pop();
	}
}
DXGI_FORMAT ConvertFormat(const tinygltf::Accessor& accessor)
{
	switch (accessor.type)
	{
	case TINYGLTF_TYPE_SCALAR:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			return DXGI_FORMAT_R8_UINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			return DXGI_FORMAT_R16_UINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			return DXGI_FORMAT_R32_UINT;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			return DXGI_FORMAT_UNKNOWN;
		}
	case TINYGLTF_TYPE_VEC2:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			return DXGI_FORMAT_R32G32_FLOAT;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			return DXGI_FORMAT_UNKNOWN;
		}
	case TINYGLTF_TYPE_VEC3:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			return DXGI_FORMAT_R32G32B32_FLOAT;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			return DXGI_FORMAT_UNKNOWN;
		}
	case TINYGLTF_TYPE_VEC4:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			return DXGI_FORMAT_R8G8B8A8_UINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			return DXGI_FORMAT_R16G16B16A16_UINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			return DXGI_FORMAT_R32G32B32A32_UINT;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			return DXGI_FORMAT_UNKNOWN;
		}
		break;
	default:
		_ASSERT_EXPR(FALSE, L"This accessor type is not supported.");
		return DXGI_FORMAT_UNKNOWN;
	}
}
void GltfModel::FetchMeshes(ID3D11Device* device, const tinygltf::Model& gltf_model)
{
	HRESULT hr;

	size_t gltf_buffer_count = gltf_model.buffers.size();
	buffers_.resize(gltf_buffer_count);

	// Create buffers
	for (size_t gltf_buffer_index = 0; gltf_buffer_index < gltf_buffer_count; ++gltf_buffer_index)
	{
		const tinygltf::Buffer& gltf_buffer = gltf_model.buffers.at(gltf_buffer_index);

		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.ByteWidth = static_cast<UINT>(gltf_buffer.data.size());
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA subresource_data{};
		subresource_data.pSysMem = gltf_buffer.data.data();
		hr = device->CreateBuffer(&buffer_desc, &subresource_data, buffers_.at(gltf_buffer_index).ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	for (std::vector<tinygltf::Mesh>::const_reference gltf_mesh : gltf_model.meshes)
	{
		Mesh& mesh{ meshes_.emplace_back() };
		mesh.name = gltf_mesh.name;
		for (std::vector<tinygltf::Primitive>::const_reference gltf_primitive : gltf_mesh.primitives)
		{
			Mesh::primitive& primitive{ mesh.primitives.emplace_back() };
			primitive.material = gltf_primitive.material;

			// Create index buffer view
			if (gltf_primitive.indices > -1)
			{
				const tinygltf::Accessor& gltf_accessor{ gltf_model.accessors.at(gltf_primitive.indices) };
				const tinygltf::BufferView& gltf_buffer_view{ gltf_model.bufferViews.at(gltf_accessor.bufferView) };

				primitive.index_buffer_view.format = ConvertFormat(gltf_accessor);
				primitive.index_buffer_view.buffer = gltf_buffer_view.buffer;
				primitive.index_buffer_view.stride_in_bytes = gltf_accessor.ByteStride(gltf_buffer_view);
				primitive.index_buffer_view.byte_offset = gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
				primitive.index_buffer_view.count = gltf_accessor.count;
			}
			// Create vertex buffer views
			for (std::map<std::string, int>::const_reference gltf_attribute : gltf_primitive.attributes)
			{
				const tinygltf::Accessor& gltf_accessor{ gltf_model.accessors.at(gltf_attribute.second) };
				const tinygltf::BufferView& gltf_buffer_view{ gltf_model.bufferViews.at(gltf_accessor.bufferView) };

				BufferView vertex_buffer_view{};


#if 1
				if (gltf_attribute.first == "WEIGHTS_0" && gltf_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
				{
					if (gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
					{
						std::vector<FLOAT> weights_0(gltf_accessor.count * 4);
						const BYTE* data = reinterpret_cast<const BYTE*>(gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset);
						for (size_t accessor_index = 0; accessor_index < gltf_accessor.count; ++accessor_index)
						{
							weights_0.at(accessor_index * 4 + 0) = static_cast<FLOAT>(data[accessor_index * 4 + 0]) / 0xFF;
							weights_0.at(accessor_index * 4 + 1) = static_cast<FLOAT>(data[accessor_index * 4 + 1]) / 0xFF;
							weights_0.at(accessor_index * 4 + 2) = static_cast<FLOAT>(data[accessor_index * 4 + 2]) / 0xFF;
							weights_0.at(accessor_index * 4 + 3) = static_cast<FLOAT>(data[accessor_index * 4 + 3]) / 0xFF;
						}

						vertex_buffer_view.buffer = static_cast<int>(buffers_.size());
						vertex_buffer_view.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
						vertex_buffer_view.stride_in_bytes = sizeof(FLOAT) * 4;
						vertex_buffer_view.byte_offset = 0;
						vertex_buffer_view.count = weights_0.size();

						Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
						D3D11_BUFFER_DESC buffer_desc{};
						buffer_desc.ByteWidth = static_cast<UINT>(weights_0.size() * sizeof(FLOAT));
						buffer_desc.Usage = D3D11_USAGE_DEFAULT;
						buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
						D3D11_SUBRESOURCE_DATA subresource_data{};
						subresource_data.pSysMem = weights_0.data();
						hr = device->CreateBuffer(&buffer_desc, &subresource_data, buffer.ReleaseAndGetAddressOf());
						_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

						buffers_.emplace_back(buffer);
					}
					else
					{
						_ASSERT_EXPR(FALSE, L"This component type is unsupported, please convert it yourself if necessary.");
					}
				}
				else if (gltf_attribute.first == "JOINTS_0" && gltf_accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					if (gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
					{
						std::vector<USHORT> joints_0(gltf_accessor.count * 4);
						const BYTE* data = reinterpret_cast<const BYTE*>(gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset);
						for (size_t accessor_index = 0; accessor_index < gltf_accessor.count; ++accessor_index)
						{
							joints_0.at(accessor_index * 4 + 0) = static_cast<USHORT>(data[accessor_index * 4 + 0]);
							joints_0.at(accessor_index * 4 + 1) = static_cast<USHORT>(data[accessor_index * 4 + 1]);
							joints_0.at(accessor_index * 4 + 2) = static_cast<USHORT>(data[accessor_index * 4 + 2]);
							joints_0.at(accessor_index * 4 + 3) = static_cast<USHORT>(data[accessor_index * 4 + 3]);
						}

						vertex_buffer_view.buffer = static_cast<int>(buffers_.size());
						vertex_buffer_view.format = DXGI_FORMAT_R16G16B16A16_UINT;
						vertex_buffer_view.stride_in_bytes = sizeof(USHORT) * 4;
						vertex_buffer_view.byte_offset = 0;
						vertex_buffer_view.count = joints_0.size();

						Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
						D3D11_BUFFER_DESC buffer_desc{};
						buffer_desc.ByteWidth = static_cast<UINT>(joints_0.size() * sizeof(USHORT));
						buffer_desc.Usage = D3D11_USAGE_DEFAULT;
						buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
						D3D11_SUBRESOURCE_DATA subresource_data{};
						subresource_data.pSysMem = joints_0.data();
						hr = device->CreateBuffer(&buffer_desc, &subresource_data, buffer.ReleaseAndGetAddressOf());
						_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

						buffers_.emplace_back(buffer);
					}
					else
					{
						_ASSERT_EXPR(FALSE, L"This component type is unsupported, please convert it yourself if necessary.");
					}
				}
				else
#endif
				{
					vertex_buffer_view.format = ConvertFormat(gltf_accessor);
					vertex_buffer_view.buffer = gltf_buffer_view.buffer;
					vertex_buffer_view.stride_in_bytes = gltf_accessor.ByteStride(gltf_buffer_view);
					vertex_buffer_view.byte_offset = gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
					vertex_buffer_view.count = gltf_accessor.count;
				}
				primitive.vertex_buffer_views.emplace(std::make_pair(gltf_attribute.first, vertex_buffer_view));
			}

			// Add dummy attributes if any are missing. 
			const std::unordered_map<std::string, BufferView> attributes{
			 { "TANGENT", { DXGI_FORMAT_R32G32B32A32_FLOAT } },
			 { "TEXCOORD_0", { DXGI_FORMAT_R32G32_FLOAT } },
			 { "JOINTS_0", { DXGI_FORMAT_R16G16B16A16_UINT } },
			 { "WEIGHTS_0", { DXGI_FORMAT_R32G32B32A32_FLOAT } },
			};
			for (std::unordered_map<std::string, BufferView>::const_reference attribute : attributes)
			{
				if (primitive.vertex_buffer_views.find(attribute.first) == primitive.vertex_buffer_views.end())
				{
					primitive.vertex_buffer_views.insert(std::make_pair(attribute.first, attribute.second));
				}
			}
		}
	}
}

void GltfModel::Render(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world)
{
	using namespace DirectX;

	const std::vector<Node>& nodes{ animated_nodes_.size() > 0 ? animated_nodes_ : GltfModel::nodes_ };

	immediate_context->PSSetShaderResources(0, 1, material_resource_view_.GetAddressOf());

	immediate_context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
	immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);
	immediate_context->IASetInputLayout(input_layout.Get());
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	std::function<void(int)> traverse{ [&](int node_index)->void {
		const Node& node{nodes.at(node_index)};
		if (node.skin > -1)
		{
			const Skin& skin{ skins_.at(node.skin) };
			_ASSERT_EXPR(skin.joints.size() <= PRIMITIVE_MAX_JOINTS, L"The size of the joint array is insufficient, please expand it.");
			PrimitiveJointConstants primitive_joint_data{};
			for (size_t joint_index = 0; joint_index < skin.joints.size(); ++joint_index)
			{
				DirectX::XMStoreFloat4x4(&primitive_joint_data.matrices[joint_index],
					XMLoadFloat4x4(&skin.inverse_bind_matrices.at(joint_index)) *
					XMLoadFloat4x4(&nodes.at(skin.joints.at(joint_index)).global_transform) *
					XMMatrixInverse(NULL, XMLoadFloat4x4(&node.global_transform))
				);
			}
			immediate_context->UpdateSubresource(primitive_joint_cbuffer.Get(), 0, 0, &primitive_joint_data, 0, 0);
			immediate_context->VSSetConstantBuffers(3, 1, primitive_joint_cbuffer.GetAddressOf());
		}
		if (node.mesh > -1)
		{
			const Mesh& mesh{ meshes_.at(node.mesh) };
			for (const Mesh::primitive& primitive : mesh.primitives)
			{
				ID3D11Buffer* vertex_buffers[]{
					primitive.has("POSITION") ? buffers_.at(primitive.vertex_buffer_views.at("POSITION").buffer).Get() : 0,
					primitive.has("NORMAL") ? buffers_.at(primitive.vertex_buffer_views.at("NORMAL").buffer).Get() : 0,
					primitive.has("TANGENT") ? buffers_.at(primitive.vertex_buffer_views.at("TANGENT").buffer).Get() : 0,
					primitive.has("TEXCOORD_0") ? buffers_.at(primitive.vertex_buffer_views.at("TEXCOORD_0").buffer).Get() : 0,
					primitive.has("JOINTS_0") ? buffers_.at(primitive.vertex_buffer_views.at("JOINTS_0").buffer).Get() : 0,
					primitive.has("WEIGHTS_0") ? buffers_.at(primitive.vertex_buffer_views.at("WEIGHTS_0").buffer).Get() : 0,
				};
				UINT strides[]{
					primitive.has("POSITION") ? static_cast<UINT>(primitive.vertex_buffer_views.at("POSITION").stride_in_bytes) : 0,
					primitive.has("NORMAL") ? static_cast<UINT>(primitive.vertex_buffer_views.at("NORMAL").stride_in_bytes) : 0,
					primitive.has("TANGENT") ? static_cast<UINT>(primitive.vertex_buffer_views.at("TANGENT").stride_in_bytes) : 0,
					primitive.has("TEXCOORD_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("TEXCOORD_0").stride_in_bytes) : 0,
					primitive.has("JOINTS_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("JOINTS_0").stride_in_bytes) : 0,
					primitive.has("WEIGHTS_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("WEIGHTS_0").stride_in_bytes) : 0,
				};
				UINT offsets[]{
					primitive.has("POSITION") ? static_cast<UINT>(primitive.vertex_buffer_views.at("POSITION").byte_offset) : 0,
					primitive.has("NORMAL") ? static_cast<UINT>(primitive.vertex_buffer_views.at("NORMAL").byte_offset) : 0,
					primitive.has("TANGENT") ? static_cast<UINT>(primitive.vertex_buffer_views.at("TANGENT").byte_offset) : 0,
					primitive.has("TEXCOORD_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("TEXCOORD_0").byte_offset) : 0,
					primitive.has("JOINTS_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("JOINTS_0").byte_offset) : 0,
					primitive.has("WEIGHTS_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("WEIGHTS_0").byte_offset) : 0,
				};
				immediate_context->IASetVertexBuffers(0, _countof(vertex_buffers), vertex_buffers, strides, offsets);


				PrimitiveConstants primitive_data{};
				primitive_data.material = primitive.material;
				primitive_data.has_tangent = primitive.has("TANGENT");
				primitive_data.skin = node.skin;
				DirectX::XMStoreFloat4x4(&primitive_data.world, XMLoadFloat4x4(&node.global_transform) * XMLoadFloat4x4(&world));
				immediate_context->UpdateSubresource(primitive_cbuffer.Get(), 0, 0, &primitive_data, 0, 0);
				immediate_context->VSSetConstantBuffers(static_cast<UINT>(ConstantBufferSlot::kPerObject), 1, primitive_cbuffer.GetAddressOf());
				immediate_context->PSSetConstantBuffers(static_cast<UINT>(ConstantBufferSlot::kPerObject), 1, primitive_cbuffer.GetAddressOf());

				// UNIT.36
				const Material& material{ materials_.at(primitive.material) };
				const int texture_indices[]
				{
					material.data.pbr_metallic_roughness.basecolor_texture.index,
					material.data.pbr_metallic_roughness.metallic_roughness_texture.index,
					material.data.normal_texture.index,
					material.data.emissive_texture.index,
					material.data.occlusion_texture.index,
				};
				ID3D11ShaderResourceView* null_shader_resource_view{};
				std::vector<ID3D11ShaderResourceView*> shader_resource_views(_countof(texture_indices));
				for (int texture_index = 0; texture_index < shader_resource_views.size(); ++texture_index)
				{
					shader_resource_views.at(texture_index) = texture_indices[texture_index] > -1 
						? texture_resource_views_.at(textures_.at(texture_indices[texture_index]).source).Get() 
						: null_shader_resource_view;
				}
				immediate_context->PSSetShaderResources(1, static_cast<UINT>(shader_resource_views.size()), shader_resource_views.data());

				if (primitive.index_buffer_view.count > 0)
				{
					if (primitive.index_buffer_view.format == DXGI_FORMAT_R8_UINT)
					{
						immediate_context->IASetIndexBuffer(
							buffers_.at(primitive.index_buffer_view.buffer).Get(),
							DXGI_FORMAT_R16_UINT, static_cast<UINT>(primitive.index_buffer_view.byte_offset));

					}
					else
					{

					immediate_context->IASetIndexBuffer(
						buffers_.at(primitive.index_buffer_view.buffer).Get(),
						primitive.index_buffer_view.format, static_cast<UINT>(primitive.index_buffer_view.byte_offset));
					
					}

					immediate_context->DrawIndexed(static_cast<UINT>(primitive.index_buffer_view.count), 0, 0);
				}
				else
				{
					immediate_context->Draw(static_cast<UINT>(primitive.vertex_buffer_views.at("POSITION").count), 0);
				}
			}
		}
		for (std::vector<int>::value_type child_index : node.children)
		{
			traverse(child_index);
		}
	} };
	for (std::vector<int>::value_type node_index : scenes.at(default_scene_).nodes)
	{
		traverse(node_index);
	}
}

void GltfModel::InstancingRender(ID3D11DeviceContext* immediate_context, UINT instance_count, ID3D11Buffer* world_matrices_buffer, UINT start_instance_location)
{
	using namespace DirectX;

	const std::vector<Node>& nodes{ animated_nodes_.size() > 0 ? animated_nodes_ : GltfModel::nodes_ };

	immediate_context->PSSetShaderResources(0, 1, material_resource_view_.GetAddressOf());

	immediate_context->VSSetShader(instancing_vertex_shader_.Get(), nullptr, 0);
	immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);
	immediate_context->IASetInputLayout(instancing_input_layout.Get());
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	std::function<void(int)> traverse{ [&](int node_index)->void {
		const Node& node{nodes.at(node_index)};
		if (node.skin > -1)
		{
			const Skin& skin{ skins_.at(node.skin) };
			_ASSERT_EXPR(skin.joints.size() <= PRIMITIVE_MAX_JOINTS, L"The size of the joint array is insufficient, please expand it.");
			PrimitiveJointConstants primitive_joint_data{};
			for (size_t joint_index = 0; joint_index < skin.joints.size(); ++joint_index)
			{
				DirectX::XMStoreFloat4x4(&primitive_joint_data.matrices[joint_index],
					XMLoadFloat4x4(&skin.inverse_bind_matrices.at(joint_index)) *
					XMLoadFloat4x4(&nodes.at(skin.joints.at(joint_index)).global_transform) *
					XMMatrixInverse(NULL, XMLoadFloat4x4(&node.global_transform))
				);
			}
			immediate_context->UpdateSubresource(primitive_joint_cbuffer.Get(), 0, 0, &primitive_joint_data, 0, 0);
			immediate_context->VSSetConstantBuffers(3, 1, primitive_joint_cbuffer.GetAddressOf());
		}
		if (node.mesh > -1)
		{
			const Mesh& mesh{ meshes_.at(node.mesh) };
			for (const Mesh::primitive& primitive : mesh.primitives)
			{
				ID3D11Buffer* vertex_buffers[]{
					primitive.has("POSITION") ? buffers_.at(primitive.vertex_buffer_views.at("POSITION").buffer).Get() : 0,
					primitive.has("NORMAL") ? buffers_.at(primitive.vertex_buffer_views.at("NORMAL").buffer).Get() : 0,
					primitive.has("TANGENT") ? buffers_.at(primitive.vertex_buffer_views.at("TANGENT").buffer).Get() : 0,
					primitive.has("TEXCOORD_0") ? buffers_.at(primitive.vertex_buffer_views.at("TEXCOORD_0").buffer).Get() : 0,
					primitive.has("JOINTS_0") ? buffers_.at(primitive.vertex_buffer_views.at("JOINTS_0").buffer).Get() : 0,
					primitive.has("WEIGHTS_0") ? buffers_.at(primitive.vertex_buffer_views.at("WEIGHTS_0").buffer).Get() : 0,
					world_matrices_buffer,
					world_matrices_buffer,
				};
				UINT strides[]{
					primitive.has("POSITION") ? static_cast<UINT>(primitive.vertex_buffer_views.at("POSITION").stride_in_bytes) : 0,
					primitive.has("NORMAL") ? static_cast<UINT>(primitive.vertex_buffer_views.at("NORMAL").stride_in_bytes) : 0,
					primitive.has("TANGENT") ? static_cast<UINT>(primitive.vertex_buffer_views.at("TANGENT").stride_in_bytes) : 0,
					primitive.has("TEXCOORD_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("TEXCOORD_0").stride_in_bytes) : 0,
					primitive.has("JOINTS_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("JOINTS_0").stride_in_bytes) : 0,
					primitive.has("WEIGHTS_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("WEIGHTS_0").stride_in_bytes) : 0,
					sizeof(DirectX::XMFLOAT4X4),
					sizeof(DirectX::XMFLOAT4X4),
				};
				UINT offsets[]{
					primitive.has("POSITION") ? static_cast<UINT>(primitive.vertex_buffer_views.at("POSITION").byte_offset) : 0,
					primitive.has("NORMAL") ? static_cast<UINT>(primitive.vertex_buffer_views.at("NORMAL").byte_offset) : 0,
					primitive.has("TANGENT") ? static_cast<UINT>(primitive.vertex_buffer_views.at("TANGENT").byte_offset) : 0,
					primitive.has("TEXCOORD_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("TEXCOORD_0").byte_offset) : 0,
					primitive.has("JOINTS_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("JOINTS_0").byte_offset) : 0,
					primitive.has("WEIGHTS_0") ? static_cast<UINT>(primitive.vertex_buffer_views.at("WEIGHTS_0").byte_offset) : 0,
					0,
					0,
				};
				//UINT offsets[_countof(vertex_buffers)]{ 0 };
				immediate_context->IASetVertexBuffers(0, _countof(vertex_buffers), vertex_buffers, strides, offsets);


				PrimitiveConstants primitive_data{};
				primitive_data.material = primitive.material;
				primitive_data.has_tangent = primitive.has("TANGENT");
				primitive_data.skin = node.skin;
				primitive_data.world = node.global_transform;
				//DirectX::XMStoreFloat4x4(&primitive_data.world, XMLoadFloat4x4(&node.global_transform) * XMLoadFloat4x4(&world));
				immediate_context->UpdateSubresource(primitive_cbuffer.Get(), 0, 0, &primitive_data, 0, 0);
				immediate_context->VSSetConstantBuffers(static_cast<UINT>(ConstantBufferSlot::kPerObject), 1, primitive_cbuffer.GetAddressOf());
				immediate_context->PSSetConstantBuffers(static_cast<UINT>(ConstantBufferSlot::kPerObject), 1, primitive_cbuffer.GetAddressOf());

				const Material& material{ materials_.at(primitive.material) };
				const int texture_indices[]
				{
					material.data.pbr_metallic_roughness.basecolor_texture.index,
					material.data.pbr_metallic_roughness.metallic_roughness_texture.index,
					material.data.normal_texture.index,
					material.data.emissive_texture.index,
					material.data.occlusion_texture.index,
				};
				ID3D11ShaderResourceView* null_shader_resource_view{};
				std::vector<ID3D11ShaderResourceView*> shader_resource_views(_countof(texture_indices));
				for (int texture_index = 0; texture_index < shader_resource_views.size(); ++texture_index)
				{
					shader_resource_views.at(texture_index) = texture_indices[texture_index] > -1
						? texture_resource_views_.at(textures_.at(texture_indices[texture_index]).source).Get()
						: null_shader_resource_view;
				}
				immediate_context->PSSetShaderResources(1, static_cast<UINT>(shader_resource_views.size()), shader_resource_views.data());

				if (primitive.index_buffer_view.count > 0)
				{
					if (primitive.index_buffer_view.format == DXGI_FORMAT_R8_UINT)
					{
						immediate_context->IASetIndexBuffer(
							buffers_.at(primitive.index_buffer_view.buffer).Get(),
							DXGI_FORMAT_R16_UINT, static_cast<UINT>(primitive.index_buffer_view.byte_offset));

					}
					else
					{

					immediate_context->IASetIndexBuffer(
						buffers_.at(primitive.index_buffer_view.buffer).Get(),
						primitive.index_buffer_view.format, static_cast<UINT>(primitive.index_buffer_view.byte_offset));

					}

					immediate_context->DrawIndexedInstanced(static_cast<UINT>(primitive.index_buffer_view.count), 
						instance_count, 0,0,start_instance_location);
				}
				else
				{
					immediate_context->Draw(static_cast<UINT>(primitive.vertex_buffer_views.at("POSITION").count), 0);
				}
			}
		}
		for (std::vector<int>::value_type child_index : node.children)
		{
			traverse(child_index);
		}
	} };
	for (std::vector<int>::value_type node_index : scenes.at(default_scene_).nodes)
	{
		traverse(node_index);
	}
}

void GltfModel::FetchMaterials(ID3D11Device* device, const tinygltf::Model& gltf_model)
{
	for (std::vector<tinygltf::Material>::const_reference gltf_material : gltf_model.materials)
	{
		std::vector<Material>::reference material = materials_.emplace_back();

		material.name = gltf_material.name;

		material.data.emissive_factor[0] = static_cast<float>(gltf_material.emissiveFactor.at(0));
		material.data.emissive_factor[1] = static_cast<float>(gltf_material.emissiveFactor.at(1));
		material.data.emissive_factor[2] = static_cast<float>(gltf_material.emissiveFactor.at(2));

		material.data.alpha_mode = gltf_material.alphaMode == "OPAQUE" ? 0 : gltf_material.alphaMode == "MASK" ? 1 : gltf_material.alphaMode == "BLEND" ? 2 : 0;
		material.data.alpha_cutoff = static_cast<float>(gltf_material.alphaCutoff);
		material.data.double_sided = gltf_material.doubleSided ? 1 : 0;

		material.data.pbr_metallic_roughness.basecolor_factor[0] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(0));
		material.data.pbr_metallic_roughness.basecolor_factor[1] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(1));
		material.data.pbr_metallic_roughness.basecolor_factor[2] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(2));
		material.data.pbr_metallic_roughness.basecolor_factor[3] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(3));
		material.data.pbr_metallic_roughness.basecolor_texture.index = gltf_material.pbrMetallicRoughness.baseColorTexture.index;
		material.data.pbr_metallic_roughness.basecolor_texture.texcoord = gltf_material.pbrMetallicRoughness.baseColorTexture.texCoord;
		material.data.pbr_metallic_roughness.metallic_factor = static_cast<float>(gltf_material.pbrMetallicRoughness.metallicFactor);
		material.data.pbr_metallic_roughness.roughness_factor = static_cast<float>(gltf_material.pbrMetallicRoughness.roughnessFactor);
		material.data.pbr_metallic_roughness.metallic_roughness_texture.index = gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index;
		material.data.pbr_metallic_roughness.metallic_roughness_texture.texcoord = gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;

		material.data.normal_texture.index = gltf_material.normalTexture.index;
		material.data.normal_texture.texcoord = gltf_material.normalTexture.texCoord;
		material.data.normal_texture.scale = static_cast<float>(gltf_material.normalTexture.scale);

		material.data.occlusion_texture.index = gltf_material.occlusionTexture.index;
		material.data.occlusion_texture.texcoord = gltf_material.occlusionTexture.texCoord;
		material.data.occlusion_texture.strength = static_cast<float>(gltf_material.occlusionTexture.strength);

		material.data.emissive_texture.index = gltf_material.emissiveTexture.index;
		material.data.emissive_texture.texcoord = gltf_material.emissiveTexture.texCoord;
	}

	// Create material data as shader resource view on GPU
	std::vector<Material::Cbuffer> material_data;
	for (std::vector<Material>::const_reference material : materials_)
	{
		material_data.emplace_back(material.data);
	}

	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D11Buffer> material_buffer;
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(Material::Cbuffer) * material_data.size());
	buffer_desc.StructureByteStride = sizeof(Material::Cbuffer);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	D3D11_SUBRESOURCE_DATA subresource_data{};
	subresource_data.pSysMem = material_data.data();
	hr = device->CreateBuffer(&buffer_desc, &subresource_data, material_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
	shader_resource_view_desc.Format = DXGI_FORMAT_UNKNOWN;
	shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	shader_resource_view_desc.Buffer.NumElements = static_cast<UINT>(material_data.size());
	hr = device->CreateShaderResourceView(material_buffer.Get(), &shader_resource_view_desc, material_resource_view_.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}
void GltfModel::FetchTextures(ID3D11Device* device, const tinygltf::Model& gltf_model)
{
	HRESULT hr{ S_OK };
	for (const tinygltf::Texture& gltf_texture : gltf_model.textures)
	{
		Texture& texture{ textures_.emplace_back() };
		texture.name = gltf_texture.name;
		texture.source = gltf_texture.source;
	}
	for (const tinygltf::Image& gltf_image : gltf_model.images)
	{
		Image& image{ images_.emplace_back() };
		image.name = gltf_image.name;
		image.width = gltf_image.width;
		image.height = gltf_image.height;
		image.component = gltf_image.component;
		image.bits = gltf_image.bits;
		image.pixel_type = gltf_image.pixel_type;
		image.buffer_view = gltf_image.bufferView;
		image.mime_type = gltf_image.mimeType;
		image.uri = gltf_image.uri;
		image.as_is = gltf_image.as_is;

		if (gltf_image.bufferView > -1)
		{
			const tinygltf::BufferView& buffer_view{ gltf_model.bufferViews.at(gltf_image.bufferView) };
			const tinygltf::Buffer& buffer{ gltf_model.buffers.at(buffer_view.buffer) };
			const byte* data = buffer.data.data() + buffer_view.byteOffset;

			ID3D11ShaderResourceView* texture_resource_view{};
			hr = LoadTexture::LoadTextureFromMemory(device, data, buffer_view.byteLength, &texture_resource_view);
			if (hr == S_OK)
			{
				texture_resource_views_.emplace_back().Attach(texture_resource_view);
			}
		}
		else
		{
			const std::filesystem::path path(filename_);
			ID3D11ShaderResourceView* shader_resource_view{};
			D3D11_TEXTURE2D_DESC texture2d_desc;
			std::wstring filename{ path.parent_path().concat(L"/").wstring() + std::wstring(gltf_image.uri.begin(), gltf_image.uri.end()) };
			hr = LoadTexture::LoadTextureFromFile(device, filename.c_str(), &shader_resource_view, &texture2d_desc);
			if (hr == S_OK)
			{
				texture_resource_views_.emplace_back().Attach(shader_resource_view);
			}
		}
	}
}

void GltfModel::FetchAnimations(const tinygltf::Model& gltf_model)
{
	using namespace std;
	using namespace tinygltf;
	using namespace DirectX;

	for (vector<tinygltf::Skin>::const_reference transmission_skin : gltf_model.skins)
	{
		Skin& skin{ skins_.emplace_back() };
		const Accessor& gltf_accessor{ gltf_model.accessors.at(transmission_skin.inverseBindMatrices) };
		const tinygltf::BufferView& gltf_buffer_view{ gltf_model.bufferViews.at(gltf_accessor.bufferView) };
		_ASSERT_EXPR(gltf_accessor.type == TINYGLTF_TYPE_MAT4, L"");

		skin.inverse_bind_matrices.resize(gltf_accessor.count);
		memcpy(skin.inverse_bind_matrices.data(), gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(XMFLOAT4X4));

		skin.joints = transmission_skin.joints;
	}

	for (vector<tinygltf::Animation>::const_reference gltf_animation : gltf_model.animations)
	{
		Animation& animation{ animations_.emplace_back() };
		animation.name = gltf_animation.name;
		for (vector<AnimationSampler>::const_reference gltf_sampler : gltf_animation.samplers)
		{
			Animation::Sampler& sampler{ animation.samplers.emplace_back() };
			sampler.input = gltf_sampler.input;
			sampler.output = gltf_sampler.output;
			sampler.interpolation = gltf_sampler.interpolation;

			const Accessor& gltf_accessor{ gltf_model.accessors.at(gltf_sampler.input) };
			const tinygltf::BufferView& gltf_buffer_view{ gltf_model.bufferViews.at(gltf_accessor.bufferView) };
			_ASSERT_EXPR(gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
			_ASSERT_EXPR(gltf_accessor.type == TINYGLTF_TYPE_SCALAR, L"");
			const pair<unordered_map<int, vector<float>>::iterator, bool>& timelines{ animation.timelines.emplace(gltf_sampler.input, gltf_accessor.count) };
			if (timelines.second)
			{
				memcpy(timelines.first->second.data(), gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(FLOAT));
			}
		}
		for (vector<AnimationChannel>::const_reference gltf_channel : gltf_animation.channels)
		{
			Animation::Channel& channel{ animation.channels.emplace_back() };
			channel.sampler = gltf_channel.sampler;
			channel.target_node = gltf_channel.target_node;
			channel.target_path = gltf_channel.target_path;

			const AnimationSampler& gltf_sampler{ gltf_animation.samplers.at(gltf_channel.sampler) };
			const Accessor& gltf_accessor{ gltf_model.accessors.at(gltf_sampler.output) };
			const tinygltf::BufferView& gltf_buffer_view{ gltf_model.bufferViews.at(gltf_accessor.bufferView) };
			if (gltf_channel.target_path == "scale")
			{
				_ASSERT_EXPR(gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
				_ASSERT_EXPR(gltf_accessor.type == TINYGLTF_TYPE_VEC3, L"");

				const pair<unordered_map<int, vector<XMFLOAT3>>::iterator, bool>& scales{ animation.scales.emplace(gltf_sampler.output, gltf_accessor.count) };
				if (scales.second)
				{
					memcpy(scales.first->second.data(), gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(XMFLOAT3));
				}
			}
			else if (gltf_channel.target_path == "rotation")
			{
				_ASSERT_EXPR(gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
				_ASSERT_EXPR(gltf_accessor.type == TINYGLTF_TYPE_VEC4, L"");

				const pair<unordered_map<int, vector<XMFLOAT4>>::iterator, bool>& rotations{ animation.rotations.emplace(gltf_sampler.output, gltf_accessor.count) };
				if (rotations.second)
				{
					memcpy(rotations.first->second.data(), gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(XMFLOAT4));
				}
			}
			else if (gltf_channel.target_path == "translation")
			{
				_ASSERT_EXPR(gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
				_ASSERT_EXPR(gltf_accessor.type == TINYGLTF_TYPE_VEC3, L"");
				const pair<unordered_map<int, vector<XMFLOAT3>>::iterator, bool>& translations{ animation.translations.emplace(gltf_sampler.output, gltf_accessor.count) };
				if (translations.second)
				{
					memcpy(translations.first->second.data(), gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(XMFLOAT3));
				}
			}
			else if (gltf_channel.target_path == "weights")
			{
				//_ASSERT_EXPR(FALSE, L"");
			}
			else
			{
				_ASSERT_EXPR(FALSE, L"");
			}
		}
	}
	// Find a longest animation duration in timeline of each channel.
	for (decltype(animations_)::reference animation : animations_)
	{
		for (decltype(animation.timelines)::reference timelines : animation.timelines)
		{
			animation.duration = std::max<float>(animation.duration, timelines.second.back());
		}
	}
}
void GltfModel::Animate(size_t animation_index, float time, std::vector<Node>& animated_nodes)
{
	using namespace std;
	using namespace DirectX;

	_ASSERT_EXPR(animations_.size() > animation_index, L"");
	_ASSERT_EXPR(animated_nodes.size() == nodes_.size(), L"");

	function<size_t(const vector<float>&, float, float&)> indexof{ [](const vector<float>& timelines, float time, float& interpolation_factor)->size_t {
		const size_t keyframe_count{ timelines.size() };
		if (time > timelines.at(keyframe_count - 1))
		{
			interpolation_factor = 1.0f;
			return keyframe_count - 2;
		}
		else if (time < timelines.at(0))
		{
			interpolation_factor = 0.0f;
			return 0;
		}
		size_t keyframe_index{ 0 };
		for (size_t time_index = 1; time_index < keyframe_count; ++time_index)
		{
			if (time < timelines.at(time_index))
			{
				keyframe_index = max<size_t>(0LL, time_index - 1);
				break;
			}
		}
		interpolation_factor = (time - timelines.at(keyframe_index + 0)) / (timelines.at(keyframe_index + 1) - timelines.at(keyframe_index + 0));
		return keyframe_index;
	} };

	if (animations_.size() > 0)
	{
		const Animation& animation{ animations_.at(animation_index) };
		for (vector<Animation::Channel>::const_reference channel : animation.channels)
		{
			const Animation::Sampler& sampler{ animation.samplers.at(channel.sampler) };
			const vector<float>& timeline{ animation.timelines.at(sampler.input) };
			if (timeline.size() == 0)
			{
				continue;
			}
			float interpolation_factor{};
			size_t keyframe_index{ indexof(timeline, time, interpolation_factor) };
			if (channel.target_path == "scale")
			{
				const vector<XMFLOAT3>& scales{ animation.scales.at(sampler.output) };
				XMStoreFloat3(&animated_nodes.at(channel.target_node).scale, XMVectorLerp(XMLoadFloat3(&scales.at(keyframe_index + 0)), XMLoadFloat3(&scales.at(keyframe_index + 1)), interpolation_factor));
			}
			else if (channel.target_path == "rotation")
			{
				const vector<XMFLOAT4>& rotations{ animation.rotations.at(sampler.output) };
				XMStoreFloat4(&animated_nodes.at(channel.target_node).rotation, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&rotations.at(keyframe_index + 0)), XMLoadFloat4(&rotations.at(keyframe_index + 1)), interpolation_factor)));
			}
			else if (channel.target_path == "translation")
			{
				const vector<XMFLOAT3>& translations{ animation.translations.at(sampler.output) };
				XMStoreFloat3(&animated_nodes.at(channel.target_node).translation, XMVectorLerp(XMLoadFloat3(&translations.at(keyframe_index + 0)), XMLoadFloat3(&translations.at(keyframe_index + 1)), interpolation_factor));
			}
		}
		CumulateTransforms(animated_nodes);
	}
}

void GltfModel::UpdateAnimation(float elapsed_time)
{
	if (animations_.size() > 0)
	{
		Animate(0, animation_time_ += elapsed_time, animated_nodes_);
		if (animations_.at(0).duration < animation_time_)
		{
			animation_time_ = 0; // Repeat playback
		}
	}
}

const std::vector<GltfModel::Node>& GltfModel::GetNodes() const
{
	return nodes_;
}

const std::vector<GltfModel::Mesh>& GltfModel::GetMeshes() const
{
	return meshes_;
}

const std::vector<GltfModel::Material>& GltfModel::GetMaterials() const
{
	return materials_;
}

const std::vector<GltfModel::Animation>& GltfModel::GetAnimations() const
{
	return animations_;
}
