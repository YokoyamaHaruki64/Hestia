#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct YAMLValue;
struct YAMLValueConversionContext;
class Component;
class GameObject;

// ファイルから復元したObject群をManagerへ移管するまで一時所有する結果。
struct GameObjectLoadResult
{
    std::string name;
    std::vector<std::unique_ptr<GameObject>> gameObjects;

    GameObjectLoadResult() = default;
    ~GameObjectLoadResult();
    GameObjectLoadResult(GameObjectLoadResult&&) noexcept;
    GameObjectLoadResult& operator=(GameObjectLoadResult&&) noexcept;
    GameObjectLoadResult(const GameObjectLoadResult&) = delete;
    GameObjectLoadResult& operator=(const GameObjectLoadResult&) = delete;
};

enum class PrefabReferenceSaveMode
{
    Include,
    Exclude,
};

enum class PrefabSourcePathMode
{
    UseSerializedPath,
    UseSourceFilePath,
};

// SceneとPrefabで共用するGameObject階層のYAML変換を担当する。
// 生成、Field復元、親子・参照解決をOnStart前に完了させる。
class GameObjectYamlSerializer final
{
public:
    static bool SaveToFile(
        const std::filesystem::path& path,
        std::string_view name,
        const std::vector<GameObject*>& gameObjects,
        PrefabReferenceSaveMode prefabReferenceMode);

    static bool LoadFromFile(
        const std::filesystem::path& path,
        GameObjectLoadResult& output,
        PrefabSourcePathMode sourcePathMode);

private:
    static constexpr uint32_t FileVersion = 1;
    using ObjectId = uint64_t;

    struct PendingComponent
    {
        Component* component{};
        const YAMLValue* fields{};
    };

    struct PendingParent
    {
        GameObject* child{};
        std::optional<ObjectId> parentId;
    };

    // 必須キーの検証を共通化し、破損したScene/Prefab文書を早期に検出する。
    static bool ReadRequiredString(const YAMLValue& map, std::string_view key, std::string& output);

    template <typename T>
    static bool ReadRequiredValue(const YAMLValue& map, std::string_view key, T& output);

    // YAMLValueのコピーを返すYamlValueConverterのラッパー
    template <typename T>
    static YAMLValue ToYAMLValue(const T& value);

    // Componentの型名とフィールドをYAMLValueに変換する。失敗時はNullを返す。
    static YAMLValue ComponentToYAMLValue(Component& component, const YAMLValueConversionContext& context);

    // Componentを生成し、Field復元のためPendingComponentに登録する。失敗時はfalseを返す。
    static bool CreateComponentsFromYAMLValue(
        const YAMLValue& source,
        GameObject& gameObject,
        std::vector<PendingComponent>& pendingComponents);
};
