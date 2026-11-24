#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class GameObject;

// GameObject参照など、YAMLValue変換時だけ必要な外部解決処理を保持する。
struct YAMLValueConversionContext
{
    using ObjectId = uint64_t;

    std::function<std::optional<ObjectId>(const GameObject*)> getObjectId;
    std::function<GameObject*(ObjectId)> resolveObject;
    std::function<bool(std::string_view)> resolvePrefab;
};

enum class YAMLMessageLevel
{
    None,
    Warning,
    Error,
};

// C++値とYAMLValueの変換だけに対する結果。ファイルI/Oの成否は含めない。
struct YAMLValueConversionResult
{
    bool success = true;
    YAMLMessageLevel messageLevel = YAMLMessageLevel::None;
    std::string message;
};

// YAMLライブラリに依存しない中間表現。型変換処理とファイル形式を分離する
struct YAMLValue
{
    enum class CollectionStyle
    {
        Block,
        Flow,
    };

    enum class Type
    {
        Null,
        Scalar,
        Sequence,
        Map,
    };

    Type type = Type::Null;
    CollectionStyle collectionStyle = CollectionStyle::Block;
    std::string scalar;
    bool isString = false;
    std::vector<YAMLValue> sequence;
    std::vector<std::pair<std::string, YAMLValue>> map;

    static YAMLValue Null()
    {
        return {};
    }

    static YAMLValue Scalar(std::string value, bool stringValue = false)
    {
        YAMLValue result;
        result.type = Type::Scalar;
        result.scalar = std::move(value);
        result.isString = stringValue;
        return result;
    }

    static YAMLValue Sequence(CollectionStyle style = CollectionStyle::Block)
    {
        YAMLValue result;
        result.type = Type::Sequence;
        result.collectionStyle = style;
        return result;
    }

    static YAMLValue Map()
    {
        YAMLValue result;
        result.type = Type::Map;
        return result;
    }

    YAMLValue& Add(std::string key, YAMLValue value)
    {
        map.emplace_back(std::move(key), std::move(value));
        return map.back().second;
    }

    const YAMLValue* Find(std::string_view key) const
    {
        for (const auto& [entryKey, value] : map)
        {
            if (entryKey == key)
            {
                return &value;
            }
        }
        return nullptr;
    }
};
