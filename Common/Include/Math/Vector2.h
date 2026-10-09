/*=============================================================================

 File   : [Vector2.h]
 Desc   : 2次元ベクトルの構造体の宣言

 ------------------------------------------------------------------------------

 Date   : 2026/04/19
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _VECTOR2_H_
#define _VECTOR2_H_

#include <cassert>
#include "Math/MathCommon.h"

/// @brief Left-Handed Coordinate System
/// @brief Right    : +X
/// @brief Up    : +Y
struct Vector2
{
    float x{ 0.0f };
    float y{ 0.0f };
    constexpr Vector2() = default;
    constexpr Vector2(float x, float y) : x(x), y(y) {}

    // ===============================
    // 演算子オーバーロード
    // ===============================
    inline constexpr Vector2 operator+(const Vector2& other) const
    {
        return Vector2(x + other.x, y + other.y);
    }

    inline constexpr Vector2& operator+=(const Vector2& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    inline constexpr Vector2 operator-(const Vector2& other) const
    {
        return Vector2(x - other.x, y - other.y);
    }

    inline constexpr Vector2& operator-=(const Vector2& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    inline constexpr Vector2 operator*(float scalar) const
    {
        return Vector2(x * scalar, y * scalar);
    }

    inline constexpr Vector2& operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    inline constexpr Vector2 operator/(float scalar) const
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Vector2 operator/");
        if (scalar == 0.0f)
        {
            return *this; // 0除算の場合は元のベクトルを返す
        }
        return Vector2(x / scalar, y / scalar);
    }

    inline constexpr Vector2& operator/=(float scalar)
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Vector2 operator/=");
        if (scalar == 0.0f)
        {
            return *this; // 0除算の場合は元のベクトルを返す
        }
        x /= scalar;
        y /= scalar;
        return *this;
    }

    // ===============================
    // 静的関数
    // ===============================

    /// @brief ゼロベクトルを返す関数
    /// @return (0, 0)のベクトル
    inline static constexpr Vector2 Zero()
    {
        return Vector2(0.0f, 0.0f);
    }

    /// @brief 全ての要素が1のベクトルを返す関数
    /// @return (1, 1)のベクトル
    inline static constexpr Vector2 One()
    {
        return Vector2(1.0f, 1.0f);
    }

    /// @brief 右方向の単位ベクトルを返す関数
    /// @return (1, 0)のベクトル
    inline static constexpr Vector2 Right()
    {
        return Vector2(1.0f, 0.0f);
    }

    /// @brief 左方向の単位ベクトルを返す関数
    /// @return (-1, 0)のベクトル
    inline static constexpr Vector2 Left()
    {
        return Vector2(-1.0f, 0.0f);
    }

    /// @brief 上方向の単位ベクトルを返す関数
    /// @return (0, 1)のベクトル
    inline static constexpr Vector2 Up()
    {
        return Vector2(0.0f, 1.0f);
    }

    /// @brief 下方向の単位ベクトルを返す関数
    /// @return (0, -1)のベクトル
    inline static constexpr Vector2 Down()
    {
        return Vector2(0.0f, -1.0f);
    }

    /// @brief 2つのベクトルのドット積を計算する関数
    /// @param a Vector2
    /// @param b Vector2
    /// @return ベクトルaとbのドット積
    inline static constexpr float Dot(const Vector2& a, const Vector2& b)
    {
        return a.x * b.x + a.y * b.y;
    }

    /// @brief 2つのベクトルのクロス積を計算する関数
    /// @param a Vector2
    /// @param b Vector2
    /// @return ベクトルaとbのクロス積（スカラー値）
    inline static constexpr float Cross(const Vector2& a, const Vector2& b)
    {
        return a.x * b.y - a.y * b.x;
    }

    /// @brief 2つのベクトルを線形補間する関数
    /// @param a 開始ベクトル
    /// @param b 終了ベクトル
    /// @param t 補間係数（0.0fから1.0fの範囲）
    /// @return ベクトルaとbの線形補間結果
    inline static constexpr Vector2 Lerp(const Vector2& a, const Vector2& b, float t)
    {
        return a + (b - a) * t;
    }

    /// @brief 2つのベクトルの要素ごとの積を計算する関数
    /// @param a Vector2
    /// @param b Vector2
    /// @return ベクトルaとbの要素ごとの積
    inline static constexpr Vector2 Multiply(const Vector2& a, const Vector2& b)
    {
        return Vector2(a.x * b.x, a.y * b.y);
    }

    /// @brief 2つのベクトルの要素ごとの商を計算する関数
    /// @param a Vector2
    /// @param b Vector2
    /// @return ベクトルaとbの要素ごとの商
    inline static constexpr Vector2 Divide(const Vector2& a, const Vector2& b)
    {
        // 0除算チェック
        assert(b.x != 0.0f && b.y != 0.0f && "Division by zero in Vector2 Divide");
        if (b.x == 0.0f || b.y == 0.0f)
        {
            return Vector2(0.0f, 0.0f); // 0除算の場合はゼロベクトルを返す
        }
        return Vector2(a.x / b.x, a.y / b.y);
    }

    /// @brief 2つのベクトル間の距離を計算する関数
    /// @param a Vector2(位置)
    /// @param b Vector2(位置)
    /// @return ベクトルaとbの距離
    inline static float Distance(const Vector2& a, const Vector2& b)
    {
        return (a - b).Length();
    }

    /// @brief 2つのベクトル間の距離の二乗を計算する関数
    /// @param a Vector2(位置)
    /// @param b Vector2(位置)
    /// @return ベクトルaとbの距離の二乗
    inline static float DistanceSqr(const Vector2& a, const Vector2& b)
    {
        Vector2 diff = a - b;
        return diff.x * diff.x + diff.y * diff.y;
    }

    /// @brief 2つのベクトル間の角度を計算する関数
    /// @param a Vector2(方向)
    /// @param b Vector2(方向)
    /// @return ベクトルaとbの角度（ラジアン）
    inline static float Angle(const Vector2& a, const Vector2& b)
    {
        const float dot = Dot(a.Normalized(), b.Normalized());
        return Math::Acos(Math::Clamp(dot, -1.0f, 1.0f));
    }

    // ===============================
    // メンバ関数
    // ===============================

    /// @brief このベクトルの長さを計算する関数
    /// @return このベクトルの長さ
    inline float Length() const
    {
        return Math::Sqrt(x * x + y * y);
    }

    /// @brief このベクトルの長さの二乗を計算する関数
    /// @return このベクトルの長さの二乗
    inline float LengthSqr() const
    {
        return x * x + y * y;
    }

    /// @brief ベクトルを正規化する関数（このベクトルは変更されない）
    /// @return 正規化された新しいベクトル
    inline Vector2 Normalized() const
    {
        float len = Length();
        if (len == 0.0f)
        {
            return Vector2(0.0f, 0.0f); // 長さが0の場合はゼロベクトルを返す
        }
        return *this / len;
    }

    /// @brief ベクトルを正規化する関数（このベクトル自体が変更される）
    /// @return 正規化されたこのベクトルへの参照
    inline Vector2& Normalize()
    {
        float len = Length();
        if (len == 0.0f)
        {
            x = 0.0f;
            y = 0.0f; // 長さが0の場合はゼロベクトルにする
        }
        else
        {
            x /= len;
            y /= len;
        }
        return *this;
    }

    /// @brief ベクトルを回転させた新しいベクトルを返す関数
    /// @param angle 回転角度（ラジアン）
    /// @return 回転された新しいベクトル
    inline Vector2 Rotated(float angle) const
    {
        float cosA = Math::Cos(angle);
        float sinA = Math::Sin(angle);
        return Vector2(
            x * cosA - y * sinA,
            x * sinA + y * cosA
        );
    }

    /// @brief このベクトルを回転させる関数（このベクトル自体が変更される）
    /// @param angle 回転角度（ラジアン）
    /// @return 回転されたこのベクトルへの参照
    inline Vector2& Rotate(float angle)
    {
        float cosA = Math::Cos(angle);
        float sinA = Math::Sin(angle);
        float newX = x * cosA - y * sinA;
        float newY = x * sinA + y * cosA;
        x = newX;
        y = newY;
        return *this;
    }

};

#endif // _VECTOR2_H_
