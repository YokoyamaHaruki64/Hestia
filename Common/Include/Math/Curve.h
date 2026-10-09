#pragma once

#include <vector>
#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <istream>
#include <ostream>
#include <type_traits>
#include <utility>
#include "Math/MathCommon.h"
#include "Math/Vector3.h"
#include "Color.h"

template<typename T>
class Curve
{
public:
    struct Key
    {
        T value{};
        float time = 0.0f;

        T inControl{};
        T outControl{};

        Key() = default;
        Key(const T& value, float time)
            : value(value), time(Math::Saturate(time)), inControl(value), outControl(value)
        {
        }

        Key(const T& value, float time, const T& inControl, const T& outControl)
            : value(value), time(Math::Saturate(time)), inControl(inControl), outControl(outControl)
        {
        }
    };

private:
    std::vector<Key> m_keys;

public:
    Curve()
    {
        if constexpr (std::is_same_v<T, float>)
        {
            AddKey(0.0f, 0.0f);
            AddKey(1.0f, 1.0f);
        }
    }

    Curve(const T& startValue, const T& endValue)
    {
        AddKey(startValue, 0.0f);
        AddKey(endValue, 1.0f);
    }

    explicit Curve(const std::vector<Key>& keys)
        : m_keys(keys)
    {
        NormalizeKeys();
    }
    // Initializer_listコンストラクタによるキーの初期化
    Curve(std::initializer_list<Key> keys)
        : m_keys(keys)
    {
        NormalizeKeys();
    }

    void SetKeys(const std::vector<Key>& keys)
    {
        m_keys = keys;
        NormalizeKeys();
    }

    const std::vector<Key>& GetKeys() const
    {
        return m_keys;
    }

    std::vector<Key>& GetKeys()
    {
        return m_keys;
    }

    std::size_t GetKeyCount() const
    {
        return m_keys.size();
    }

    void Clear()
    {
        m_keys.clear();
    }

    // ControlPoint なし版
    void AddKey(const T& value, float time)
    {
        Key key;
        key.value = value;
        key.time = Math::Saturate(time);
        key.inControl = value;
        key.outControl = value;

        m_keys.push_back(key);

        NormalizeKeys();
    }

    // ControlPoint あり版
    void AddKey(
        const T& value,
        float time,
        const T& inControl,
        const T& outControl)
    {
        Key key;
        key.value = value;
        key.time = Math::Saturate(time);
        key.inControl = inControl;
        key.outControl = outControl;

        m_keys.push_back(key);

        NormalizeKeys();
    }

    void SetKey(std::size_t index, const Key& key)
    {
        if (index >= m_keys.size())
        {
            return;
        }

        m_keys[index] = key;
        m_keys[index].time = Math::Saturate(m_keys[index].time);

        NormalizeKeys();
    }

    void SetValue(std::size_t index, const T& value)
    {
        if (index >= m_keys.size())
        {
            return;
        }

        m_keys[index].value = value;

    }

    void SetTime(std::size_t index, float time)
    {
        if (index >= m_keys.size())
        {
            return;
        }

        m_keys[index].time = Math::Saturate(time);

        NormalizeKeys();
    }

    void SetInControl(std::size_t index, const T& inControl)
    {
        if (index >= m_keys.size())
        {
            return;
        }

        m_keys[index].inControl = inControl;
    }

    void SetOutControl(std::size_t index, const T& outControl)
    {
        if (index >= m_keys.size())
        {
            return;
        }

        m_keys[index].outControl = outControl;
    }

    void RemoveKey(std::size_t index)
    {
        if (index >= m_keys.size())
        {
            return;
        }

        m_keys.erase(m_keys.begin() + index);

    }

    T Evaluate(float t) const
    {
        t = Math::Saturate(t);

        if (m_keys.empty())
        {
            return T{};
        }

        if (m_keys.size() == 1)
        {
            return m_keys[0].value;
        }

        if (t <= m_keys.front().time)
        {
            return m_keys.front().value;
        }

        if (t >= m_keys.back().time)
        {
            return m_keys.back().value;
        }

        const std::size_t segmentIndex = FindSegmentIndex(t);

        const Key& k0 = m_keys[segmentIndex];
        const Key& k1 = m_keys[segmentIndex + 1];

        const float duration = k1.time - k0.time;

        if (duration <= 0.0f)
        {
            return k1.value;
        }

        const float localT = (t - k0.time) / duration;

        return Bezier(
            k0.value,
            k0.outControl,
            k1.inControl,
            k1.value,
            localT
        );
    }

    void Serialize(std::ostream& os) const
    {
        os << m_keys.size() << std::endl;
        for (const Key& key : m_keys)
        {
            os << key.value << " " << key.time << " " << key.inControl << " " << key.outControl << std::endl;
        }
    }

    bool Deserialize(std::istream& is)
    {
        if (!is) false;

        std::size_t keyCount;
        is >> keyCount;
        if (!is) return false;
        std::vector<Key> tempKeys;
        tempKeys.reserve(keyCount);

        for (std::size_t i = 0; i < keyCount; ++i)
        {
            Key key;
            is >> key.value >> key.time >> key.inControl >> key.outControl;
            if (!is) return false;
            tempKeys.push_back(key);
        }
        m_keys = std::move(tempKeys);
        NormalizeKeys();
        return true;
    }

private:
    void NormalizeKeys()
    {
        for (Key& key : m_keys)
        {
            key.time = Math::Saturate(key.time);
        }

        std::sort(
            m_keys.begin(),
            m_keys.end(),
            [](const Key& a, const Key& b)
            {
                return a.time < b.time;
            }
        );
    }


    std::size_t FindSegmentIndex(float t) const
    {
        for (std::size_t i = 0; i + 1 < m_keys.size(); ++i)
        {
            if (t >= m_keys[i].time && t <= m_keys[i + 1].time)
            {
                return i;
            }
        }

        return m_keys.size() - 2;
    }

    static T Lerp(const T& a, const T& b, float t)
    {
        return a * (1.0f - t) + b * t;
    }

    static T Bezier(
        const T& p0,
        const T& p1,
        const T& p2,
        const T& p3,
        float t)
    {
        const float u = 1.0f - t;

        const float b0 = u * u * u;
        const float b1 = 3.0f * u * u * t;
        const float b2 = 3.0f * u * t * t;
        const float b3 = t * t * t;

        return p0 * b0
            + p1 * b1
            + p2 * b2
            + p3 * b3;
    }
};

class CurveVector3
{
public:
    Curve<float> x;
    Curve<float> y;
    Curve<float> z;

    CurveVector3() = default;

    CurveVector3(const Vector3& startValue, const Vector3& endValue)
        :
        x(startValue.x, endValue.x),
        y(startValue.y, endValue.y),
        z(startValue.z, endValue.z)
    {
    }

    CurveVector3(
        const std::vector<Curve<float>::Key>& xKeys,
        const std::vector<Curve<float>::Key>& yKeys,
        const std::vector<Curve<float>::Key>& zKeys)
        : x(xKeys), y(yKeys), z(zKeys)
    {
    }
    CurveVector3(
        std::initializer_list<Curve<float>::Key> xKeys,
        std::initializer_list<Curve<float>::Key> yKeys,
        std::initializer_list<Curve<float>::Key> zKeys)
        : x(xKeys), y(yKeys), z(zKeys)
    {
    }

    Vector3 Evaluate(float t) const
    {
        return Vector3(
            x.Evaluate(t),
            y.Evaluate(t),
            z.Evaluate(t)
        );
    }

    void Serialize(std::ostream& os) const
    {
        x.Serialize(os);
        y.Serialize(os);
        z.Serialize(os);
    }

    bool Deserialize(std::istream& is)
    {
        if (!x.Deserialize(is)) return false;
        if (!y.Deserialize(is)) return false;
        if (!z.Deserialize(is)) return false;

        return true;
    }
};

class CurveColor
{
public:
    Curve<float> r;
    Curve<float> g;
    Curve<float> b;
    Curve<float> a;

    CurveColor()
        :
        r(1.0f, 1.0f),
        g(1.0f, 1.0f),
        b(1.0f, 1.0f),
        a(1.0f, 0.0f)
    {
    }

    CurveColor(const Color& startColor, const Color& endColor)
        :
        r(startColor.r, endColor.r),
        g(startColor.g, endColor.g),
        b(startColor.b, endColor.b),
        a(startColor.a, endColor.a)
    {
    }

    CurveColor(const std::vector<Curve<float>::Key>& rKeys,
               const std::vector<Curve<float>::Key>& gKeys,
               const std::vector<Curve<float>::Key>& bKeys,
               const std::vector<Curve<float>::Key>& aKeys)
        : r(rKeys), g(gKeys), b(bKeys), a(aKeys)
    {
    }

    CurveColor(std::initializer_list<Curve<float>::Key> rKeys,
               std::initializer_list<Curve<float>::Key> gKeys,
               std::initializer_list<Curve<float>::Key> bKeys,
               std::initializer_list<Curve<float>::Key> aKeys)
        : r(rKeys), g(gKeys), b(bKeys), a(aKeys)
    {
    }

    Color Evaluate(float t) const
    {
        return Color(
            r.Evaluate(t),
            g.Evaluate(t),
            b.Evaluate(t),
            a.Evaluate(t)
        );
    }

    void Serialize(std::ostream& os) const
    {
        r.Serialize(os);
        g.Serialize(os);
        b.Serialize(os);
        a.Serialize(os);
    }

    bool Deserialize(std::istream& is)
    {
        if (!r.Deserialize(is)) return false;
        if (!g.Deserialize(is)) return false;
        if (!b.Deserialize(is)) return false;
        if (!a.Deserialize(is)) return false;
        return true;
    }
};
