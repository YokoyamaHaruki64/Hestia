/*=============================================================================

 File   : [Vector3.h]
 Desc   : 3次元ベクトルの構造体の宣言

 ------------------------------------------------------------------------------

 Date   : 2026/04/19
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _VECTOR3_H_
#define _VECTOR3_H_

#include <cmath>
#include <cassert>

#include "Math/MathCommon.h"


/// @brief Left-Handed Coordinate System
/// @brief Right	: +X
/// @brief Up	: +Y
/// @brief Forward	: +Z
struct Vector3
{
    float x{ 0.0f };
    float y{ 0.0f };
    float z{ 0.0f };
    constexpr Vector3() = default;
    constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    // ===============================
    // 演算子オーバーロード
    // ===============================
    inline constexpr Vector3 operator+(const Vector3& other) const
    {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    inline constexpr Vector3& operator+=(const Vector3& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    inline constexpr Vector3 operator-(const Vector3& other) const
    {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    inline constexpr Vector3& operator-=(const Vector3& other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    inline constexpr Vector3 operator*(float scalar) const
    {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }

    inline constexpr Vector3& operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    inline constexpr Vector3 operator/(float scalar) const
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Vector3 operator/");
        if (scalar == 0.0f)
        {
            return *this; // 0除算の場合は元のベクトルを返す
        }
        return Vector3(x / scalar, y / scalar, z / scalar);
    }

    inline constexpr Vector3& operator/=(float scalar)
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Vector3 operator/=");
        if (scalar == 0.0f)
        {
            return *this; // 0除算の場合は元のベクトルを返す
        }
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    inline constexpr bool operator==(const Vector3& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }

    // ===============================
    // 静的関数
    // ===============================

    /// @brief ゼロベクトルを返す関数
    /// @return (0, 0, 0)のベクトル
    inline static constexpr Vector3 Zero()
    {
        return Vector3(0.0f, 0.0f, 0.0f);
    }

    /// @brief 全ての要素が1のベクトルを返す関数
    /// @return (1, 1, 1)のベクトル
    inline static constexpr Vector3 One()
    {
        return Vector3(1.0f, 1.0f, 1.0f);
    }

    /// @brief 右方向の単位ベクトルを返す関数
    /// @return (1, 0, 0)のベクトル
    inline static constexpr Vector3 Right()
    {
        return Vector3(1.0f, 0.0f, 0.0f);
    }

    /// @brief 左方向の単位ベクトルを返す関数
    /// @return (-1, 0, 0)のベクトル
    inline static constexpr Vector3 Left()
    {
        return Vector3(-1.0f, 0.0f, 0.0f);
    }

    /// @brief 上方向の単位ベクトルを返す関数
    /// @return (0, 1, 0)のベクトル
    inline static constexpr Vector3 Up()
    {
        return Vector3(0.0f, 1.0f, 0.0f);
    }

    /// @brief 下方向の単位ベクトルを返す関数
    /// @return (0, -1, 0)のベクトル
    inline static constexpr Vector3 Down()
    {
        return Vector3(0.0f, -1.0f, 0.0f);
    }

    /// @brief 前方向の単位ベクトルを返す関数
    /// @return (0, 0, 1)のベクトル
    inline static constexpr Vector3 Forward()
    {
        return Vector3(0.0f, 0.0f, 1.0f);
    }

    /// @brief 後方向の単位ベクトルを返す関数
    /// @return (0, 0, -1)のベクトル
    inline static constexpr Vector3 Backward()
    {
        return Vector3(0.0f, 0.0f, -1.0f);
    }

    /// @brief 2つのベクトルのドット積を計算する関数
    /// @param a Vector3
    /// @param b Vector3
    /// @return ベクトルaとbのドット積
    inline static constexpr float Dot(const Vector3& a, const Vector3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    /// @brief 2つのベクトルのクロス積を計算する関数
    /// @param a Vector3
    /// @param b Vector3
    /// @return ベクトルaとbのクロス積
    inline static constexpr Vector3 Cross(const Vector3& a, const Vector3& b)
    {
        return Vector3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    /// @brief 2つのベクトルを線形補間する関数
    /// @param a 開始ベクトル
    /// @param b 終了ベクトル
    /// @param t 補間係数（0.0fから1.0fの範囲）
    /// @return ベクトルaとbの線形補間結果
    inline static constexpr Vector3 Lerp(const Vector3& a, const Vector3& b, float t)
    {
        return a + (b - a) * t;
    }

    /// @brief 2つのベクトルの要素ごとの積を計算する関数
    /// @param a Vector3
    /// @param b Vector3
    /// @return ベクトルaとbの要素ごとの積
    inline static constexpr Vector3 Multiply(const Vector3& a, const Vector3& b)
    {
        return Vector3(a.x * b.x, a.y * b.y, a.z * b.z);
    }

    /// @brief 2つのベクトルの要素ごとの商を計算する関数
    /// @param a Vector3
    /// @param b Vector3
    /// @return ベクトルaとbの要素ごとの商
    inline static constexpr Vector3 Divide(const Vector3& a, const Vector3& b)
    {
        // 0除算チェック
        assert(b.x != 0.0f && b.y != 0.0f && b.z != 0.0f && "Division by zero in Vector3 Divide");
        if (b.x == 0.0f || b.y == 0.0f || b.z == 0.0f)
        {
            return Vector3(0.0f, 0.0f, 0.0f); // 0除算の場合はゼロベクトルを返す
        }
        return Vector3(a.x / b.x, a.y / b.y, a.z / b.z);
    }

    /// @brief 2つのベクトル間の距離を計算する関数
    /// @param a Vector3(位置)
    /// @param b Vector3(位置)
    /// @return ベクトルaとbの距離
    inline static float Distance(const Vector3& a, const Vector3& b)
    {
        return (a - b).Length();
    }

    /// @brief 2つのベクトル間の距離の二乗を計算する関数
    /// @param a Vector3(位置)
    /// @param b Vector3(位置)
    /// @return ベクトルaとbの距離の二乗
    inline static constexpr float DistanceSqr(const Vector3& a, const Vector3& b)
    {
        Vector3 diff = a - b;
        return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    }

    /// @brief 2つのベクトル間の角度を計算する関数
    /// @param a Vector3(方向)
    /// @param b Vector3(方向)
    /// @return ベクトルaとbの角度（ラジアン）
    inline static float Angle(const Vector3& a, const Vector3& b)
    {
        const float dot = Dot(a.Normalized(), b.Normalized());
        return Math::Acos(Math::Clamp(dot, -1.0f, 1.0f));
    }

    /// @brief 2つのベクトルの反射ベクトルを計算する関数
    /// @param inDir 入射ベクトル
    /// @param normal 法線ベクトル
    /// @return 反射ベクトル
    inline static Vector3 Reflect(const Vector3& inDir, const Vector3& normal)
    {
        return inDir + normal * -(2.0f * Dot(inDir, normal));
    }

    /// @brief 2つのベクトルの反射ベクトルを計算する関数（反発係数付き）
    /// @param inDir 入射ベクトル
    /// @param normal 法線ベクトル
    /// @param e 反発係数（0.0fから1.0fの範囲）
    /// @return 反射ベクトル
    inline static Vector3 Reflect(const Vector3& inDir, const Vector3& normal,float e)
    {
        return inDir + normal * -(1.0f + e) * Dot(inDir, normal);
    }

    /// @brief 法線ベクトルから右方向のベクトルを生成する関数
    /// @param normal 法線ベクトル
    /// @return 右方向のベクトル
    inline static Vector3 CreateRight(const Vector3& normal)
    {
        Vector3 n = normal.Normalized();
        bool isParallel = Vector3::Dot(n, Vector3::Up()) > 0.999f;
        bool isAntiParallel = Vector3::Dot(n, Vector3::Up()) < -0.999f;
        Vector3 refUp = isParallel || isAntiParallel ? Vector3::Forward() : Vector3::Up();
        Vector3 right = Vector3::Cross(refUp, n).Normalized();

        return isParallel ? right * -1.0f : right; // 上方向と平行な場合は右方向を反転させる
    }

    /// @brief 法線ベクトルから上方向のベクトルを生成する関数
    /// @param normal 法線ベクトル
    /// @return 上方向のベクトル
    inline static Vector3 CreateUp(const Vector3& normal)
    {
        Vector3 n = normal.Normalized();
        bool isParallel = Vector3::Dot(n, Vector3::Up()) > 0.999f;
        bool isAntiParallel = Vector3::Dot(n, Vector3::Up()) < -0.999f;
        Vector3 refUp = isParallel || isAntiParallel ? Vector3::Forward() : Vector3::Up();
        Vector3 right = Vector3::Cross(refUp, n).Normalized();
        Vector3 up = Vector3::Cross(n, right).Normalized();
        return isParallel ? up * -1.0f : up; // 上方向と平行な場合は上方向を反転させる
    }

    /// @brief 法線ベクトルから右方向と上方向のベクトルを生成する関数
    /// @param normal 法線ベクトル
    /// @param outRight 生成された右方向のベクトル
    /// @param outUp 生成された上方向のベクトル
    inline static void CreateRightUp(const Vector3& normal, Vector3& outRight, Vector3& outUp)
    {
        Vector3 n = normal.Normalized();
        bool isParallel = Vector3::Dot(n, Vector3::Up()) > 0.999f;
        bool isAntiParallel = Vector3::Dot(n, Vector3::Up()) < -0.999f;
        Vector3 refUp = isParallel || isAntiParallel ? Vector3::Forward() : Vector3::Up();
        outRight = Vector3::Cross(refUp, n).Normalized();
        outUp = Vector3::Cross(n, outRight).Normalized();
        if( isParallel )
        {
            outRight *= -1.0f; // 上方向と平行な場合は右方向を反転させる
        }
    }

    // ===============================
    // メンバ関数
    // ===============================

    /// @brief このベクトルの長さを計算する関数
    /// @return このベクトルの長さ
    inline float Length() const
    {
        return sqrtf(x * x + y * y + z * z);
    }

    /// @brief このベクトルの長さの二乗を計算する関数
    /// @return このベクトルの長さの二乗
    inline float LengthSqr() const
    {
        return x * x + y * y + z * z;
    }

    /// @brief ベクトルを正規化する関数（このベクトルは変更されない）
    /// @return 正規化された新しいベクトル
    inline Vector3 Normalized() const
    {
        float len = Length();
        if (len == 0.0f)
        {
            return Vector3(0.0f, 0.0f, 0.0f); // 長さが0の場合はゼロベクトルを返す
        }
        return *this / len;
    }

    /// @brief ベクトルを正規化する関数（このベクトル自体が変更される）
    /// @return 正規化されたこのベクトルへの参照
    inline Vector3& Normalize()
    {
        float len = Length();
        if (len == 0.0f)
        {
            x = 0.0f;
            y = 0.0f;
            z = 0.0f; // 長さが0の場合はゼロベクトルにする
        }
        else
        {
            x /= len;
            y /= len;
            z /= len;
        }
        return *this;
    }

    /// @brief このベクトルと他のベクトルのクロス積を計算する関数
    /// @param other 他のベクトル
    /// @return クロス積の結果のベクトル
    inline Vector3 Cross(const Vector3& other) const
    {
        return Vector3::Cross(*this, other);
    }

    /// @brief このベクトルと他のベクトルのドット積を計算する関数
    /// @param other 他のベクトル
    /// @return ドット積の結果のスカラー値
    inline float Dot(const Vector3& other) const
    {
        return Vector3::Dot(*this, other);
    }

};

#endif // _VECTOR3_H_
