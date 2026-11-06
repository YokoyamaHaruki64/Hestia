#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>

#include <immintrin.h>

#include "math/Vector3_SIMD.h"

#include "Math/MathCommon.h"

struct Matrix4x4;

/// @brief 行ベクトル用の行優先行列。各行を1つのSIMDレジスタに格納する。
/// @brief 行ベクトル・row-major の 4x4 行列。一行を一 SIMD レジスタに保持
/// @note SRT は S * R * T、親子合成は Local * ParentWorld、ベクトル変換は v * M
struct Matrix4x4_SIMD
{
private:
    union alignas(16) RowStorage
    {
        __m128 value;
        float lanes[4];
    } m_rows[4];

public:
    /// @brief 行内の列へ書き込み可能なアクセスを提供する。
    struct RowReference
    {
        RowStorage& data;
        /// @brief 指定した添字の要素への書き込み可能な参照を返す。
        float& operator[](const std::size_t column) const
        {
            assert(column < 4);
            return data.lanes[column];
        }
    };

    /// @brief 行内の列への読み取り専用アクセスを提供する。
    struct ConstRowReference
    {
        const RowStorage& data;
        /// @brief 指定した添字の要素の値を返す。
        float operator[](const std::size_t column) const
        {
            assert(column < 4);
            return data.lanes[column];
        }
    };

    /// @brief ゼロ初期化したSIMD行列を作る。
    Matrix4x4_SIMD()
    {
        m_rows[0].value = _mm_setzero_ps();
        m_rows[1].value = _mm_setzero_ps();
        m_rows[2].value = _mm_setzero_ps();
        m_rows[3].value = _mm_setzero_ps();
    }
    /// @brief 指定した16要素からSIMD行列を作る。
    Matrix4x4_SIMD(
        const float m00, const float m01, const float m02, const float m03,
        const float m10, const float m11, const float m12, const float m13,
        const float m20, const float m21, const float m22, const float m23,
        const float m30, const float m31, const float m32, const float m33)
    {
        m_rows[0].value = _mm_setr_ps(m00, m01, m02, m03);
        m_rows[1].value = _mm_setr_ps(m10, m11, m12, m13);
        m_rows[2].value = _mm_setr_ps(m20, m21, m22, m23);
        m_rows[3].value = _mm_setr_ps(m30, m31, m32, m33);
    }

    /// @brief 既存型から値を読み込む。
    static Matrix4x4_SIMD Load(const Matrix4x4& source);
    /// @brief 既存型の値として返す。
    Matrix4x4 Store() const;
    /// @brief 既存型の出力先へ値を保存する。
    void Store(Matrix4x4& destination) const;

    /// @brief 指定した行への書き込み可能なアクセスを返す。
    RowReference operator[](const std::size_t row)
    {
        assert(row < 4);
        return RowReference{ m_rows[row] };
    }

    /// @brief 指定した行への読み取り専用アクセスを返す。
    ConstRowReference operator[](const std::size_t row) const
    {
        assert(row < 4);
        return ConstRowReference{ m_rows[row] };
    }

    /// @brief 一次元添字で指定した要素への書き込み可能な参照を返す。
    float& At(const std::size_t index)
    {
        assert(index < 16);
        return m_rows[index / 4].lanes[index % 4];
    }

    /// @brief 一次元添字で指定した要素の値を返す。
    float At(const std::size_t index) const
    {
        assert(index < 16);
        return m_rows[index / 4].lanes[index % 4];
    }

private:
    Matrix4x4_SIMD(const __m128 row0, const __m128 row1, const __m128 row2, const __m128 row3)
    {
        m_rows[0].value = row0;
        m_rows[1].value = row1;
        m_rows[2].value = row2;
        m_rows[3].value = row3;
    }

public:
    /// @brief 他の行列を加算してこの行列を更新する。
    Matrix4x4_SIMD& operator+=(const Matrix4x4_SIMD& other)
    {
        for (std::size_t row = 0; row < 4; ++row)
        {
            m_rows[row].value = _mm_add_ps(m_rows[row].value, other.m_rows[row].value);
        }
        return *this;
    }

    /// @brief 他の行列を減算してこの行列を更新する。
    Matrix4x4_SIMD& operator-=(const Matrix4x4_SIMD& other)
    {
        for (std::size_t row = 0; row < 4; ++row)
        {
            m_rows[row].value = _mm_sub_ps(m_rows[row].value, other.m_rows[row].value);
        }
        return *this;
    }

    /// @brief 他の行列との積でこの行列を更新する。
    Matrix4x4_SIMD& operator*=(const Matrix4x4_SIMD& other)
    {
        Multiply(*this, other, *this);
        return *this;
    }

    /// @brief スカラー倍してこの行列を更新する。
    Matrix4x4_SIMD& operator*=(const float scalar)
    {
        for (std::size_t row = 0; row < 4; ++row)
        {
            m_rows[row].value = _mm_mul_ps(m_rows[row].value, _mm_set1_ps(scalar));
        }
        return *this;
    }

    /// @brief スカラーで除算してこの行列を更新する。
    Matrix4x4_SIMD& operator/=(const float scalar)
    {
        assert(scalar != 0.0f && "Division by zero in Matrix4x4_SIMD operator/=");
        if (scalar != 0.0f)
        {
            const __m128 divisor = _mm_set1_ps(scalar);
            for (std::size_t row = 0; row < 4; ++row)
            {
                m_rows[row].value = _mm_div_ps(m_rows[row].value, divisor);
            }
        }
        return *this;
    }

    /// @brief 単位行列を返す。
    static Matrix4x4_SIMD Identity()
    {
        return Matrix4x4_SIMD(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    /// @brief 指定量だけ平行移動する行列を返す。
    static Matrix4x4_SIMD Translation(const float x, const float y, const float z)
    {
        return Matrix4x4_SIMD(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            x, y, z, 1.0f);
    }

    /// @brief 各軸の拡大率を指定した行列を返す。
    static Matrix4x4_SIMD Scaling(const float sx, const float sy, const float sz)
    {
        return Matrix4x4_SIMD(
            sx, 0.0f, 0.0f, 0.0f,
            0.0f, sy, 0.0f, 0.0f,
            0.0f, 0.0f, sz, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    /// @brief 全軸を同率で拡大する行列を返す。
    static Matrix4x4_SIMD ScalingUniform(const float scale)
    {
        return Scaling(scale, scale, scale);
    }

    /// @brief X軸回転行列を返す。
    static Matrix4x4_SIMD RotationX(const float angle)
    {
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        return Matrix4x4_SIMD(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, cosine, sine, 0.0f,
            0.0f, -sine, cosine, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    /// @brief Y軸回転行列を返す。
    static Matrix4x4_SIMD RotationY(const float angle)
    {
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        return Matrix4x4_SIMD(
            cosine, 0.0f, -sine, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            sine, 0.0f, cosine, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    /// @brief Z軸回転行列を返す。
    static Matrix4x4_SIMD RotationZ(const float angle)
    {
        const float cosine = Math::Cos(angle);
        const float sine = Math::Sin(angle);
        return Matrix4x4_SIMD(
            cosine, sine, 0.0f, 0.0f,
            -sine, cosine, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    /// @brief ZXY順で合成した回転行列を返す。
    static Matrix4x4_SIMD RotationRollPitchYaw(const float pitch, const float yaw, const float roll)
    {
        const Matrix4x4_SIMD rotationZ = RotationZ(roll);
        const Matrix4x4_SIMD rotationX = RotationX(pitch);
        const Matrix4x4_SIMD rotationY = RotationY(yaw);
        Matrix4x4_SIMD result;
        Multiply(rotationZ, rotationX, result);
        Multiply(result, rotationY, result);
        return result;
    }

    /// @brief 左手系のビュー行列を作る。
    static Matrix4x4_SIMD LookAtLH(const Vector3_SIMD& eye, const Vector3_SIMD& target, const Vector3_SIMD& up)
    {
        Vector3_SIMD zAxis = target;
        zAxis -= eye;
        zAxis.Normalize();
        Vector3_SIMD xAxis;
        Vector3_SIMD::Cross(up, zAxis, xAxis);
        xAxis.Normalize();
        Vector3_SIMD yAxis;
        Vector3_SIMD::Cross(zAxis, xAxis, yAxis);
        return Matrix4x4_SIMD(
            xAxis.x(), yAxis.x(), zAxis.x(), 0.0f,
            xAxis.y(), yAxis.y(), zAxis.y(), 0.0f,
            xAxis.z(), yAxis.z(), zAxis.z(), 0.0f,
            -Vector3_SIMD::Dot(xAxis, eye), -Vector3_SIMD::Dot(yAxis, eye), -Vector3_SIMD::Dot(zAxis, eye), 1.0f);
    }

    /// @brief 左手系の透視投影行列を作る。
    static Matrix4x4_SIMD PerspectiveFovLH(const float fovY, const float aspect, const float nearZ, const float farZ)
    {
        const float yScale = 1.0f / Math::Tan(fovY * 0.5f);
        const float xScale = yScale / aspect;
        const float q = farZ / (farZ - nearZ);
        return Matrix4x4_SIMD(
            xScale, 0.0f, 0.0f, 0.0f,
            0.0f, yScale, 0.0f, 0.0f,
            0.0f, 0.0f, q, 1.0f,
            0.0f, 0.0f, -q * nearZ, 0.0f);
    }

    /// @brief この行列に平行移動を適用する。
    Matrix4x4_SIMD& Translate(const float x, const float y, const float z)
    {
        const Matrix4x4_SIMD translation = Translation(x, y, z);
        Multiply(*this, translation, *this);
        return *this;
    }

    /// @brief この行列に拡大縮小を適用する。
    Matrix4x4_SIMD& Scale(const float sx, const float sy, const float sz)
    {
        const Matrix4x4_SIMD scaling = Scaling(sx, sy, sz);
        Multiply(*this, scaling, *this);
        return *this;
    }

    /// @brief この行列に等方拡大縮小を適用する。
    Matrix4x4_SIMD& ScaleUniform(const float scale)
    {
        const Matrix4x4_SIMD scaling = ScalingUniform(scale);
        Multiply(*this, scaling, *this);
        return *this;
    }

    /// @brief この行列にX軸回転を適用する。
    Matrix4x4_SIMD& RotateX(const float angle)
    {
        const Matrix4x4_SIMD rotation = RotationX(angle);
        Multiply(*this, rotation, *this);
        return *this;
    }

    /// @brief この行列にY軸回転を適用する。
    Matrix4x4_SIMD& RotateY(const float angle)
    {
        const Matrix4x4_SIMD rotation = RotationY(angle);
        Multiply(*this, rotation, *this);
        return *this;
    }

    /// @brief この行列にZ軸回転を適用する。
    Matrix4x4_SIMD& RotateZ(const float angle)
    {
        const Matrix4x4_SIMD rotation = RotationZ(angle);
        Multiply(*this, rotation, *this);
        return *this;
    }

    /// @brief この行列にZXY順の回転を適用する。
    Matrix4x4_SIMD& RotateRollPitchYaw(const float pitch, const float yaw, const float roll)
    {
        const Matrix4x4_SIMD rotation = RotationRollPitchYaw(pitch, yaw, roll);
        Multiply(*this, rotation, *this);
        return *this;
    }

    /// @brief 転置行列を出力先へ格納する。
    void Transpose(Matrix4x4_SIMD& result) const
    {
        const __m128 row0 = m_rows[0].value;
        const __m128 row1 = m_rows[1].value;
        const __m128 row2 = m_rows[2].value;
        const __m128 row3 = m_rows[3].value;
        __m128 column0 = row0;
        __m128 column1 = row1;
        __m128 column2 = row2;
        __m128 column3 = row3;
        _MM_TRANSPOSE4_PS(column0, column1, column2, column3);
        result.m_rows[0].value = column0;
        result.m_rows[1].value = column1;
        result.m_rows[2].value = column2;
        result.m_rows[3].value = column3;
    }

    /// @brief 2つの行列の和を出力先へ格納する。
    static void Add(const Matrix4x4_SIMD& a, const Matrix4x4_SIMD& b, Matrix4x4_SIMD& result)
    {
        for (std::size_t row = 0; row < 4; ++row)
        {
            result.m_rows[row].value = _mm_add_ps(a.m_rows[row].value, b.m_rows[row].value);
        }
    }

    /// @brief 2つの行列の差を出力先へ格納する。
    static void Subtract(const Matrix4x4_SIMD& a, const Matrix4x4_SIMD& b, Matrix4x4_SIMD& result)
    {
        for (std::size_t row = 0; row < 4; ++row)
        {
            result.m_rows[row].value = _mm_sub_ps(a.m_rows[row].value, b.m_rows[row].value);
        }
    }

    /// @brief 2つの行列を乗算して出力先へ格納する。
    static void Multiply(const Matrix4x4_SIMD& a, const Matrix4x4_SIMD& b, Matrix4x4_SIMD& result)
    {
        const __m128 lhs0 = a.m_rows[0].value;
        const __m128 lhs1 = a.m_rows[1].value;
        const __m128 lhs2 = a.m_rows[2].value;
        const __m128 lhs3 = a.m_rows[3].value;
        const __m128 x0 = _mm_shuffle_ps(lhs0, lhs0, _MM_SHUFFLE(0, 0, 0, 0));
        const __m128 y0 = _mm_shuffle_ps(lhs0, lhs0, _MM_SHUFFLE(1, 1, 1, 1));
        const __m128 z0 = _mm_shuffle_ps(lhs0, lhs0, _MM_SHUFFLE(2, 2, 2, 2));
        const __m128 w0 = _mm_shuffle_ps(lhs0, lhs0, _MM_SHUFFLE(3, 3, 3, 3));
        const __m128 x1 = _mm_shuffle_ps(lhs1, lhs1, _MM_SHUFFLE(0, 0, 0, 0));
        const __m128 y1 = _mm_shuffle_ps(lhs1, lhs1, _MM_SHUFFLE(1, 1, 1, 1));
        const __m128 z1 = _mm_shuffle_ps(lhs1, lhs1, _MM_SHUFFLE(2, 2, 2, 2));
        const __m128 w1 = _mm_shuffle_ps(lhs1, lhs1, _MM_SHUFFLE(3, 3, 3, 3));
        const __m128 x2 = _mm_shuffle_ps(lhs2, lhs2, _MM_SHUFFLE(0, 0, 0, 0));
        const __m128 y2 = _mm_shuffle_ps(lhs2, lhs2, _MM_SHUFFLE(1, 1, 1, 1));
        const __m128 z2 = _mm_shuffle_ps(lhs2, lhs2, _MM_SHUFFLE(2, 2, 2, 2));
        const __m128 w2 = _mm_shuffle_ps(lhs2, lhs2, _MM_SHUFFLE(3, 3, 3, 3));
        const __m128 x3 = _mm_shuffle_ps(lhs3, lhs3, _MM_SHUFFLE(0, 0, 0, 0));
        const __m128 y3 = _mm_shuffle_ps(lhs3, lhs3, _MM_SHUFFLE(1, 1, 1, 1));
        const __m128 z3 = _mm_shuffle_ps(lhs3, lhs3, _MM_SHUFFLE(2, 2, 2, 2));
        const __m128 w3 = _mm_shuffle_ps(lhs3, lhs3, _MM_SHUFFLE(3, 3, 3, 3));

        __m128 result0 = _mm_mul_ps(x0, b.m_rows[0].value);
        result0 = _mm_add_ps(result0, _mm_mul_ps(y0, b.m_rows[1].value));
        result0 = _mm_add_ps(result0, _mm_mul_ps(z0, b.m_rows[2].value));
        result0 = _mm_add_ps(result0, _mm_mul_ps(w0, b.m_rows[3].value));
        __m128 result1 = _mm_mul_ps(x1, b.m_rows[0].value);
        result1 = _mm_add_ps(result1, _mm_mul_ps(y1, b.m_rows[1].value));
        result1 = _mm_add_ps(result1, _mm_mul_ps(z1, b.m_rows[2].value));
        result1 = _mm_add_ps(result1, _mm_mul_ps(w1, b.m_rows[3].value));
        __m128 result2 = _mm_mul_ps(x2, b.m_rows[0].value);
        result2 = _mm_add_ps(result2, _mm_mul_ps(y2, b.m_rows[1].value));
        result2 = _mm_add_ps(result2, _mm_mul_ps(z2, b.m_rows[2].value));
        result2 = _mm_add_ps(result2, _mm_mul_ps(w2, b.m_rows[3].value));
        __m128 result3 = _mm_mul_ps(x3, b.m_rows[0].value);
        result3 = _mm_add_ps(result3, _mm_mul_ps(y3, b.m_rows[1].value));
        result3 = _mm_add_ps(result3, _mm_mul_ps(z3, b.m_rows[2].value));
        result3 = _mm_add_ps(result3, _mm_mul_ps(w3, b.m_rows[3].value));
        result.m_rows[0].value = result0;
        result.m_rows[1].value = result1;
        result.m_rows[2].value = result2;
        result.m_rows[3].value = result3;
    }

    /// @brief スカラー倍した結果を出力先へ格納する。
    static void Multiply(const Matrix4x4_SIMD& value, const float scalar, Matrix4x4_SIMD& result)
    {
        const __m128 factor = _mm_set1_ps(scalar);
        result.m_rows[0].value = _mm_mul_ps(value.m_rows[0].value, factor);
        result.m_rows[1].value = _mm_mul_ps(value.m_rows[1].value, factor);
        result.m_rows[2].value = _mm_mul_ps(value.m_rows[2].value, factor);
        result.m_rows[3].value = _mm_mul_ps(value.m_rows[3].value, factor);
    }

    /// @brief スカラーで除算して出力先へ格納する。
    static void Divide(const Matrix4x4_SIMD& value, const float scalar, Matrix4x4_SIMD& result)
    {
        assert(scalar != 0.0f && "Division by zero in Matrix4x4_SIMD::Divide");
        if (scalar == 0.0f)
        {
            result = value;
            return;
        }
        const __m128 divisor = _mm_set1_ps(scalar);
        result.m_rows[0].value = _mm_div_ps(value.m_rows[0].value, divisor);
        result.m_rows[1].value = _mm_div_ps(value.m_rows[1].value, divisor);
        result.m_rows[2].value = _mm_div_ps(value.m_rows[2].value, divisor);
        result.m_rows[3].value = _mm_div_ps(value.m_rows[3].value, divisor);
    }
};

