/*=============================================================================

 File   : [Vector3_SIMD.h]
 Desc   : 3次元ベクトルのSSE実装を宣言する。

===============================================================================*/

#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>

#include <immintrin.h>
#include "math/Vector3.h"

/// @brief SSEレジスタを使って3次元ベクトルを演算する。
struct Vector3_SIMD
{
private:
    union alignas(16) Storage
    {
        __m128 value;
        float lanes[4];
    } m_storage;

public:

    /// @brief ゼロ初期化したSIMDベクトルを作る。
    Vector3_SIMD()
    {
        m_storage.value = _mm_setzero_ps();
    }

    /// @brief 指定した3要素からベクトルを作る。
    Vector3_SIMD(const float x, const float y, const float z)
    {
        m_storage.value = _mm_setr_ps(x, y, z, 0.0f);
    }

    /// @brief 既存のVector3から値を読み込む。
    static Vector3_SIMD Load(const Vector3& source)
    {
        return Vector3_SIMD(source.x, source.y, source.z);
    }

    /// @brief 既存型の値として返す。
    Vector3 Store() const
    {
        return Vector3(m_storage.lanes[0], m_storage.lanes[1], m_storage.lanes[2]);
    }

    /// @brief 既存型の出力先へ値を保存する。
    void Store(Vector3& destination) const
    {
        destination.x = m_storage.lanes[0];
        destination.y = m_storage.lanes[1];
        destination.z = m_storage.lanes[2];
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

    /// @brief 指定した添字の要素への書き込み可能な参照を返す。
    float& operator[](const std::size_t index)
    {
        assert(index < 3);
        return m_storage.lanes[index];
    }

    /// @brief 指定した添字の要素の値を返す。
    float operator[](const std::size_t index) const
    {
        assert(index < 3);
        return m_storage.lanes[index];
    }

private:
    explicit Vector3_SIMD(const __m128 value) { m_storage.value = value; }

public:
    /// @brief 加算結果をこの値へ反映する。
    Vector3_SIMD& operator+=(const Vector3_SIMD& other)
    {
        m_storage.value = _mm_add_ps(m_storage.value, other.m_storage.value);
        return *this;
    }

    /// @brief 減算結果をこの値へ反映する。
    Vector3_SIMD& operator-=(const Vector3_SIMD& other)
    {
        m_storage.value = _mm_sub_ps(m_storage.value, other.m_storage.value);
        return *this;
    }

    /// @brief スカラー倍してこのベクトルを更新する。
    Vector3_SIMD& operator*=(const float scalar)
    {
        m_storage.value = _mm_mul_ps(m_storage.value, _mm_set1_ps(scalar));
        return *this;
    }

    /// @brief 除算結果をこの値へ反映する。
    Vector3_SIMD& operator/=(const float scalar)
    {
        assert(scalar != 0.0f && "Division by zero in Vector3_SIMD");
        if (scalar != 0.0f)
        {
            m_storage.value = _mm_div_ps(m_storage.value, _mm_set1_ps(scalar));
        }
        return *this;
    }

    /// @brief 要素が一致するか判定する。
    bool operator==(const Vector3_SIMD& other) const
    {
        return m_storage.lanes[0] == other.m_storage.lanes[0] && m_storage.lanes[1] == other.m_storage.lanes[1] && m_storage.lanes[2] == other.m_storage.lanes[2];
    }

    /// @brief ゼロ値を返す。
    static Vector3_SIMD Zero() { return {}; }
    /// @brief 全要素が1の値を返す。
    static Vector3_SIMD One() { return Vector3_SIMD(1.0f, 1.0f, 1.0f); }
    /// @brief 右方向の単位ベクトルを返す。
    static Vector3_SIMD Right() { return Vector3_SIMD(1.0f, 0.0f, 0.0f); }
    /// @brief 左方向の単位ベクトルを返す。
    static Vector3_SIMD Left() { return Vector3_SIMD(-1.0f, 0.0f, 0.0f); }
    /// @brief 上方向の単位ベクトルを返す。
    static Vector3_SIMD Up() { return Vector3_SIMD(0.0f, 1.0f, 0.0f); }
    /// @brief 下方向の単位ベクトルを返す。
    static Vector3_SIMD Down() { return Vector3_SIMD(0.0f, -1.0f, 0.0f); }
    /// @brief 前方向の単位ベクトルを返す。
    static Vector3_SIMD Forward() { return Vector3_SIMD(0.0f, 0.0f, 1.0f); }
    /// @brief 後方向の単位ベクトルを返す。
    static Vector3_SIMD Backward() { return Vector3_SIMD(0.0f, 0.0f, -1.0f); }

    /// @brief 内積を返す。
    static float Dot(const Vector3_SIMD& a, const Vector3_SIMD& b)
    {
        const __m128 product = _mm_mul_ps(a.m_storage.value, b.m_storage.value);
        const __m128 pairSums = _mm_add_ps(product, _mm_movehl_ps(product, product));
        const __m128 secondPair = _mm_shuffle_ps(pairSums, pairSums, _MM_SHUFFLE(1, 1, 1, 1));
        return _mm_cvtss_f32(_mm_add_ss(pairSums, secondPair));
    }

    /// @brief 外積を出力先へ格納する。
    static void Cross(const Vector3_SIMD& a, const Vector3_SIMD& b, Vector3_SIMD& result)
    {
        const __m128 left = a.m_storage.value;
        const __m128 right = b.m_storage.value;
        const __m128 ayzx = _mm_shuffle_ps(left, left, _MM_SHUFFLE(3, 0, 2, 1));
        const __m128 azxy = _mm_shuffle_ps(left, left, _MM_SHUFFLE(3, 1, 0, 2));
        const __m128 byzx = _mm_shuffle_ps(right, right, _MM_SHUFFLE(3, 0, 2, 1));
        const __m128 bzxy = _mm_shuffle_ps(right, right, _MM_SHUFFLE(3, 1, 0, 2));
        result.m_storage.value = _mm_sub_ps(_mm_mul_ps(ayzx, bzxy), _mm_mul_ps(azxy, byzx));
    }

    /// @brief 外積を出力先へ格納する。
    void Cross(const Vector3_SIMD& other, Vector3_SIMD& result) const { Cross(*this, other, result); }
    /// @brief 内積を返す。
    float Dot(const Vector3_SIMD& other) const { return Dot(*this, other); }

    /// @brief 2つの値を係数で線形補間し、出力先へ格納する。
    static void Lerp(const Vector3_SIMD& a, const Vector3_SIMD& b, const float t, Vector3_SIMD& result)
    {
        const __m128 factor = _mm_set1_ps(t);
        const __m128 value = _mm_add_ps(
            a.m_storage.value,
            _mm_mul_ps(_mm_sub_ps(b.m_storage.value, a.m_storage.value), factor));
        result.m_storage.value = _mm_and_ps(value, _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1)));
    }

    /// @brief 長さの二乗を返す。
    float LengthSqr() const
    {
        return m_storage.lanes[0] * m_storage.lanes[0] +
            m_storage.lanes[1] * m_storage.lanes[1] +
            m_storage.lanes[2] * m_storage.lanes[2];
    }
    /// @brief 長さを返す。
    float Length() const
    {
        return Math::Sqrt(
            m_storage.lanes[0] * m_storage.lanes[0] +
            m_storage.lanes[1] * m_storage.lanes[1] +
            m_storage.lanes[2] * m_storage.lanes[2]);
    }
    /// @brief 正規化した値を出力先へ格納する。
    void Normalized(Vector3_SIMD& result) const
    {
        const float length = Math::Sqrt(
            m_storage.lanes[0] * m_storage.lanes[0] +
            m_storage.lanes[1] * m_storage.lanes[1] +
            m_storage.lanes[2] * m_storage.lanes[2]);
        result.m_storage.value = length == 0.0f
            ? _mm_setzero_ps()
            : _mm_div_ps(m_storage.value, _mm_set1_ps(length));
    }

    /// @brief この値を正規化する。
    Vector3_SIMD& Normalize()
    {
        const float length = Math::Sqrt(
            m_storage.lanes[0] * m_storage.lanes[0] +
            m_storage.lanes[1] * m_storage.lanes[1] +
            m_storage.lanes[2] * m_storage.lanes[2]);
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
    static float DistanceSqr(const Vector3_SIMD& a, const Vector3_SIMD& b)
    {
        const float dx = a.m_storage.lanes[0] - b.m_storage.lanes[0];
        const float dy = a.m_storage.lanes[1] - b.m_storage.lanes[1];
        const float dz = a.m_storage.lanes[2] - b.m_storage.lanes[2];
        return dx * dx + dy * dy + dz * dz;
    }

    /// @brief 2点間の距離を返す。
    static float Distance(const Vector3_SIMD& a, const Vector3_SIMD& b)
    {
        return Math::Sqrt(DistanceSqr(a, b));
    }

    /// @brief 2つの方向のなす角をラジアンで返す。
    static float Angle(const Vector3_SIMD& a, const Vector3_SIMD& b)
    {
        const float lengthA = Math::Sqrt(
            a.m_storage.lanes[0] * a.m_storage.lanes[0] +
            a.m_storage.lanes[1] * a.m_storage.lanes[1] +
            a.m_storage.lanes[2] * a.m_storage.lanes[2]);
        const float lengthB = Math::Sqrt(
            b.m_storage.lanes[0] * b.m_storage.lanes[0] +
            b.m_storage.lanes[1] * b.m_storage.lanes[1] +
            b.m_storage.lanes[2] * b.m_storage.lanes[2]);
        const float dot = lengthA == 0.0f || lengthB == 0.0f
            ? 0.0f
            : Dot(a, b) / (lengthA * lengthB);
        return Math::Acos(Math::Clamp(dot, -1.0f, 1.0f));
    }

    /// @brief 入射方向の反射ベクトルを出力先へ格納する。
    static void Reflect(const Vector3_SIMD& inDirection, const Vector3_SIMD& normal, Vector3_SIMD& result)
    {
        result.m_storage.value = _mm_add_ps(
            inDirection.m_storage.value,
            _mm_mul_ps(normal.m_storage.value, _mm_set1_ps(-2.0f * Dot(inDirection, normal))));
    }

    /// @brief 入射方向の反射ベクトルを出力先へ格納する。
    static void Reflect(const Vector3_SIMD& inDirection, const Vector3_SIMD& normal, const float restitution, Vector3_SIMD& result)
    {
        result.m_storage.value = _mm_add_ps(
            inDirection.m_storage.value,
            _mm_mul_ps(normal.m_storage.value, _mm_set1_ps(-(1.0f + restitution) * Dot(inDirection, normal))));
    }

    /// @brief 法線から右方向を出力先へ格納する。
    static void CreateRight(const Vector3_SIMD& normal, Vector3_SIMD& result)
    {
        Vector3_SIMD n;
        normal.Normalized(n);
        const bool parallel = Dot(n, Up()) > 0.999f;
        const bool antiParallel = Dot(n, Up()) < -0.999f;
        const Vector3_SIMD referenceUp = parallel || antiParallel ? Forward() : Up();
        Vector3_SIMD right;
        Cross(referenceUp, n, right);
        right.Normalize();
        if (parallel)
        {
            right *= -1.0f;
        }
        result = right;
    }

    /// @brief 法線から上方向を出力先へ格納する。
    static void CreateUp(const Vector3_SIMD& normal, Vector3_SIMD& result)
    {
        Vector3_SIMD n;
        normal.Normalized(n);
        const bool parallel = Dot(n, Up()) > 0.999f;
        const bool antiParallel = Dot(n, Up()) < -0.999f;
        const Vector3_SIMD referenceUp = parallel || antiParallel ? Forward() : Up();
        Vector3_SIMD right;
        Cross(referenceUp, n, right);
        right.Normalize();
        Cross(n, right, result);
        result.Normalize();
        if (parallel)
        {
            result *= -1.0f;
        }
    }

    /// @brief 法線から右方向と上方向を作る。
    static void CreateRightUp(const Vector3_SIMD& normal, Vector3_SIMD& outRight, Vector3_SIMD& outUp)
    {
        Vector3_SIMD n;
        normal.Normalized(n);
        const bool parallel = Dot(n, Up()) > 0.999f;
        const bool antiParallel = Dot(n, Up()) < -0.999f;
        const Vector3_SIMD referenceUp = parallel || antiParallel ? Forward() : Up();
        Cross(referenceUp, n, outRight);
        outRight.Normalize();
        Cross(n, outRight, outUp);
        outUp.Normalize();
        if (parallel)
        {
            outRight *= -1.0f;
        }
    }
    /// @brief 2つの値を加算して出力先へ格納する。
    static void Add(const Vector3_SIMD& a, const Vector3_SIMD& b, Vector3_SIMD& result)
    {
        result.m_storage.value = _mm_add_ps(a.m_storage.value, b.m_storage.value);
    }

    /// @brief 2つの値を減算して出力先へ格納する。
    static void Subtract(const Vector3_SIMD& a, const Vector3_SIMD& b, Vector3_SIMD& result)
    {
        result.m_storage.value = _mm_sub_ps(a.m_storage.value, b.m_storage.value);
    }

    /// @brief 要素ごとの積を返す。
    static void Multiply(const Vector3_SIMD& a, const Vector3_SIMD& b, Vector3_SIMD& result)
    {
        result.m_storage.value = _mm_mul_ps(a.m_storage.value, b.m_storage.value);
    }

    /// @brief スカラー倍した結果を出力先へ格納する。
    static void Multiply(const Vector3_SIMD& value, const float scalar, Vector3_SIMD& result)
    {
        result.m_storage.value = _mm_mul_ps(value.m_storage.value, _mm_set1_ps(scalar));
    }

    /// @brief 要素ごとの商を返す。
    static void Divide(const Vector3_SIMD& a, const Vector3_SIMD& b, Vector3_SIMD& result)
    {
        assert(b.m_storage.lanes[0] != 0.0f && b.m_storage.lanes[1] != 0.0f && b.m_storage.lanes[2] != 0.0f && "Division by zero in Vector3_SIMD::Divide");
        if (b.m_storage.lanes[0] == 0.0f || b.m_storage.lanes[1] == 0.0f || b.m_storage.lanes[2] == 0.0f)
        {
            result = Zero();
            return;
        }
        const __m128 quotient = _mm_div_ps(a.m_storage.value, b.m_storage.value);
        result.m_storage.value = _mm_and_ps(quotient, _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1)));
    }

    /// @brief スカラーで除算した結果を出力先へ格納する。
    static void Divide(const Vector3_SIMD& value, const float scalar, Vector3_SIMD& result)
    {
        assert(scalar != 0.0f && "Division by zero in Vector3_SIMD::Divide");
        if (scalar == 0.0f)
        {
            result = value;
            return;
        }
        result.m_storage.value = _mm_div_ps(value.m_storage.value, _mm_set1_ps(scalar));
    }
};
