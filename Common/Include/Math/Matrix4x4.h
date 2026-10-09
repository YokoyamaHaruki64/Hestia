/*=============================================================================

File   : [Matrix4x4.h]
Desc   : 4x4行列の構造体の宣言

------------------------------------------------------------------------------

Date   : 2026/04/19
Author : Yokoyama Haruki

===============================================================================*/

#ifndef _MATRIX4X4_H_
#define _MATRIX4X4_H_

#include <cassert>

#include "math/Vector3.h"
#include "Math/MathCommon.h"

/// @brief row-major
/// @brief 行ベクトル (v * M)
/// @brief m[row][column]
/// @note SRT は S * R * T、親子合成は Local * ParentWorld。左手系
struct Matrix4x4
{
    float m[4][4]{};
    constexpr Matrix4x4() = default;
    constexpr Matrix4x4(
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33)
        :
        m
        {
            { m00, m01, m02, m03 },
            { m10, m11, m12, m13 },
            { m20, m21, m22, m23 },
            { m30, m31, m32, m33 }
        }
    {
    }

    // ===============================
    // 演算子オーバーロード
    // ===============================

    inline constexpr Matrix4x4 operator+(const Matrix4x4& other) const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                result.m[i][j] = m[i][j] + other.m[i][j];
            }
        }
        return result;
    }

    inline constexpr Matrix4x4& operator+=(const Matrix4x4& other)
    {
        *this = *this + other;
        return *this;
    }

    inline constexpr Matrix4x4 operator-(const Matrix4x4& other) const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                result.m[i][j] = m[i][j] - other.m[i][j];
            }
        }
        return result;
    }

    inline constexpr Matrix4x4& operator-=(const Matrix4x4& other)
    {
        *this = *this - other;
        return *this;
    }

    inline constexpr Matrix4x4 operator*(const Matrix4x4& other) const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                result.m[i][j] = m[i][0] * other.m[0][j] +
                    m[i][1] * other.m[1][j] +
                    m[i][2] * other.m[2][j] +
                    m[i][3] * other.m[3][j];
            }
        }
        return result;
    }

    inline constexpr Matrix4x4& operator*=(const Matrix4x4& other)
    {
        *this = *this * other;
        return *this;
    }

    inline constexpr Matrix4x4 operator*(float scalar) const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                result.m[i][j] = m[i][j] * scalar;
            }
        }
        return result;
    }

    inline constexpr Matrix4x4& operator*=(float scalar)
    {
        *this = *this * scalar;
        return *this;
    }

    inline constexpr Matrix4x4 operator/(float scalar) const
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Matrix4x4 operator/");
        if( scalar == 0.0f )
        {
            return *this; // 0除算の場合は元の行列を返す
        }

        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                result.m[i][j] = m[i][j] / scalar;
            }
        }
        return result;
    }

    inline constexpr Matrix4x4& operator/=(float scalar)
    {
        *this = *this / scalar;
        return *this;
    }

    // ===============================
    // 静的関数
    // ===============================

    /// @brief 単位行列を返す
    /// @return 4x4の単位行列
    inline static constexpr Matrix4x4 Identity()
    {
        return Matrix4x4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }

    /// @brief 平行移動行列を返す
    /// @param x X軸方向の移動量
    /// @param y Y軸方向の移動量
    /// @param z Z軸方向の移動量
    /// @return 4x4の平行移動行列
    inline static constexpr Matrix4x4 Translation(float x, float y, float z)
    {
        return Matrix4x4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            x, y, z, 1.0f
        );
    }

    /// @brief スケーリング行列を返す
    /// @param sx X軸方向のスケーリング量
    /// @param sy Y軸方向のスケーリング量
    /// @param sz Z軸方向のスケーリング量
    /// @return 4x4のスケーリング行列
    inline static constexpr Matrix4x4 Scaling(float sx, float sy, float sz)
    {
        return Matrix4x4(
            sx, 0.0f, 0.0f, 0.0f,
            0.0f, sy, 0.0f, 0.0f,
            0.0f, 0.0f, sz, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }

    /// @brief 等方スケーリング行列を返す
    /// @param scale スケーリング量（X軸、Y軸、Z軸に同じ値を使用）
    /// @return 4x4の等方スケーリング行列
    inline static constexpr Matrix4x4 ScalingUniform(float scale)
    {
        return Scaling(scale, scale, scale);
    }

    /// @brief X軸回転行列を返す
    /// @param angle 回転角度（ラジアン）
    /// @return 4x4のX軸回転行列
    inline static Matrix4x4 RotationX(float angle)
    {
        float cosA = Math::Cos(angle);
        float sinA = Math::Sin(angle);
        return Matrix4x4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, cosA, sinA, 0.0f,
            0.0f, -sinA, cosA, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }

    /// @brief Y軸回転行列を返す
    /// @param angle 回転角度（ラジアン）
    /// @return 4x4のY軸回転行列
    inline static Matrix4x4 RotationY(float angle)
    {
        float cosA = Math::Cos(angle);
        float sinA = Math::Sin(angle);
        return Matrix4x4(
            cosA, 0.0f, -sinA, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            sinA, 0.0f, cosA, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }

    /// @brief Z軸回転行列を返す
    /// @param angle 回転角度（ラジアン）
    /// @return 4x4のZ軸回転行列
    inline static Matrix4x4 RotationZ(float angle)
    {
        float cosA = cosf(angle);
        float sinA = sinf(angle);
        return Matrix4x4(
            cosA, sinA, 0.0f, 0.0f,
            -sinA, cosA, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }

    /// @brief ZXY順で回転する行列を返す
    /// @param pitch ピッチ角（X軸回転、ラジアン）
    /// @param yaw ヨー角（Y軸回転、ラジアン）
    /// @param roll ロール角（Z軸回転、ラジアン）
    /// @return 4x4のZXY順で回転する行列
    inline static Matrix4x4 RotationRollPitchYaw(float pitch, float yaw, float roll)
    {
        Matrix4x4 rotX = RotationX(pitch);
        Matrix4x4 rotY = RotationY(yaw);
        Matrix4x4 rotZ = RotationZ(roll);
        return rotZ * rotX * rotY; // ZXY順で回転
    }

    inline static Matrix4x4 LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up)
    {
        Vector3 zAxis = (target - eye).Normalized();
        Vector3 xAxis = up.Cross(zAxis).Normalized();
        Vector3 yAxis = zAxis.Cross(xAxis);
        return Matrix4x4(
            xAxis.x, yAxis.x, zAxis.x, 0.0f,
            xAxis.y, yAxis.y, zAxis.y, 0.0f,
            xAxis.z, yAxis.z, zAxis.z, 0.0f,
            -xAxis.Dot(eye), -yAxis.Dot(eye), -zAxis.Dot(eye), 1.0f
        );
    }

    inline static Matrix4x4 PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ)
    {
        float yScale = 1.0f / Math::Tan(fovY * 0.5f);
        float xScale = yScale / aspect;
        float Q = farZ / (farZ - nearZ);
        return Matrix4x4(
            xScale, 0.0f, 0.0f, 0.0f,
            0.0f, yScale, 0.0f, 0.0f,
            0.0f, 0.0f, Q, 1.0f,
            0.0f, 0.0f, -Q * nearZ, 0.0f
        );
    }

    /// @brief 左手系の正射影行列を返す
    /// @param width 視錐台の幅
    /// @param height 視錐台の高さ
    /// @param nearZ ニア平面の Z 座標
    /// @param farZ ファー平面の Z 座標
    /// @pre width と height は正、0 <= nearZ < farZ
    /// @return 行ベクトル用の正射影行列。深度は [0, 1]
    static constexpr Matrix4x4 OrthographicLH(float width, float height, float nearZ, float farZ)
    {
        assert(width > 0.0f && height > 0.0f && nearZ >= 0.0f && farZ > nearZ);

        const float depthScale = 1.0f / (farZ - nearZ);
        return Matrix4x4(
            2.0f / width, 0.0f, 0.0f, 0.0f,
            0.0f, 2.0f / height, 0.0f, 0.0f,
            0.0f, 0.0f, depthScale, 0.0f,
            0.0f, 0.0f, -nearZ * depthScale, 1.0f);
    }

    // ===============================
    // メンバ関数
    // ===============================

    /// @brief この行列に平行移動を適用する関数
    /// @param x X軸方向の移動量
    /// @param y Y軸方向の移動量
    /// @param z Z軸方向の移動量
    /// @return この行列への参照
    inline Matrix4x4& Translate(float x, float y, float z)
    {
        *this = *this * Translation(x, y, z);
        return *this;
    }

    /// @brief この行列にスケーリングを適用する関数
    /// @param sx X軸方向のスケーリング量
    /// @param sy Y軸方向のスケーリング量
    /// @param sz Z軸方向のスケーリング量
    /// @return この行列への参照
    inline Matrix4x4& Scale(float sx, float sy, float sz)
    {
        *this = *this * Scaling(sx, sy, sz);
        return *this;
    }

    /// @brief この行列に等方スケーリングを適用する関数
    /// @param scale スケーリング量（X軸、Y軸、Z軸に同じ値を使用）
    /// @return この行列への参照
    inline Matrix4x4& ScaleUniform(float scale)
    {
        *this = *this * ScalingUniform(scale);
        return *this;
    }

    /// @brief この行列に回転（X軸回転）を適用する関数
    /// @param angle 回転角度（ラジアン）
    /// @return この行列への参照
    inline Matrix4x4& RotateX(float angle)
    {
        *this = *this * RotationX(angle);
        return *this;
    }

    /// @brief この行列に回転（Y軸回転）を適用する関数
    /// @param angle 回転角度（ラジアン）
    /// @return この行列への参照
    inline Matrix4x4& RotateY(float angle)
    {
        *this = *this * RotationY(angle);
        return *this;
    }

    /// @brief この行列に回転（Z軸回転）を適用する関数
    /// @param angle 回転角度（ラジアン）
    /// @return この行列への参照
    inline Matrix4x4& RotateZ(float angle)
    {
        *this = *this * RotationZ(angle);
        return *this;
    }

    /// @brief この行列に回転（ZXY順）を適用する関数
    /// @param pitch ピッチ角（X軸回転、ラジアン）
    /// @param yaw ヨー角（Y軸回転、ラジアン）
    /// @param roll ロール角（Z軸回転、ラジアン）
    /// @return この行列への参照
    inline Matrix4x4& RotateRollPitchYaw(float pitch, float yaw, float roll)
    {
        *this = *this * RotationRollPitchYaw(pitch, yaw, roll);
        return *this;
    }

    /// @brief この行列の転置を返す関数
    /// @return 転置された行列のコピー
    inline Matrix4x4 Transpose() const
    {
        return Matrix4x4(
            m[0][0], m[1][0], m[2][0], m[3][0],
            m[0][1], m[1][1], m[2][1], m[3][1],
            m[0][2], m[1][2], m[2][2], m[3][2],
            m[0][3], m[1][3], m[2][3], m[3][3]
        );
    }
};

#endif // _MATRIX4X4_H_
