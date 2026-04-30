#pragma once
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <typeindex>
#include <stdexcept>
#include<functional>

#include "component_position.h"
#include "component_rotation.h"
#include "component_scale.h"
#include "component_local_to_world.h"
#include "component_color.h"
#include "component_gltf.h"
#include "component_mesh.h"
#include "component_primitive.h"
#include "component_material.h"
#include "component_texture.h"
#include "component_instanced.h"
#include "component_sky_atmosphere.h"
#include "component_volumetric_cloud.h"
#include"component_ajast_pbr_paramter_.h"
#include "component_camera.h"
#include "component_screen_space_reflection.h"
#include"component_name.h"

//�R���|�[�l���g�̊Ǘ��ҁB���ꂩ��Ԃ��Ԃ��傫���Ȃ�ƍl����Ƃ�����ƔY�ݕ�
class ComponentManager 
{
public:
    ComponentManager() {
        // �R���X�g���N�^�Ō^���Ƃ̃X�g���[�W��o�^���Ă���
        registerContainer<ComponentPosition>(positions_);
        registerContainer<ComponentRotation>(rotations_);
        registerContainer<ComponentScale>(scales_);
        registerContainer<ComponentLocalToWorld>(l2ws_);
        registerContainer<ComponentColor>(colors_);
        registerContainer<ComponentGltf>(gltfs_);
        registerContainer<ComponentMesh>(meshes_);
        registerContainer<ComponentPrimitive>(primitives_);
        registerContainer<ComponentMaterial>(materials_);
        registerContainer<ComponentTexture>(textures_);
        registerContainer<ComponentInstanced>(instanced_);
        registerContainer<ComponentSkyAtmosphere>(skys_);
        registerContainer<ComponentVolumetricCloud>(clouds_);
        registerContainer<ComponentAdjastPbrParamter>(ajast_pbr_paramters_);
        registerContainer<ComponentCamera>(cameras_);
        registerContainer<ComponentSsr>(ssrs_);
        registerContainer<ComponentName>(names_);
    }

    template<typename T>
    int Add(const T& component) {
        auto& container = getContainer<T>();
        container.push_back(component);
        return static_cast<int>(container.size() - 1);
    }

    template<typename T>
    int Add(uint32_t entity_id, const T& component) {
        auto& container = getContainer<T>();
        container.emplace_back(component);
        int id = static_cast<int>(container.size() - 1);
        entity_to_component_[std::type_index(typeid(T))][entity_id] = id;
        return id;
    }

    //�R���|�[�l���g��i�[���Ă���z��̗v�f�Ŏ��o���Q�b�^�[
    template<typename T>
    T& Get(int id) {
        auto& container = getContainer<T>();
        return container.at(id);
    }

    template<typename T>
    const T& Get(int id) const {
        const auto& container = getContainer<T>();
        return container.at(id);
    }

    //�v�f�̍폜�֐�
    template<typename T>
    void Remove(uint32_t entity_id) {
        auto type = std::type_index(typeid(T));
        auto mit = entity_to_component_.find(type);
        if (mit == entity_to_component_.end()) return;

        auto& mapping = mit->second;
        auto it = mapping.find(entity_id);
        if (it == mapping.end()) return;

        int index_to_remove = it->second;
        auto& container = getContainer<T>();
        int last_index = static_cast<int>(container.size() - 1);

        if (index_to_remove != last_index) {
            // swap with last
            std::swap(container[index_to_remove], container[last_index]);

            // �X�V�Ώۂ� entity ��T��
            for (auto& [eid, idx] : mapping) {
                if (idx == last_index) {
                    idx = index_to_remove;
                    break;
                }
            }
        }

        container.pop_back();
        mapping.erase(entity_id);
    }

    //�ꏏ�ɓo�^�����G���e�B�e�B�ŗv�f����o���Q�b�^�[
    template<typename T>
    T& GetByEntity(uint32_t entity_id) {
        auto& container = getContainer<T>();
        auto& mapping = entity_to_component_[std::type_index(typeid(T))];
        return container.at(mapping.at(entity_id));
    }

    //�o�^�����R���|�[�l���g���Ȃ��ꍇ�Ȃǂ̈��S�ŃQ�b�^�[
    template<typename T>
    T* TryGetByEntity(uint32_t entity_id) {
        auto it = entity_to_component_.find(std::type_index(typeid(T)));
        if (it == entity_to_component_.end()) return nullptr;

        auto& mapping = it->second;
        auto mit = mapping.find(entity_id);
        if (mit == mapping.end()) return nullptr;

        auto& container = getContainer<T>();
        return &container.at(mit->second);
    }
    //�G���e�B�e�B�����̃R���|�[�l���g����L���Ă��邩�̊m�F
    template<typename T>
    inline bool Has(uint32_t entity_id) 
    {
        auto it = entity_to_component_.find(std::type_index(typeid(T)));
        if (it == entity_to_component_.end()) return false;
        return it->second.find(entity_id) != it->second.end();
    }

    //����̃R���|�[�l���g����G���e�B�e�B�ɑ΂��Ĉꊇ���������ׂ̑����֐�
    template<typename T>
    void ForEach(std::function<void(uint32_t, T&)> func) {
        auto& container = getContainer<T>();
        auto& mapping = entity_to_component_[std::type_index(typeid(T))];
        for (const auto& [entity_id, index] : mapping) {
            func(entity_id, container[index]);
        }
    }


    //�R���|�[�l���g�̎�����̃G���e�B�e�B�����񂾂Ƃ��A������R���|�[�l���g�����
    void RemoveAllComponents(uint32_t entity_id) {
        for (auto& [type, remove_fn] : removers_)
        {
            remove_fn(entity_id);
        }

    }

private:
    // �^���Ƃ̃R���e�i��ėp�I�Ɉ������߂̎d�g��
    template<typename T>
    void registerContainer(std::vector<T>& vec) {
        containers_[std::type_index(typeid(T))] = &vec;

        //remover��o�^
        removers_[std::type_index(typeid(T))] = [this](uint32_t eid)
            {
                this->Remove<T>(eid);
            };
    }

    template<typename T>
    std::vector<T>& getContainer() {
        auto it = containers_.find(std::type_index(typeid(T)));
        if (it == containers_.end()) {
            throw std::runtime_error("�R���|�[�l���g���o�^����Ă��܂���");
        }
        return *static_cast<std::vector<T>*>(it->second);
    }

    template<typename T>
    const std::vector<T>& getContainer() const {
        auto it = containers_.find(std::type_index(typeid(T)));
        if (it == containers_.end()) {
            throw std::runtime_error("�R���|�[�l���g���o�^����Ă��܂���");
        }
        return *static_cast<const std::vector<T>*>(it->second);
    }

private:
    std::unordered_map<std::type_index, void*> containers_;
    //�^���Ƃ̃}�b�s���O�@�\
    std::unordered_map<std::type_index, std::unordered_map<uint32_t, int>> entity_to_component_;
    std::unordered_map<std::type_index, std::function<void(uint32_t)>> removers_;

    std::vector<ComponentPosition> positions_;
    std::vector<ComponentRotation> rotations_;
    std::vector<ComponentScale> scales_;
    std::vector<ComponentLocalToWorld> l2ws_;
    std::vector<ComponentColor> colors_;
    std::vector<ComponentGltf> gltfs_;
    std::vector<ComponentMesh>meshes_;
    std::vector<ComponentPrimitive>primitives_;
    std::vector<ComponentMaterial>materials_;
    std::vector<ComponentTexture>textures_;
    std::vector<ComponentInstanced>instanced_;
    std::vector<ComponentSkyAtmosphere>skys_;
    std::vector<ComponentVolumetricCloud>clouds_;
    std::vector<ComponentAdjastPbrParamter>ajast_pbr_paramters_;
    std::vector<ComponentCamera>cameras_;
    std::vector<ComponentSsr>ssrs_;
    std::vector<ComponentName>names_;
};
