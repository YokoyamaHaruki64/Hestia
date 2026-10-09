/*=============================================================================

 File   : [Vector2_SIMD.h]
 Desc   : 2次元ベクトルのSSE実装を宣言する。

===============================================================================*/

#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>

#include <immintrin.h>
#include "math/Vector2.h"

/// @brief SSEレジスタを使って2次元ベクトルを演算する。
struct Vector2_SIMD
{
private:
    union alignas(16) Storage
    {
        __m128 value;
        float lanes[4];
    } m_storage;

    explicit Vector2_SIMD(const __m128 value) { m_storage.value = value; }

public:

    /// @brief ゼロ初期化したSIMDベクトルを作る。
    Vector2_SIMD()
    {
        m_storage.value = _mm_setzero_ps();
    }

    /// @brief 指定した2要素からベクトルを作る。
    Vector2_SIMD(const float x, const float y)
    {
        m_storage.value = _mm_setr_ps(x, y, 0.0f, 0.0f);
    }

    /// @brief 既存のVector2から値を読み込む。
    static Vector2_SIMD Load(const Vector2& source)
    {
        return Vector2_SIMD(source.x, source.y);
    }

    /// @brief 既存型の値として返す。
    Vector2 Store() const
    {
        return Vector2(m_storage.lanes[0], m_storage.lanes[1]);
    }

    /// @brief 既存型の出力先へ値を保存する。
    void Store(Vector2& destination) const
    {
        destination.x = m_storage.lanes[0];
        destination.y = m_storage.lanes[1];
    }

    /// @brief X成分への書き込み可能な参照を返す。
    float& x() { return m_storage.lanes[0]; }
    /// @brief X成分の値を返す。
    float x() const { return m_storage.lanes[0]; }
    /// @brief Y成分への書き込み可能な参照を返す。
    float& y() { return m_storage.lanes[1]; }
    /// @brief Y成分の値を返す。
    float y() const { return m_storage.lanes[1]; }

    /// @brief 指定した添字の要素への書き込み可能な参照を返す。
    float& operator[](const std::size_t index)
    {
        assert(index < 2);
        return m_storage.lanes[index];
    }

    /// @brief 指定した添字の要素の値を返す。
    float operator[](const std::size_t index) const
    {
        assert(index < 2);
        return m_storage.lanes[index];
    }

    /// @brief 加算結果をこの値へ反映する。
    Vector2_SIMD& operator+=(const Vector2_SIMD& other)
    {
        m_storage.value = _mm_add_ps(m_storage.value, other.m_storage.value);
        return *this;
    }

    /// @brief 減算結果をこの値へ反映する。
    Vector2_SIMD& operator-=(const Vector2_SIMD& other)
    {
        m_storage.value = _mm_sub_ps(m_storage.value, other.m_storage.value);
        return *this;
    }

    /// @brief スカラー倍してこのベクトルを更新する。
    Vector2_SIMD& operator*=(const float scalar)
    {
        m_storage.value = _mm_mul_ps(m_storage.value, _mm_set1_ps(scalar));
        return *this;
    }

    /// @brief 除算結果をこの値へ反映する。
    Vector2_SIMD& operator/=(const float scalar)
    {
        assert(scalar != 0.0f && "Division by zero in Vector2_SIMD");
        if (scalar != 0.0f)
        {
            m_storage.value = _mm_div_ps(m_storage.value, _mm_set1_ps(scalar));
        }
        return *this;
    }

    /// @brief 要素が一致するか判定する。
    bool operator==(const Vector2_SIMD& other) const
    {
        return m_storage.lanes[0] == other.m_storage.lanes[0] && m_storage.lanes[1] == other.m_storage.lanes[1];
    }

    /// @brief ゼロ値を返す。
    static Vector2_SIMD Zero() { return {}; }
    /// @brief 全要素が1の値を返す。
    static Vector2_SIMD One() { return Vector2_SIMD(1.0f, 1.0f); }
    /// @brief 右方向の単位ベクトルを返す。
    static Vector2_SIMD Right() { return Vector2_SIMD(1.0f, 0.0f); }
    /// @brief 左方向の単位ベクトルを返す。
    static Vector2_SIMD Left() { return Vector2_SIMD(-1.0f, 0.0f); }
    /// @brief 上方向の単位ベクトルを返す。
    static Vector2_SIMD Up() { return Vector2_SIMD(0.0f, 1.0f); }
    /// @brief 下方向の単位ベクトルを返す。
    static Vector2_SIMD Down() { return Vector2_SIMD(0.0f, -1.0f); }

    /// @brief 内積を返す。
    static float Dot(const Vector2_SIMD& a, const Vector2_SIMD& b)
    {
        const __m128 product = _mm_mul_ps(a.m_storage.value, b.m_storage.value);
        const __m128 pairSums = _mm_add_ps(product, _mm_movehl_ps(product, product));
        const __m128 secondPair = _mm_shuffle_ps(pairSums, pairSums, _MM_SHUFFLE(1, 1, 1, 1));
        return _mm_cvtss_f32(_mm_add_ss(pairSums, secondPair));
    }

    /// @brief 外積を返す。
    static float Cross(const Vector2_SIMD& a, const Vector2_SIMD& b)
    {
        return a.m_storage.lanes[0] * b.m_storage.lanes[1] - a.m_storage.lanes[1] * b.m_storage.lanes[0];
    }

    /// @brief 2つの値を係数で線形補間し、出力先へ格納する。
    static void Lerp(const Vector2_SIMD& a, const Vector2_SIMD& b, const float t, Vector2_SIMD& result)
    {
        const __m128 factor = _mm_set1_ps(t);
        const __m128 value = _mm_add_ps(
            a.m_storage.value,
            _mm_mul_ps(_mm_sub_ps(b.m_storage.value, a.m_storage.value), factor));
        result.m_storage.value = _mm_and_ps(value, _mm_castsi128_ps(_mm_set_epi32(0, 0, -1, -1)));
    }

    /// @brief 長さの二乗を返す。
    __forceinline float LengthSqr() const
    {
        return m_storage.lanes[0] * m_storage.lanes[0] +
            m_storage.lanes[1] * m_storage.lanes[1];
    }
    /// @brief 長さを返す。
    __forceinline float Length() const { return Math::Sqrt(LengthSqr()); }
    /// @brief 正規化した値を返す。
    void Normalized(Vector2_SIMD& result) const
    {
        const float length = Length();
        result.m_storage.value = length == 0.0f
            ? _mm_setzero_ps()
            : _mm_div_ps(m_storage.value, _mm_set1_ps(length));
    }

    /// @brief この値を正規化する。
    Vector2_SIMD& Normalize()
    {
        const float length = Length();
        if (length == 0.0f)
        {
            m_storage.value = _mm_setzero_ps();
        }
        else
        {
            m_storage.value = _mm_div_ps(m_storage.value, _mm_set1_ps(length));
        }
        return *this;
    }

    /// @brief 2点間距離の二乗を返す。
    static float DistanceSqr(const Vector2_SIMD& a, const Vector2_SIMD& b)
    {
        const float dx = a.m_storage.lanes[0] - b.m_storage.lanes[0];
        const float dy = a.m_storage.lanes[1] - b.m_storage.lanes[1];
        return dx * dx + dy * dy;
    }

    /// @brief 2点間の距離を返す。
    static float Distance(const Vector2_SIMD& a, const Vector2_SIMD& b)
    {
        return Math::Sqrt(DistanceSqr(a, b));
    }

    /// @brief 2つの方向のなす角をラジアンで返す。
    static float Angle(const Vector2_SIMD& a, const Vector2_SIMD& b)
    {
        const float lengthA = a.Length();
        const float lengthB = b.Length();
        if (lengthA == 0.0f || lengthB == 0.0f)
        {
            return Math::Acos(0.0f);
        }
        const float dot =
            (a.m_storage.lanes[0] / lengthA) * (b.m_storage.lanes[0] / lengthB) +
            (a.m_storage.lanes[1] / lengthA) * (b.m_storage.lanes[1] / lengthB);
        return Math::Acos(Math::Clamp(dot, -1.0f, 1.0f));
    }

    /// @brief 回転したベクトルを出力先へ格納する。
    void Rotated(const float angle, Vector2_SIMD& result) const
    {
        const float cosine = Math::Cos(angle);
        const float sine = Math::Sin(angle);
        const __m128 value = m_storage.value;
        const __m128 swapped = _mm_shuffle_ps(value, value, _MM_SHUFFLE(3, 2, 0, 1));
        const __m128 scales = _mm_setr_ps(cosine, cosine, 0.0f, 0.0f);
        const __m128 crossScales = _mm_setr_ps(-sine, sine, 0.0f, 0.0f);
        result.m_storage.value = _mm_add_ps(_mm_mul_ps(value, scales), _mm_mul_ps(swapped, crossScales));
    }

    /// @brief このベクトルを回転する。
    Vector2_SIMD& Rotate(const float angle)
    {
        Rotated(angle, *this);
        return *this;
    }
    /// @brief 2つの値を加算して出力先へ格納する。
    static void Add(const Vector2_SIMD& a, const Vector2_SIMD& b, Vector2_SIMD& result)
    {
        result.m_storage.value = _mm_add_ps(a.m_storage.value, b.m_storage.value);
    }

    /// @brief 2つの値を減算して出力先へ格納する。
    static void Subtract(const Vector2_SIMD& a, const Vector2_SIMD& b, Vector2_SIMD& result)
    {
        result.m_storage.value = _mm_sub_ps(a.m_storage.value, b.m_storage.value);
    }

    /// @brief 要素ごとの積を返す。
    static void Multiply(const Vector2_SIMD& a, const Vector2_SIMD& b, Vector2_SIMD& result)
    {
        result.m_storage.value = _mm_mul_ps(a.m_storage.value, b.m_storage.value);
    }

    /// @brief スカラー倍した結果を出力先へ格納する。
    static void Multiply(const Vector2_SIMD& value, const float scalar, Vector2_SIMD& result)
    {
        result.m_storage.value = _mm_mul_ps(value.m_storage.value, _mm_set1_ps(scalar));
    }

    /// @brief 要素ごとの商を返す。
    static void Divide(const Vector2_SIMD& a, const Vector2_SIMD& b, Vector2_SIMD& result)
    {
        assert(b.m_storage.lanes[0] != 0.0f && b.m_storage.lanes[1] != 0.0f && "Division by zero in Vector2_SIMD::Divide");
        if (b.m_storage.lanes[0] == 0.0f || b.m_storage.lanes[1] == 0.0f)
        {
            result = Zero();
            return;
        }
        const __m128 quotient = _mm_div_ps(a.m_storage.value, b.m_storage.value);
        result.m_storage.value = _mm_and_ps(quotient, _mm_castsi128_ps(_mm_set_epi32(0, 0, -1, -1)));
    }

    /// @brief スカラーで除算した結果を出力先へ格納する。
    static void Divide(const Vector2_SIMD& value, const float scalar, Vector2_SIMD& result)
    {
        assert(scalar != 0.0f && "Division by zero in Vector2_SIMD::Divide");
        if (scalar == 0.0f)
        {
            result = value;
            return;
        }
        result.m_storage.value = _mm_div_ps(value.m_storage.value, _mm_set1_ps(scalar));
    }
};
