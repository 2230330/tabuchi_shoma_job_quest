#pragma once
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <any>
#include <set>
#include<stdint.h>

//特定のコンポーネントの組み合わせを持つエンティティの集合構造体
class Archetype {
public:
    //保存するコンポーネントの組み合わせを記録するコンストラクタ
    Archetype(const std::set<std::type_index>& types);

    //このアーチタイプに新しいエンティティIDを追加する
    uint32_t AddEntity(uint32_t entity_id);
    //このアーチタイプに新しいコンポーネントを追加する
    template<typename T>
    void AddComponent(uint32_t index, const T& value)
    {
        //送られてきたコンポーネントのタイプを判別
        auto& array_any = component_arrays_[typeid(T)];
        if (!array_any.has_value())
        {
            array_any = std::vector<T>();
        }

        //送られてきたコンポーネントを格納
        auto& vec = std::any_cast<std::vector<T>&>(array_any);
        if (vec.size() <= index)
        {
            vec.resize(index + 1);
        }
        vec[index] = value;
    }

    //指定されたコンポーネントに対応する配列を返す
    template<typename T>
    std::vector<T>* GetComponentArray(std::type_index type)
    {
        auto it = component_arrays_.find(typeid(T));
        
        //配列に無ければ無効を返す
        if (it == component_arrays_.end())return nullptr;

        //配列にコンポーネントがあっても、中身が無かったら無効を返す
        if (!it->second.has_value())return nullptr;

        return std::any_cast<std::vector<T>>(&it->second);
    }

    //対象コンポーネントをもつ配列が存在するかどうかを調べる
    template<typename T>
    bool HasComponent()const{
        return types_.count(typeid(T)) > 0;
    }

    //このアーチタイプに属するエンティティIDの一覧を返す
    std::vector<uint32_t>& GetEntityList();

private:
    //このアーチタイプが管理するコンポーネント型の組み合わせを表す
    std::set<std::type_index> types_;
    //コンポーネント型毎に配列を保持するマップ
    std::unordered_map<std::type_index, std::any> component_arrays_; // vector<T> を any で保持
    //エンティティ一覧
    std::vector<uint32_t> entities_;
};