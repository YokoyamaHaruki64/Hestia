/*=============================================================================

 File   : [Vector4_SIMD.h]
 Desc   : 4次元ベクトルのSSE実装を宣言する。

===============================================================================*/

#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>

#include <immintrin.h>
#include "Math/Vector4.h"

/// @brief SSEレジスタを使って4次元ベクトルを演算する。
struct Vector4_SIMD
{
private:
    union alignas(16) Storage
    {
        __m128 value;
        float lanes[4];
    } m_storage;

public:

    /// @brief ゼロ初期化したSIMDベクトルを作る。
    Vector4_SIMD()
    {
        m_storage.value = _mm_setzero_ps();
    }

    /// @brief 指定した4要素からベクトルを作る。
    Vector4_SIMD(const float x, const float y, const float z, const float w)
    {
        m_storage.value = _mm_setr_ps(x, y, z, w);
    }

    /// @brief 既存のVector4から値を読み込む。
    static Vector4_SIMD Load(const Vector4& source)
    {
        return Vector4_SIMD(source.x, source.y, source.z, source.w);
    }

    /// @brief 既存型の値として返す。
    Vector4 Store() const
    {
        return Vector4(m_storage.lanes[0], m_storage.lanes[1], m_storage.lanes[2], m_storage.lanes[3]);
    }

    /// @brief 既存型の出力先へ値を保存する。
    void Store(Vector4& destination) const
    {
        destination.x = m_storage.lanes[0];
        destination.y = m_storage.lanes[1];
        destination.z = m_storage.lanes[2];
        destination.w = m_storage.lanes[3];
    }

    /// @brief X成分への書き込み可能な参照を返す。
    float& x() { return m_storage.lanes[0]; }
    /// @brief X成分の値を返す。
    float x() const { return m_storage.lanes[0]; }
    /// @brief Y成分への書き込み可能な参照を返す。
    float& y() { return m_storage.lanes[1]; }
    /// @brief Y成分の値を返す。
    float y() const { return m_storage.lanes[1]; }
    /// @brief Z成分への書き込み可能な参照を返す。
    float& z() { return m_storage.lanes[2]; }
    /// @brief Z成分の値を返す。
    float z() const { return m_storage.lanes[2]; }
    /// @brief W成分への書き込み可能な参照を返す。
    float& w() { return m_storage.lanes[3]; }
    /// @brief W成分の値を返す。
    float w() const { return m_storage.lanes[3]; }

    /// @brief 指定した添字の要素への書き込み可能な参照を返す。
    float& operator[](const std::size_t index)
    {
        assert(index < 4);
        return m_storage.lanes[index];
    }

    /// @brief 指定した添字の要素の値を返す。
    float operator[](const std::size_t index) const
    {
        assert(index < 4);
        return m_storage.lanes[index];
    }

private:
    explicit Vector4_SIMD(const __m128 value) { m_storage.value = value; }

public:
    /// @brief 加算結果をこの値へ反映する。
    Vector4_SIMD& operator+=(const Vector4_SIMD& other)
    {
        m_storage.value = _mm_add_ps(m_storage.value, other.m_storage.value);
        return *this;
    }

    /// @brief 減算結果をこの値へ反映する。
    Vector4_SIMD& operator-=(const Vector4_SIMD& other)
    {
        m_storage.value = _mm_sub_ps(m_storage.value, other.m_storage.value);
        return *this;
    }

    /// @brief スカラー倍してこのベクトルを更新する。
    Vector4_SIMD& operator*=(const float scalar)
    {
        m_storage.value = _mm_mul_ps(m_storage.value, _mm_set1_ps(scalar));
        return *this;
    }

    /// @brief 除算結果をこの値へ反映する。
    Vector4_SIMD& operator/=(const float scalar)
    {
        assert(scalar != 0.0f && "Division by zero in Vector4_SIMD");
        if (scalar != 0.0f)
        {
            m_storage.value = _mm_div_ps(m_storage.value, _mm_set1_ps(scalar));
        }
        return *this;
    }

    /// @brief ゼロ値を返す。
    static Vector4_SIMD Zero() { return {}; }
    /// @brief 全要素が1の値を返す。
    static Vector4_SIMD One() { return Vector4_SIMD(1.0f, 1.0f, 1.0f, 1.0f); }

    /// @brief 要素が一致するか判定する。
    bool operator==(const Vector4_SIMD& other) const
    {
        return m_storage.lanes[0] == other.m_storage.lanes[0] && m_storage.lanes[1] == other.m_storage.lanes[1] &&
            m_storage.lanes[2] == other.m_storage.lanes[2] && m_storage.lanes[3] == other.m_storage.lanes[3];
    }

    /// @brief 内積を返す。
    static float Dot(const Vector4_SIMD& a, const Vector4_SIMD& b)
    {
        const __m128 product = _mm_mul_ps(a.m_storage.value, b.m_storage.value);
        const __m128 pairSums = _mm_add_ps(product, _mm_movehl_ps(product, product));
        const __m128 secondPair = _mm_shuffle_ps(pairSums, pairSums, _MM_SHUFFLE(1, 1, 1, 1));
        return _mm_cvtss_f32(_mm_add_ss(pairSums, secondPair));
    }

    /// @brief 2つの値を係数で線形補間し、出力先へ格納する。
    static void Lerp(const Vector4_SIMD& a, const Vector4_SIMD& b, const float t, Vector4_SIMD& result)
    {
        const __m128 factor = _mm_set1_ps(t);
        result.m_storage.value = _mm_add_ps(
            a.m_storage.value,
            _mm_mul_ps(_mm_sub_ps(b.m_storage.value, a.m_storage.value), factor));
    }

    /// @brief 長さの二乗を返す。
    float LengthSqr() const
    {
        return m_storage.lanes[0] * m_storage.lanes[0] +
            m_storage.lanes[1] * m_storage.lanes[1] +
            m_storage.lanes[2] * m_storage.lanes[2] +
            m_storage.lanes[3] * m_storage.lanes[3];
    }
    /// @brief 長さを返す。
    float Length() const
    {
        return Math::Sqrt(
            m_storage.lanes[0] * m_storage.lanes[0] +
            m_storage.lanes[1] * m_storage.lanes[1] +
            m_storage.lanes[2] * m_storage.lanes[2] +
            m_storage.lanes[3] * m_storage.lanes[3]);
    }
    /// @brief 正規化した値を返す。
    void Normalized(Vector4_SIMD& result) const
    {
        const float length = Math::Sqrt(
            m_storage.lanes[0] * m_storage.lanes[0] +
            m_storage.lanes[1] * m_storage.lanes[1] +
            m_storage.lanes[2] * m_storage.lanes[2] +
            m_storage.lanes[3] * m_storage.lanes[3]);
        result.m_storage.value = length == 0.0f
            ? _mm_setzero_ps()
            : _mm_div_ps(m_storage.value, _mm_set1_ps(length));
    }

    /// @brief この値を正規化する。
    Vector4_SIMD& Normalize()
    {
        const float length = Math::Sqrt(
            m_storage.lanes[0] * m_storage.lanes[0] +
            m_storage.lanes[1] * m_storage.lanes[1] +
            m_storage.lanes[2] * m_storage.lanes[2] +
            m_storage.lanes[3] * m_storage.lanes[3]);
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
    static float DistanceSqr(const Vector4_SIMD& a, const Vector4_SIMD& b)
    {
        const float dx = a.m_storage.lanes[0] - b.m_storage.lanes[0];
        const float dy = a.m_storage.lanes[1] - b.m_storage.lanes[1];
        const float dz = a.m_storage.lanes[2] - b.m_storage.lanes[2];
        const float dw = a.m_storage.lanes[3] - b.m_storage.lanes[3];
        return dx * dx + dy * dy + dz * dz + dw * dw;
    }

    /// @brief 2点間の距離を返す。
    static float Distance(const Vector4_SIMD& a, const Vector4_SIMD& b)
    {
        return Math::Sqrt(DistanceSqr(a, b));
    }

    /// @brief 2つの方向のなす角をラジアンで返す。
    static float Angle(const Vector4_SIMD& a, const Vector4_SIMD& b)
    {
        const float lengthA = Math::Sqrt(
            a.m_storage.lanes[0] * a.m_storage.lanes[0] +
            a.m_storage.lanes[1] * a.m_storage.lanes[1] +
            a.m_storage.lanes[2] * a.m_storage.lanes[2] +
            a.m_storage.lanes[3] * a.m_storage.lanes[3]);
        const float lengthB = Math::Sqrt(
            b.m_storage.lanes[0] * b.m_storage.lanes[0] +
            b.m_storage.lanes[1] * b.m_storage.lanes[1] +
            b.m_storage.lanes[2] * b.m_storage.lanes[2] +
            b.m_storage.lanes[3] * b.m_storage.lanes[3]);
        const float dot = lengthA == 0.0f || lengthB == 0.0f
            ? 0.0f
            : Dot(a, b) / (lengthA * lengthB);
        return Math::Acos(Math::Clamp(dot, -1.0f, 1.0f));
    }
    /// @brief 2つの値を加算して出力先へ格納する。
    static void Add(const Vector4_SIMD& a, const Vector4_SIMD& b, Vector4_SIMD& result)
    {
        result.m_storage.value = _mm_add_ps(a.m_storage.value, b.m_storage.value);
    }

    /// @brief 2つの値を減算して出力先へ格納する。
    static void Subtract(const Vector4_SIMD& a, const Vector4_SIMD& b, Vector4_SIMD& result)
    {
        result.m_storage.value = _mm_sub_ps(a.m_storage.value, b.m_storage.value);
    }

    /// @brief 要素ごとの積を返す。
    static void Multiply(const Vector4_SIMD& a, const Vector4_SIMD& b, Vector4_SIMD& result)
    {
        result.m_storage.value = _mm_mul_ps(a.m_storage.value, b.m_storage.value);
    }

    /// @brief スカラー倍した結果を出力先へ格納する。
    static void Multiply(const Vector4_SIMD& value, const float scalar, Vector4_SIMD& result)
    {
        result.m_storage.value = _mm_mul_ps(value.m_storage.value, _mm_set1_ps(scalar));
    }

    /// @brief 要素ごとの商を返す。
    static void Divide(const Vector4_SIMD& a, const Vector4_SIMD& b, Vector4_SIMD& result)
    {
        assert(b.m_storage.lanes[0] != 0.0f && b.m_storage.lanes[1] != 0.0f && b.m_storage.lanes[2] != 0.0f && b.m_storage.lanes[3] != 0.0f && "Division by zero in Vector4_SIMD::Divide");
        if (b.m_storage.lanes[0] == 0.0f || b.m_storage.lanes[1] == 0.0f || b.m_storage.lanes[2] == 0.0f || b.m_storage.lanes[3] == 0.0f)
        {
            result = Zero();
            return;
        }
        result.m_storage.value = _mm_div_ps(a.m_storage.value, b.m_storage.value);
    }

    /// @brief スカラーで除算した結果を出力先へ格納する。
    static void Divide(const Vector4_SIMD& value, const float scalar, Vector4_SIMD& result)
    {
        assert(scalar != 0.0f && "Division by zero in Vector4_SIMD::Divide");
        if (scalar == 0.0f)
        {
            result = value;
            return;
        }
        result.m_storage.value = _mm_div_ps(value.m_storage.value, _mm_set1_ps(scalar));
    }
};
