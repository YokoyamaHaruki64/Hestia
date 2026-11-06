/*=============================================================================

 File   : [Vector4.h]
 Desc   : 4次元ベクトルの構造体の宣言

 ------------------------------------------------------------------------------

 Date   : 2026/04/19
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _VECTOR4_H_
#define _VECTOR4_H_

/// @brief Left-Handed Coordinate System
/// @brief Right    : +X
/// @brief Up    : +Y
/// @brief Forward    : +Z
/// @brief Wは通常、同次座標やアルファ値などに使用される
struct Vector4
{
    float x{ 0.0f };
    float y{ 0.0f };
    float z{ 0.0f };
    float w{ 0.0f };
    constexpr Vector4() = default;
    constexpr Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    // ===============================
    // 演算子オーバーロード
    // ===============================
    inline constexpr Vector4 operator+(const Vector4& other) const
    {
        return Vector4(x + other.x, y + other.y, z + other.z, w + other.w);
    }

    inline constexpr Vector4& operator+=(const Vector4& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }

    inline constexpr Vector4 operator-(const Vector4& other) const
    {
        return Vector4(x - other.x, y - other.y, z - other.z, w - other.w);
    }

    inline constexpr Vector4& operator-=(const Vector4& other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }

    inline constexpr Vector4 operator*(float scalar) const
    {
        return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
    }

    inline constexpr Vector4& operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        w *= scalar;
        return *this;
    }

    inline constexpr Vector4 operator/(float scalar) const
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Vector4 operator/");
        if (scalar == 0.0f)
        {
            return *this; // 0除算の場合は元のベクトルを返す
        }
        return Vector4(x / scalar, y / scalar, z / scalar, w / scalar);
    }

    inline constexpr Vector4& operator/=(float scalar)
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Vector4 operator/=");
        if (scalar == 0.0f)
        {
            return *this; // 0除算の場合は元のベクトルを返す
        }
        x /= scalar;
        y /= scalar;
        z /= scalar;
        w /= scalar;
        return *this;
    }

    // ===============================
    // 静的関数
    // ===============================

    /// @brief ゼロベクトルを返す関数
    /// @return (0, 0, 0, 0)のベクトル
    inline static constexpr Vector4 Zero()
    {
        return Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    /// @brief 全ての要素が1のベクトルを返す関数
    /// @return (1, 1, 1, 1)のベクトル
    inline static constexpr Vector4 One()
    {
        return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    /// @brief 2つのベクトルのドット積を計算する関数
    /// @param a Vector4
    /// @param b Vector4
    /// @return ベクトルaとbのドット積
    inline static constexpr float Dot(const Vector4& a, const Vector4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }

    /// @brief 2つのベクトルを線形補間する関数
    /// @param a 開始ベクトル
    /// @param b 終了ベクトル
    /// @param t 補間係数（0.0fから1.0fの範囲）
    inline static constexpr Vector4 Lerp(const Vector4& a, const Vector4& b, float t)
    {
        return a + (b - a) * t;
    }

    /// @brief 2つのベクトルの要素ごとの積を計算する関数
    /// @param a Vector4
    /// @param b Vector4
    /// @return ベクトルaとbの要素ごとの積
    inline static constexpr Vector4 Multiply(const Vector4& a, const Vector4& b)
    {
        return Vector4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
    }

    /// @brief 2つのベクトルの要素ごとの商を計算する関数
    /// @param a Vector4
    /// @param b Vector4
    /// @return ベクトルaとbの要素ごとの商
    inline static constexpr Vector4 Divide(const Vector4& a, const Vector4& b)
    {
        // 0除算チェック
        assert(b.x != 0.0f && b.y != 0.0f && b.z != 0.0f && b.w != 0.0f && "Division by zero in Vector4 Divide");
        if (b.x == 0.0f || b.y == 0.0f || b.z == 0.0f || b.w == 0.0f)
        {
            return Vector4(0.0f, 0.0f, 0.0f, 0.0f); // 0除算の場合はゼロベクトルを返す
        }
        return Vector4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
    }

};

#endif // _VECTOR4_H_
