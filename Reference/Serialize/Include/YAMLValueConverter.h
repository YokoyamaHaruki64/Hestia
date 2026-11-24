#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <system_error>
#include <tuple>
#include <vector>

#include "Common/Color.h"
#include "math/Matrix3x3.h"
#include "math/Matrix4x4.h"
#include "math/Curve.h"
#include "math/Quaternion.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "System/Core/GameObjectReference.h"
#include "System/Core/PrefabReference.h"
#include "System/Serialization/YAMLReflection.h"
#include "System/Serialization/YAMLTypes.h"

// 型TとYAMLValueの相互変換を提供する。YAMLテキストの入出力は担当しない。
template <typename T, typename Enable = void>
struct YAMLValueConverter;

// Vector、Colorなどの固定長数値列をYAML Sequenceとして保存するための変換処理を提供する
template <typename T>
struct YAMLFloatSequenceTraits;

// 数値列や診断メッセージなど、複数のCodecで共有する変換処理。
class YAMLValueConverterUtility final
{
public:
    static void MergeConversionMessage(YAMLValueConversionResult& destination, const YAMLValueConversionResult& source)
    {
        if (source.message.empty())
        {
            return;
        }

        if (!destination.message.empty())
        {
            destination.message += "; ";
        }
        destination.message += source.message;
        if (static_cast<int>(source.messageLevel) > static_cast<int>(destination.messageLevel))
        {
            destination.messageLevel = source.messageLevel;
        }
    }

    // 数値を文字列に変換してYAMLValue::Scalarとして保存する
    template <typename T>
    static YAMLValueConversionResult NumberToYAMLValue(T value, YAMLValue& output)
    {
        std::array<char, 64> buffer{};
        std::to_chars_result result;
        if constexpr (std::is_floating_point_v<T>)
        {
            result = std::to_chars(
                buffer.data(),
                buffer.data() + buffer.size(),
                value,
                std::chars_format::general,
                std::numeric_limits<T>::max_digits10);
        }
        else
        {
            result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        }

        if (result.ec != std::errc{})
        {
            return { false };
        }

        output = YAMLValue::Scalar(std::string(buffer.data(), result.ptr));
        return {};
    }

    // YAMLValue::Scalarから数値に変換する。std::from_charsを使用するため、整数は10進数表記のみ対応する。
    template <typename T>
    static YAMLValueConversionResult NumberFromYAMLValue(const YAMLValue& input, T& output)
    {
        if (input.type != YAMLValue::Type::Scalar)
        {
            return { false };
        }

        const char* begin = input.scalar.data();
        const char* end = begin + input.scalar.size();
        const std::from_chars_result result = std::from_chars(begin, end, output);
        return { result.ec == std::errc{} && result.ptr == end };
    }

    // floatの固定長配列をYAML Sequenceとして保存する
    // YAMLFloatSequenceTraits<T>::ToArrayでfloat配列に変換し、NumberToYAMLValueで各要素をScalarとして保存する
    // CountはYAMLFloatSequenceTraits<T>::Countから提供される
    template <typename T, size_t Count>
    static YAMLValueConversionResult FloatSequenceToYAMLValue(const T& value, YAMLValue& output)
    {
        const std::array<float, Count> components = ::YAMLFloatSequenceTraits<T>::ToArray(value);
        output = YAMLValue::Sequence(YAMLValue::CollectionStyle::Flow);
        output.sequence.reserve(Count);

        for (float component : components)
        {
            YAMLValue element;
            const YAMLValueConversionResult result = NumberToYAMLValue(component, element);
            if (!result.success)
            {
                return result;
            }
            output.sequence.emplace_back(std::move(element));
        }
        return {};
    }

    // YAML Sequenceからfloatの固定長配列に変換する
    template <typename T, size_t Count>
    static YAMLValueConversionResult FloatSequenceFromYAMLValue(const YAMLValue& input, T& output)
    {
        if (input.type != YAMLValue::Type::Sequence || input.sequence.size() != Count)
        {
            return { false };
        }

        std::array<float, Count> components{};
        for (size_t index = 0; index < Count; ++index)
        {
            const YAMLValueConversionResult result = NumberFromYAMLValue(input.sequence[index], components[index]);
            if (!result.success)
            {
                return result;
            }
        }

        // float配列からTに変換
        output = ::YAMLFloatSequenceTraits<T>::FromArray(components);
        return {};
    }
};

// 整数はScalarとして保存する。
template <typename T>
struct YAMLValueConverter<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
{
    static YAMLValueConversionResult ToYAMLValue(T value, YAMLValue& output, const YAMLValueConversionContext&)
    {
        return YAMLValueConverterUtility::NumberToYAMLValue(value, output);
    }

    static YAMLValueConversionResult FromYAMLValue(const YAMLValue& input, T& output, const YAMLValueConversionContext&)
    {
        return YAMLValueConverterUtility::NumberFromYAMLValue(input, output);
    }
};

// 浮動小数点はScalarとして保存する。
template <typename T>
struct YAMLValueConverter<T, std::enable_if_t<std::is_floating_point_v<T>>>
{
    static YAMLValueConversionResult ToYAMLValue(T value, YAMLValue& output, const YAMLValueConversionContext&)
    {
        return YAMLValueConverterUtility::NumberToYAMLValue(value, output);
    }

    static YAMLValueConversionResult FromYAMLValue(const YAMLValue& input, T& output, const YAMLValueConversionContext&)
    {
        return YAMLValueConverterUtility::NumberFromYAMLValue(input, output);
    }
};

template <>
struct YAMLValueConverter<bool>
{
    static YAMLValueConversionResult ToYAMLValue(bool value, YAMLValue& output, const YAMLValueConversionContext&)
    {
        output = YAMLValue::Scalar(value ? "true" : "false");
        return {};
    }

    static YAMLValueConversionResult FromYAMLValue(const YAMLValue& input, bool& output, const YAMLValueConversionContext&)
    {
        if (input.type != YAMLValue::Type::Scalar || (input.scalar != "true" && input.scalar != "false"))
        {
            return { false };
        }
        output = input.scalar == "true";
        return {};
    }
};

template <>
struct YAMLValueConverter<std::string>
{
    static YAMLValueConversionResult ToYAMLValue(const std::string& value, YAMLValue& output, const YAMLValueConversionContext&)
    {
        output = YAMLValue::Scalar(value, true);
        return {};
    }

    static YAMLValueConversionResult FromYAMLValue(const YAMLValue& input, std::string& output, const YAMLValueConversionContext&)
    {
        if (input.type != YAMLValue::Type::Scalar)
        {
            return { false };
        }
        output = input.scalar;
        return {};
    }
};

// --------------------------------------------------------------------------
// floatの固定長配列処理を提供するYAMLFloatSequenceTraitsの特殊化を定義
// --------------------------------------------------------------------------

#define YAML_FLOAT_SEQUENCE_TRAITS_COUNT(count) \
    static constexpr size_t Count = count;

template <>
struct YAMLFloatSequenceTraits<Vector2>
{
    YAML_FLOAT_SEQUENCE_TRAITS_COUNT(2)

    static std::array<float, Count> ToArray(const Vector2& value)
    {
        return { value.x, value.y };
    }

    static Vector2 FromArray(const std::array<float, Count>& values)
    {
        return { values[0], values[1] };
    }
};

template <>
struct YAMLFloatSequenceTraits<Vector3>
{
    YAML_FLOAT_SEQUENCE_TRAITS_COUNT(3)

    static std::array<float, Count> ToArray(const Vector3& value)
    {
        return { value.x, value.y, value.z };
    }

    static Vector3 FromArray(const std::array<float, Count>& values)
    {
        return { values[0], values[1], values[2] };
    }
};

template <>
struct YAMLFloatSequenceTraits<Vector4>
{
    YAML_FLOAT_SEQUENCE_TRAITS_COUNT(4)

    static std::array<float, Count> ToArray(const Vector4& value)
    {
        return { value.x, value.y, value.z, value.w };
    }

    static Vector4 FromArray(const std::array<float, Count>& values)
    {
        return { values[0], values[1], values[2], values[3] };
    }
};

template <>
struct YAMLFloatSequenceTraits<Color>
{
    YAML_FLOAT_SEQUENCE_TRAITS_COUNT(4)

    static std::array<float, Count> ToArray(const Color& value)
    {
        return { value.r, value.g, value.b, value.a };
    }

    static Color FromArray(const std::array<float, Count>& values)
    {
        return { values[0], values[1], values[2], values[3] };
    }
};

template <>
struct YAMLFloatSequenceTraits<Quaternion>
{
    YAML_FLOAT_SEQUENCE_TRAITS_COUNT(4)

    static std::array<float, Count> ToArray(const Quaternion& value)
    {
        return { value.x, value.y, value.z, value.w };
    }

    // Quaternionは復元時に正規化する
    static Quaternion FromArray(const std::array<float, Count>& values)
    {
        return Quaternion(values[0], values[1], values[2], values[3]).Normalized();
    }
};

template <>
struct YAMLFloatSequenceTraits<Matrix3x3>
{
    YAML_FLOAT_SEQUENCE_TRAITS_COUNT(9)
    static std::array<float, Count> ToArray(const Matrix3x3& value)
    {
        std::array<float, Count> result{};
        std::copy(&value.m[0][0], &value.m[0][0] + result.size(), result.begin());
        return result;
    }

    static Matrix3x3 FromArray(const std::array<float, Count>& values)
    {
        Matrix3x3 result;
        std::copy(values.begin(), values.end(), &result.m[0][0]);
        return result;
    }
};

template <>
struct YAMLFloatSequenceTraits<Matrix4x4>
{
    YAML_FLOAT_SEQUENCE_TRAITS_COUNT(16)
    static std::array<float, Count> ToArray(const Matrix4x4& value)
    {
        std::array<float, Count> result{};
        std::copy(&value.m[0][0], &value.m[0][0] + result.size(), result.begin());
        return result;
    }

    static Matrix4x4 FromArray(const std::array<float, Count>& values)
    {
        Matrix4x4 result;
        std::copy(values.begin(), values.end(), &result.m[0][0]);
        return result;
    }
};

// 固定長のFlow Sequenceとして保存する。
template <typename T>
struct YAMLValueConverter<T, std::void_t<
    decltype(YAMLFloatSequenceTraits<T>::Count),    // YAMLFloatSequenceTraitsとCountの定義を確認
    std::enable_if_t < !requires { Reflect<T>::Members(); } >> > // Reflect<T>が定義されていないことを確認。bool値をenable_if_tで返すことで、SFINAEを利用してこの特殊化を選択する。
{
    static YAMLValueConversionResult ToYAMLValue(const T& value, YAMLValue& output, const YAMLValueConversionContext&)
    {
        return YAMLValueConverterUtility::FloatSequenceToYAMLValue<T, YAMLFloatSequenceTraits<T>::Count>(value, output);
    }

    static YAMLValueConversionResult FromYAMLValue(const YAMLValue& input, T& output, const YAMLValueConversionContext&)
    {
        return YAMLValueConverterUtility::FloatSequenceFromYAMLValue<T, YAMLFloatSequenceTraits<T>::Count>(input, output);
    }
};

// Reflect<T>が定義された独自型は、登録メンバをMapへ再帰的に展開する。
template <typename T>
struct YAMLValueConverter<T, std::void_t<decltype(Reflect<T>::Members())>>
{
private:
    template <typename Member>
    static void ToMember(
        const T& object,
        const Member& member,
        YAMLValue& output,
        const YAMLValueConversionContext& context,
        bool& success,
        YAMLValueConversionResult& aggregate)
    {
        // オブジェクトとメンバポインタからメンバ型を取得、修飾子を除去する。
        // object.*member.secondでメンバ値を取得できる
        using Value = std::remove_cvref_t<decltype(object.*member.second)>;
        YAMLValue encoded;

        // メンバ値を再帰的に変換する
        const YAMLValueConversionResult result = YAMLValueConverter<Value>::ToYAMLValue(
            object.*member.second,
            encoded,
            context);
        YAMLValueConverterUtility::MergeConversionMessage(aggregate, result);
        if (!result.success)
        {
            success = false;
            return;
        }
        // 再帰的に作成した木をMapに追加する
        output.Add(std::string(member.first), std::move(encoded));
    }

    template <typename Member>
    static void FromMember(
        const YAMLValue& input,
        T& object,
        const Member& member,
        const YAMLValueConversionContext& context,
        bool& success,
        YAMLValueConversionResult& aggregate)
    {
        using Value = std::remove_cvref_t<decltype(object.*member.second)>;
        const YAMLValue* encoded = input.Find(member.first);
        if (!encoded)
        {
            // キー欠落は既定値を維持する。将来のフィールド追加/削除と旧ファイルの互換性を保つ。
            return;
        }

        const YAMLValueConversionResult result = YAMLValueConverter<Value>::FromYAMLValue(
            *encoded,
            object.*member.second,
            context);
        YAMLValueConverterUtility::MergeConversionMessage(aggregate, result);
        if (!result.success)
        {
            success = false;
        }
    }

public:
    static YAMLValueConversionResult ToYAMLValue(
        const T& value,
        YAMLValue& output,
        const YAMLValueConversionContext& context)
    {
        output = YAMLValue::Map();
        bool success = true;
        YAMLValueConversionResult aggregate;
        // Reflect<T>::Members()は、メンバ名とメンバポインタのペアを含むタプルを返す。
        const auto members = Reflect<T>::Members();
        // applyでタプルを展開し、各メンバをToMemberで変換する。
        // const auto&... member はタプルの各要素を参照として受け取る。
        std::apply(
            [&value, &output, &context, &success, &aggregate](const auto&... member)
            {
                (ToMember(value, member, output, context, success, aggregate), ...);
            },
            members);
        aggregate.success = success;
        return aggregate;
    }

    static YAMLValueConversionResult FromYAMLValue(
        const YAMLValue& input,
        T& output,
        const YAMLValueConversionContext& context)
    {
        if (input.type != YAMLValue::Type::Map)
        {
            return { false, YAMLMessageLevel::Error, "Reflect対象はYAML Mapである必要があります" };
        }

        // 欠落したキーは既定値を維持し、将来のフィールド追加と旧ファイルの互換性を保つ。
        T decoded{};
        bool success = true;
        YAMLValueConversionResult aggregate;
        const auto members = Reflect<T>::Members();
        std::apply(
            [&input, &decoded, &context, &success, &aggregate](const auto&... member)
            {
                (FromMember(input, decoded, member, context, success, aggregate), ...);
            },
            members);

        aggregate.success = success;
        if (success)
        {
            output = std::move(decoded);
        }
        return aggregate;
    }
};

// Curveは各KeyをMapとして並べ、時間と制御点を明示する。
template <typename T>
struct YAMLValueConverter<Curve<T>>
{
    static YAMLValueConversionResult ToYAMLValue(
        const Curve<T>& curve,
        YAMLValue& output,
        const YAMLValueConversionContext& context)
    {
        output = YAMLValue::Sequence();
        output.sequence.reserve(curve.GetKeyCount());
        for (const typename Curve<T>::Key& key : curve.GetKeys())
        {
            YAMLValue encodedKey = YAMLValue::Map();
            YAMLValue time;
            YAMLValue value;
            YAMLValue inControl;
            YAMLValue outControl;
            if (!YAMLValueConverter<float>::ToYAMLValue(key.time, time, context).success ||
                !YAMLValueConverter<T>::ToYAMLValue(key.value, value, context).success ||
                !YAMLValueConverter<T>::ToYAMLValue(key.inControl, inControl, context).success ||
                !YAMLValueConverter<T>::ToYAMLValue(key.outControl, outControl, context).success)
            {
                return { false };
            }

            encodedKey.Add("time", std::move(time));
            encodedKey.Add("value", std::move(value));
            encodedKey.Add("inControl", std::move(inControl));
            encodedKey.Add("outControl", std::move(outControl));
            output.sequence.emplace_back(std::move(encodedKey));
        }
        return {};
    }

    static YAMLValueConversionResult FromYAMLValue(
        const YAMLValue& input,
        Curve<T>& output,
        const YAMLValueConversionContext& context)
    {
        if (input.type != YAMLValue::Type::Sequence)
        {
            return { false };
        }

        std::vector<typename Curve<T>::Key> keys;
        keys.reserve(input.sequence.size());
        for (const YAMLValue& encodedKey : input.sequence)
        {
            if (encodedKey.type != YAMLValue::Type::Map)
            {
                return { false };
            }

            const YAMLValue* time = encodedKey.Find("time");
            const YAMLValue* value = encodedKey.Find("value");
            const YAMLValue* inControl = encodedKey.Find("inControl");
            const YAMLValue* outControl = encodedKey.Find("outControl");
            typename Curve<T>::Key key;
            if (!time || !value || !inControl || !outControl ||
                !YAMLValueConverter<float>::FromYAMLValue(*time, key.time, context).success ||
                !YAMLValueConverter<T>::FromYAMLValue(*value, key.value, context).success ||
                !YAMLValueConverter<T>::FromYAMLValue(*inControl, key.inControl, context).success ||
                !YAMLValueConverter<T>::FromYAMLValue(*outControl, key.outControl, context).success)
            {
                return { false };
            }
            keys.emplace_back(std::move(key));
        }

        output.SetKeys(keys);
        return {};
    }
};

// CurveColorは4本のCurveをMapの各チャンネルへ分けて保存する。
template <>
struct YAMLValueConverter<CurveColor>
{
    static YAMLValueConversionResult ToYAMLValue(
        const CurveColor& curve,
        YAMLValue& output,
        const YAMLValueConversionContext& context)
    {
        YAMLValue red;
        YAMLValue green;
        YAMLValue blue;
        YAMLValue alpha;
        if (!YAMLValueConverter<Curve<float>>::ToYAMLValue(curve.r, red, context).success ||
            !YAMLValueConverter<Curve<float>>::ToYAMLValue(curve.g, green, context).success ||
            !YAMLValueConverter<Curve<float>>::ToYAMLValue(curve.b, blue, context).success ||
            !YAMLValueConverter<Curve<float>>::ToYAMLValue(curve.a, alpha, context).success)
        {
            return { false };
        }

        output = YAMLValue::Map();
        output.Add("r", std::move(red));
        output.Add("g", std::move(green));
        output.Add("b", std::move(blue));
        output.Add("a", std::move(alpha));
        return {};
    }

    static YAMLValueConversionResult FromYAMLValue(
        const YAMLValue& input,
        CurveColor& output,
        const YAMLValueConversionContext& context)
    {
        if (input.type != YAMLValue::Type::Map)
        {
            return { false };
        }

        const YAMLValue* red = input.Find("r");
        const YAMLValue* green = input.Find("g");
        const YAMLValue* blue = input.Find("b");
        const YAMLValue* alpha = input.Find("a");
        CurveColor decoded;
        if (!red || !green || !blue || !alpha ||
            !YAMLValueConverter<Curve<float>>::FromYAMLValue(*red, decoded.r, context).success ||
            !YAMLValueConverter<Curve<float>>::FromYAMLValue(*green, decoded.g, context).success ||
            !YAMLValueConverter<Curve<float>>::FromYAMLValue(*blue, decoded.b, context).success ||
            !YAMLValueConverter<Curve<float>>::FromYAMLValue(*alpha, decoded.a, context).success)
        {
            return { false };
        }

        output = std::move(decoded);
        return {};
    }
};

// 外部アセット参照はパスだけを保存し、読込時に存在確認を行う。
template <>
struct YAMLValueConverter<PrefabReference>
{
    static YAMLValueConversionResult ToYAMLValue(const PrefabReference& value, YAMLValue& output, const YAMLValueConversionContext&)
    {
        output = value ? YAMLValue::Scalar(value.GetPath(), true) : YAMLValue::Null();
        return {};
    }

    static YAMLValueConversionResult FromYAMLValue(
        const YAMLValue& input,
        PrefabReference& output,
        const YAMLValueConversionContext& context)
    {
        if (input.type == YAMLValue::Type::Null)
        {
            output.Clear();
            return {};
        }
        if (input.type != YAMLValue::Type::Scalar)
        {
            return { false };
        }

        if (context.resolvePrefab && !context.resolvePrefab(input.scalar))
        {
            output.MarkResolutionError(input.scalar);
            return { true, YAMLMessageLevel::Error, "Prefab参照を解決できません" };
        }

        output.SetPath(input.scalar);
        return {};
    }
};

// GameObject参照はファイル内IDへ変換し、ロード後に実体へ解決する。
template <>
struct YAMLValueConverter<GameObjectReference>
{
    static YAMLValueConversionResult ToYAMLValue(
        const GameObjectReference& value,
        YAMLValue& output,
        const YAMLValueConversionContext& context)
    {
        if (!value)
        {
            output = YAMLValue::Null();
            return {};
        }
        if (!context.getObjectId)
        {
            output = YAMLValue::Null();
            return { true, YAMLMessageLevel::Warning, "GameObject参照のID変換が未指定です" };
        }

        const std::optional<YAMLValueConversionContext::ObjectId> id = context.getObjectId(value.Get());
        if (!id)
        {
            output = YAMLValue::Null();
            return { true, YAMLMessageLevel::Warning, "保存範囲外のGameObject参照をNoneとして保存します" };
        }
        return YAMLValueConverter<YAMLValueConversionContext::ObjectId>::ToYAMLValue(*id, output, context);
    }

    static YAMLValueConversionResult FromYAMLValue(
        const YAMLValue& input,
        GameObjectReference& output,
        const YAMLValueConversionContext& context)
    {
        if (input.type == YAMLValue::Type::Null)
        {
            output.Clear();
            return {};
        }

        YAMLValueConversionContext::ObjectId id{};
        const YAMLValueConversionResult idResult = YAMLValueConverter<YAMLValueConversionContext::ObjectId>::FromYAMLValue(input, id, context);
        if (!idResult.success)
        {
            return idResult;
        }

        GameObject* resolved = context.resolveObject ? context.resolveObject(id) : nullptr;
        if (!resolved)
        {
            output.MarkResolutionError();
            return { true, YAMLMessageLevel::Error, "GameObject参照を解決できません" };
        }

        output.Set(resolved);
        return {};
    }
};

// vectorは要素Codecを再利用したSequenceとして保存する。
template <typename T>
struct YAMLValueConverter<std::vector<T>>
{
    static YAMLValueConversionResult ToYAMLValue(
        const std::vector<T>& values,
        YAMLValue& output,
        const YAMLValueConversionContext& context)
    {
        output = YAMLValue::Sequence();
        output.sequence.reserve(values.size());
        YAMLValueConversionResult aggregate;
        for (const auto& sourceElement : values)
        {
            const T element = sourceElement;
            YAMLValue encoded;
            const YAMLValueConversionResult result = YAMLValueConverter<T>::ToYAMLValue(element, encoded, context);
            if (!result.success)
            {
                return result;
            }
            YAMLValueConverterUtility::MergeConversionMessage(aggregate, result);
            output.sequence.emplace_back(std::move(encoded));
        }
        return aggregate;
    }

    static YAMLValueConversionResult FromYAMLValue(
        const YAMLValue& input,
        std::vector<T>& output,
        const YAMLValueConversionContext& context)
    {
        if (input.type != YAMLValue::Type::Sequence)
        {
            return { false };
        }

        std::vector<T> decoded;
        decoded.reserve(input.sequence.size());
        YAMLValueConversionResult aggregate;
        for (const YAMLValue& sourceElement : input.sequence)
        {
            T element{};
            const YAMLValueConversionResult result = YAMLValueConverter<T>::FromYAMLValue(sourceElement, element, context);
            if (!result.success)
            {
                return result;
            }
            YAMLValueConverterUtility::MergeConversionMessage(aggregate, result);
            decoded.emplace_back(std::move(element));
        }

        output = std::move(decoded);
        return aggregate;
    }
};
