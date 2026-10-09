/*=============================================================================

 File   : [matrix3x3.h]
 Desc   : 3x3行列の構造体

 ------------------------------------------------------------------------------

 Date   : 2026/04/19
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _MATRIX3X3_H_
#define _MATRIX3X3_H_

#include <assert.h>
#include "Math/MathCommon.h"

/// @brief row-major
/// @brief 行ベクトル (v * M)
/// @brief m[row][column]
struct Matrix3x3
{
    float m[3][3]{};
    constexpr Matrix3x3() = default;
    constexpr Matrix3x3(
        float m00, float m01, float m02,
        float m10, float m11, float m12,
        float m20, float m21, float m22)
    {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
    }

    // ===============================
    // 演算子オーバーロード
    // ===============================

    inline constexpr Matrix3x3 operator+(const Matrix3x3& other) const
    {
        Matrix3x3 result;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                result.m[i][j] = m[i][j] + other.m[i][j];
            }
        }
        return result;
    }

    inline constexpr Matrix3x3& operator+=(const Matrix3x3& other)
    {
        *this = *this + other;
        return *this;
    }

    inline constexpr Matrix3x3 operator-(const Matrix3x3& other) const
    {
        Matrix3x3 result;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                result.m[i][j] = m[i][j] - other.m[i][j];
            }
        }
        return result;
    }

    inline constexpr Matrix3x3& operator-=(const Matrix3x3& other)
    {
        *this = *this - other;
        return *this;
    }

    inline constexpr Matrix3x3 operator*(const Matrix3x3& other) const
    {
        Matrix3x3 result;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                result.m[i][j] = m[i][0] * other.m[0][j] +
                    m[i][1] * other.m[1][j] +
                    m[i][2] * other.m[2][j];
            }
        }
        return result;
    }

    inline constexpr Matrix3x3& operator*=(const Matrix3x3& other)
    {
        *this = *this * other;
        return *this;
    }

    inline constexpr Matrix3x3 operator*(float scalar) const
    {
        Matrix3x3 result;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                result.m[i][j] = m[i][j] * scalar;
            }
        }
        return result;
    }

    inline constexpr Matrix3x3& operator*=(float scalar)
    {
        *this = *this * scalar;
        return *this;
    }

    inline constexpr Matrix3x3 operator/(float scalar) const
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Matrix3x3 operator/");
        if(scalar == 0.0f)
            {
                return *this; // 0除算の場合は元の行列を返す
        }

        Matrix3x3 result;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                result.m[i][j] = m[i][j] / scalar;
            }
        }
        return result;
    }

    inline constexpr Matrix3x3& operator/=(float scalar)
    {
        *this = *this / scalar;
        return *this;
    }

    // ===============================
    // 静的関数
    // ===============================

    /// @brief 単位行列を返す関数
    /// @return 3x3の単位行列
    inline static constexpr Matrix3x3 Identity()
    {
        return Matrix3x3(
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 1.0f
        );
    }

    /// @brief 平行移動行列を返す関数
    /// @param tx X軸方向の移動量
    /// @param ty Y軸方向の移動量
    /// @return 3x3の平行移動行列
    inline static constexpr Matrix3x3 Translation(float tx, float ty)
    {
        return Matrix3x3(
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            tx, ty, 1.0f
        );
    }

    /// @brief スケーリング行列を返す関数
    /// @param sx X軸方向のスケーリング量
    /// @param sy Y軸方向のスケーリング量
    /// @return 3x3のスケーリング行列
    inline static constexpr Matrix3x3 Scaling(float sx, float sy)
    {
        return Matrix3x3(
            sx, 0.0f, 0.0f,
            0.0f, sy, 0.0f,
            0.0f, 0.0f, 1.0f
        );
    }

    /// @brief 等方スケーリング行列を返す関数
    /// @param scale スケーリング量（X軸とY軸に同じ値を使用）
    /// @return 3x3の等方スケーリング行列
    inline static constexpr Matrix3x3 ScalingUniform(float scale)
    {
        return Scaling(scale, scale);
    }

    /// @brief 回転行列を返す関数
    /// @param angle 回転角度（ラジアン）
    /// @return 3x3の回転行列
    inline static Matrix3x3 Rotation(float angle)
    {
        float cosA = Math::Cos(angle);
        float sinA = Math::Sin(angle);
        return Matrix3x3(
            cosA, sinA, 0.0f,
            -sinA, cosA, 0.0f,
            0.0f, 0.0f, 1.0f
        );
    }

    // ===============================
    // メンバ関数
    // ===============================

    /// @brief この行列に平行移動を適用する関数
    /// @param tx X軸方向の移動量
    /// @param ty Y軸方向の移動量
    /// @return この行列への参照
    inline Matrix3x3& Translate(float tx, float ty)
    {
        *this = *this * Translation(tx, ty);
        return *this;
    }

    /// @brief この行列にスケーリングを適用する関数
    /// @param sx X軸方向のスケーリング量
    /// @param sy Y軸方向のスケーリング量
    /// @return この行列への参照
    inline Matrix3x3& Scale(float sx, float sy)
    {
        *this = *this * Scaling(sx, sy);
        return *this;
    }

    /// @brief この行列に等方スケーリングを適用する関数
    /// @param scale スケーリング量（X軸とY軸に同じ値を使用）
    /// @return この行列への参照
    inline Matrix3x3& ScaleUniform(float scale)
    {
        return Scale(scale, scale);
    }

    /// @brief この行列に回転を適用する関数
    /// @param angle 回転角度（ラジアン）
    /// @return この行列への参照
    inline Matrix3x3& Rotate(float angle)
    {
        *this = *this * Rotation(angle);
        return *this;
    }

    /// @brief この行列の転置を返す関数
    /// @return この行列の転置のコピー
    inline Matrix3x3 Transpose() const
    {
        Matrix3x3 result;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                result.m[i][j] = m[j][i];
            }
        }
        return result;
    }

};

#endif // _MATRIX3X3_H_
