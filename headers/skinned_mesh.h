#pragma once
//Unit17
#include<d3d11.h>
#include<wrl.h>
#include<DirectXMath.h>
#include<vector>
#include<string>
#include<fbxsdk.h>
//Unit18
#include<unordered_map>
//Unit30
#include<cereal/archives/binary.hpp>
#include<cereal/types/memory.hpp>
#include<cereal/types/vector.hpp>
#include<cereal/types/set.hpp>
#include<cereal/types/unordered_map.hpp>

//DirectXのXMFLOAT系の構造体をCerealでシリアライズする

namespace DirectX
{
    template<class T>
    void serialize(T& archive, DirectX::XMFLOAT2& v)
    {
        archive(
            cereal::make_nvp("x", v.x),
            cereal::make_nvp("y", v.y)
        );
    }

    template<class T>
    void serialize(T& archive, DirectX::XMFLOAT3& v)
    {
        archive(
            cereal::make_nvp("x", v.x),
            cereal::make_nvp("y", v.y),
            cereal::make_nvp("z", v.z)
        );
    }

    template<class T>
    void serialize(T& archive, DirectX::XMFLOAT4& v)
    {
        archive(
            cereal::make_nvp("x", v.x),
            cereal::make_nvp("y", v.y),
            cereal::make_nvp("z", v.z),
            cereal::make_nvp("w", v.w)
        );
    }

    template<class T>
    void serialize(T& archive, DirectX::XMFLOAT4X4& m)
    {
        archive(
            cereal::make_nvp("_11", m._11), cereal::make_nvp("_12", m._12),
            cereal::make_nvp("_13", m._13), cereal::make_nvp("_14", m._14),
            cereal::make_nvp("_21", m._21), cereal::make_nvp("_22", m._22),
            cereal::make_nvp("_23", m._23), cereal::make_nvp("_24", m._24),
            cereal::make_nvp("_31", m._31), cereal::make_nvp("_32", m._32),
            cereal::make_nvp("_33", m._33), cereal::make_nvp("_34", m._34),
            cereal::make_nvp("_41", m._41), cereal::make_nvp("_42", m._42),
            cereal::make_nvp("_43", m._43), cereal::make_nvp("_44", m._44)
        );
    }
}

struct Skeleton

{
    struct Bone
    {
        uint64_t unique_id{ 0 };
        std::string name;
        //'parent_index' is index that refers to the parent bone's position in the array that contains itself.
        int64_t parent_index{ -1 };  // -1 : the bone is orphan
        //'node_index' is an index that refers to the node array of the scene.
        int64_t node_index{ 0 };

        //'offset_transform' is used to convert from model(mesh) space to bone(node) scene.
        DirectX::XMFLOAT4X4 offset_transform{ 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 };

        bool IsOrphan() const { return parent_index < 0; }

        //Unit30
        template<class T>
        void serialize(T& archive)
        {
            archive(unique_id, name, parent_index, node_index, offset_transform);
        }
    };
    std::vector<Bone> bones;
    int64_t Indexof(uint64_t unique_id)const
    {
        int64_t index{ 0 };
        for (const Bone& bone : bones)
        {
            if (bone.unique_id == unique_id)
            {
                return index;
            }
            ++index;
        }
        return -1;
    }

    template<class T>
    void serialize(T& archive)
    {
        archive(bones);
    }
};
struct Animation
{
    std::string name;
    float sampling_rate{ 0 };

    struct KeyFrame
    {
        struct Node
        {
            //'global_transform' is used to convert from local space of node to global space of scene.
            DirectX::XMFLOAT4X4 global_transform{ 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 };

            //Unit27
            //The transformation data of a node includes its translation,rotation and scaling vectors
            //with respect to its parent.
            DirectX::XMFLOAT3 scaling{ 1,1,1 };
            DirectX::XMFLOAT4 rotation{ 0,0,0,1 };  //Rotation quaternion
            DirectX::XMFLOAT3 translation{ 0,0,0 };

            template<class T>
            void serialize(T& archive)
            {
                archive(global_transform, scaling, rotation, translation);
            }
        };
        std::vector<Node>nodes;

        template<class T>
        void serialize(T& archive)
        {
            archive(nodes);
        }
    };
    std::vector<KeyFrame> sequence;

    template<class T>
    void serialize(T& archive)
    {
        archive(name, sampling_rate, sequence);
    }
};
struct SceneGraph
{
    struct Node
    {
        uint64_t unique_id{ 0 };
        std::string name;
        FbxNodeAttribute::EType attribute{ FbxNodeAttribute::EType::eUnknown };
        int64_t parent_index{ -1 };

        template<class T>
        void serialize(T& archive)
        {
            archive(unique_id, name, attribute, parent_index);
        }
    };
    std::vector<Node> nodes;
    int64_t Indexof(uint64_t unique_id)const
    {
        int64_t index{ 0 };
        for (const Node& node : nodes)
        {
            if (node.unique_id == unique_id)
            {
                return index;
            }
            ++index;
        }
        return -1;
    }

    template <class T>
    void serialize(T& archive)
    {
        archive(nodes);
    }
};

//FBX形式ファイルのモデル
class SkinnedMesh
{
private:
    static const int MAX_BONE_INFLUENCES{ 4 };
    struct Vertex
    {
        DirectX::XMFLOAT3   position;
        DirectX::XMFLOAT3   normal{ 0,1,0 };
        DirectX::XMFLOAT4   tangent{ 1,0,0,1 };
        DirectX::XMFLOAT2   texcoord{ 0,0 };
        float               bone_weights[MAX_BONE_INFLUENCES]{ 1,0,0,0 };
        uint32_t            bone_indices[MAX_BONE_INFLUENCES]{};

        template <class T>
        void serialize(T& archive)
        {
            archive(position, normal, tangent, texcoord, bone_weights, bone_indices);
        }
    };
    static const int MAX_BONES{ 256 };
    struct Constants
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4   material_color;
        DirectX::XMFLOAT4X4 bone_transforms[MAX_BONES]
        {
            {
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1,
            }
        };
    };
    struct Mesh
    {
        uint64_t unique_id{ 0 };
        std::string name;
        //'node_index' is an index that refers to the node array of the scene;
        int64_t node_index{ 0 };

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        struct Subset
        {
            uint64_t material_unique_id{ 0 };
            std::string material_name;

            uint32_t start_index_location{ 0 };
            uint32_t index_count{ 0 };

            template <class T>
            void serialize(T& archive)
            {
                archive(material_unique_id, material_name, start_index_location, index_count);
            }
        };
        std::vector<Subset>subsets;

        DirectX::XMFLOAT4X4 default_global_transform
        {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        };
        Skeleton bind_pose;
        DirectX::XMFLOAT3 bounding_box[2]
        {
            {+D3D11_FLOAT32_MAX,+D3D11_FLOAT32_MAX,+D3D11_FLOAT32_MAX},
            {-D3D11_FLOAT32_MAX,-D3D11_FLOAT32_MAX,-D3D11_FLOAT32_MAX}
        };

        template <class T>
        void serialize(T& archive)
        {
            archive(unique_id, name, node_index, subsets, default_global_transform,
                bind_pose, bounding_box, vertices, indices);
        }
    private:
        Microsoft::WRL::ComPtr<ID3D11Buffer>vertex_buffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>index_buffer;
        //friend=>https://learn.microsoft.com/ja-jp/cpp/cpp/friend-cpp?view=msvc-170&devlangs=cpp&f1url=%3FappId%3DDev16IDEF1%26l%3DJA-JP%26k%3Dk(friend_CPP)%3Bk(friend)%3Bk(DevLang-C%252B%252B)%3Bk(TargetOS-Windows)%26rd%3Dtrue
        friend class SkinnedMesh;
    };
    struct Material
    {
        uint64_t unique_id{ 0 };
        std::string name;

        DirectX::XMFLOAT4 Ka{ 0.2f,0.2f,0.2f,1.0f };
        DirectX::XMFLOAT4 Kd{ 0.8f,0.8f,0.8f,1.0f };
        DirectX::XMFLOAT4 Ks{ 1.0f,1.0f,1.0f,1.0f };

        std::string texture_filenames[4];
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view[4];

        //Unit30
        template<class T>
        void serialize(T& archive)
        {
            archive(unique_id, name, Ka, Kd, Ks, texture_filenames);
        }
    };

    SceneGraph scene_view_;
    std::vector<Mesh>meshes_;
    std::unordered_map<uint64_t, Material> materials_;
    std::vector<Animation> animation_clips_;
    //d3d11
    Microsoft::WRL::ComPtr<ID3D11VertexShader>      vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       input_layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            constant_buffer_;
public:
    SkinnedMesh(ID3D11Device* device, const wchar_t* fbx_filename, bool trianglate = false, int sampling_rate = 0);
    virtual ~SkinnedMesh() = default;
    void Render(ID3D11DeviceContext* immediate_context,
        const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4& material_color, const Animation::KeyFrame* keyframe = nullptr);
    Animation& GetAnimationClip(int clip_index) { return animation_clips_.at(clip_index); }

private:
    void FetchScene(const wchar_t* fbx_filename, bool trianglate, int sampling_rate);
    void FetchMeshes(FbxScene* fbx_scene, std::vector<Mesh>& meshes);
    void CreateComObjects(ID3D11Device* device, const wchar_t* fbx_filename);
    void FetchMaterials(FbxScene* fbx_scene, std::unordered_map<uint64_t, Material>&);
    //std::unordered_map<uint64_t,material>&の実態を出さないようにする。
    void FetchSkeleton(FbxMesh* fbx_mesh, Skeleton& bind_pose);
    void FetchAnimations(FbxScene* fbx_scene, std::vector<Animation>& animation_clips,
        float sampling_rate/*If this value is 0,the animation data will be sampled at the default frame rate*/);
    void UpdateAnimation(Animation::KeyFrame& keyframe);
    bool UppendAnimations(const char* animation_filename, float sampling_rate);
    void BlendAnimations(const Animation::KeyFrame* keyframes[2], float factor, Animation::KeyFrame& keyframe);
};