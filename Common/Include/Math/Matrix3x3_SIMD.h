#pragma once

#include <cassert>
#include <cstddef>

#include <immintrin.h>

#include "Math/MathCommon.h"

struct Matrix3x3;

/// @brief 各行を1つのSIMDレジスタに格納する行優先行列。
/// @note 行ベクトル v * M、2D の変換順序は S * R * T
struct Matrix3x3_SIMD
{
private:
    union alignas(16) RowStorage
    {
        __m128 value;
        float lanes[4];
    } m_rows[3];

public:
    /// @brief 行内の列へ書き込み可能なアクセスを提供する。
    struct RowReference
    {
        RowStorage& data;
        /// @brief 指定した添字の要素への書き込み可能な参照を返す。
        float& operator[](const std::size_t column) const
        {
            assert(column < 3);
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
            assert(column < 3);
            return data.lanes[column];
        }
    };

    /// @brief ゼロ初期化したSIMD行列を作る。
    Matrix3x3_SIMD()
    {
        m_rows[0].value = _mm_setzero_ps();
        m_rows[1].value = _mm_setzero_ps();
        m_rows[2].value = _mm_setzero_ps();
    }
    /// @brief 指定した9要素からSIMD行列を作る。
    Matrix3x3_SIMD(
        const float m00, const float m01, const float m02,
        const float m10, const float m11, const float m12,
        const float m20, const float m21, const float m22)
    {
        m_rows[0].value = _mm_setr_ps(m00, m01, m02, 0.0f);
        m_rows[1].value = _mm_setr_ps(m10, m11, m12, 0.0f);
        m_rows[2].value = _mm_setr_ps(m20, m21, m22, 0.0f);
    }

    /// @brief 既存型から値を読み込む。
    static Matrix3x3_SIMD Load(const Matrix3x3& source);
    /// @brief 既存型の値として返す。
    Matrix3x3 Store() const;
    /// @brief 既存型の出力先へ値を保存する。
    void Store(Matrix3x3& destination) const;

    /// @brief 指定した行への書き込み可能なアクセスを返す。
    RowReference operator[](const std::size_t row)
    {
        assert(row < 3);
        return RowReference{ m_rows[row] };
    }

    /// @brief 指定した行への読み取り専用アクセスを返す。
    ConstRowReference operator[](const std::size_t row) const
    {
        assert(row < 3);
        return ConstRowReference{ m_rows[row] };
    }

    /// @brief 一次元添字で指定した要素への書き込み可能な参照を返す。
    float& At(const std::size_t index)
    {
        assert(index < 9);
        return m_rows[index / 3].lanes[index % 3];
    }

    /// @brief 一次元添字で指定した要素の値を返す。
    float At(const std::size_t index) const
    {
        assert(index < 9);
        return m_rows[index / 3].lanes[index % 3];
    }

private:
    Matrix3x3_SIMD(const __m128 row0, const __m128 row1, const __m128 row2)
    {
        m_rows[0].value = row0;
        m_rows[1].value = row1;
        m_rows[2].value = row2;
    }

public:
    /// @brief 他の行列を加算してこの行列を更新する。
    Matrix3x3_SIMD& operator+=(const Matrix3x3_SIMD& other)
    {
        for (std::size_t row = 0; row < 3; ++row)
        {
            m_rows[row].value = _mm_add_ps(m_rows[row].value, other.m_rows[row].value);
        }
        return *this;
    }

    /// @brief 他の行列を減算してこの行列を更新する。
    Matrix3x3_SIMD& operator-=(const Matrix3x3_SIMD& other)
    {
        for (std::size_t row = 0; row < 3; ++row)
        {
            m_rows[row].value = _mm_sub_ps(m_rows[row].value, other.m_rows[row].value);
        }
        return *this;
    }

    /// @brief 他の行列との積でこの行列を更新する。
    Matrix3x3_SIMD& operator*=(const Matrix3x3_SIMD& other)
    {
        Multiply(*this, other, *this);
        return *this;
    }

    /// @brief スカラー倍してこの行列を更新する。
    Matrix3x3_SIMD& operator*=(const float scalar)
    {
        for (std::size_t row = 0; row < 3; ++row)
        {
            m_rows[row].value = _mm_mul_ps(m_rows[row].value, _mm_set1_ps(scalar));
        }
        return *this;
    }

    /// @brief スカラーで除算してこの行列を更新する。
    Matrix3x3_SIMD& operator/=(const float scalar)
    {
        assert(scalar != 0.0f && "Division by zero in Matrix3x3_SIMD operator/=");
        if (scalar != 0.0f)
        {
            const __m128 divisor = _mm_set1_ps(scalar);
            for (std::size_t row = 0; row < 3; ++row)
            {
                m_rows[row].value = _mm_div_ps(m_rows[row].value, divisor);
            }
        }
        return *this;
    }

    /// @brief 単位行列を返す。
    static Matrix3x3_SIMD Identity()
    {
        return Matrix3x3_SIMD(
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 1.0f);
    }

    /// @brief 指定量だけ平行移動する行列を返す。
    static Matrix3x3_SIMD Translation(const float tx, const float ty)
    {
        return Matrix3x3_SIMD(
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            tx, ty, 1.0f);
    }

    /// @brief 各軸の拡大率を指定した行列を返す。
    static Matrix3x3_SIMD Scaling(const float sx, const float sy)
    {
        return Matrix3x3_SIMD(
            sx, 0.0f, 0.0f,
            0.0f, sy, 0.0f,
            0.0f, 0.0f, 1.0f);
    }

    /// @brief 全軸を同率で拡大する行列を返す。
    static Matrix3x3_SIMD ScalingUniform(const float scale)
    {
        return Scaling(scale, scale);
    }

    /// @brief 指定角度の回転行列を返す。
    static Matrix3x3_SIMD Rotation(const float angle)
    {
        const float cosine = Math::Cos(angle);
        const float sine = Math::Sin(angle);
        return Matrix3x3_SIMD(
            cosine, sine, 0.0f,
            -sine, cosine, 0.0f,
            0.0f, 0.0f, 1.0f);
    }

    /// @brief この行列に平行移動を適用する。
    Matrix3x3_SIMD& Translate(const float tx, const float ty)
    {
        const Matrix3x3_SIMD translation = Translation(tx, ty);
        Multiply(*this, translation, *this);
        return *this;
    }

    /// @brief この行列に拡大縮小を適用する。
    Matrix3x3_SIMD& Scale(const float sx, const float sy)
    {
        const Matrix3x3_SIMD scaling = Scaling(sx, sy);
        Multiply(*this, scaling, *this);
        return *this;
    }

    /// @brief この行列に等方拡大縮小を適用する。
    Matrix3x3_SIMD& ScaleUniform(const float scale)
    {
        return Scale(scale, scale);
    }

    /// @brief この行列に回転を適用する。
    Matrix3x3_SIMD& Rotate(const float angle)
    {
        const Matrix3x3_SIMD rotation = Rotation(angle);
        Multiply(*this, rotation, *this);
        return *this;
    }

    /// @brief 転置行列を出力先へ格納する。
    void Transpose(Matrix3x3_SIMD& result) const
    {
        __m128 row0 = m_rows[0].value;
        __m128 row1 = m_rows[1].value;
        __m128 row2 = m_rows[2].value;
        __m128 row3 = _mm_setzero_ps();
        _MM_TRANSPOSE4_PS(row0, row1, row2, row3);
        result.m_rows[0].value = row0;
        result.m_rows[1].value = row1;
        result.m_rows[2].value = row2;
    }

    /// @brief 2つの行列の和を出力先へ格納する。
    static void Add(const Matrix3x3_SIMD& a, const Matrix3x3_SIMD& b, Matrix3x3_SIMD& result)
    {
        for (std::size_t row = 0; row < 3; ++row)
        {
            result.m_rows[row].value = _mm_add_ps(a.m_rows[row].value, b.m_rows[row].value);
        }
    }

    /// @brief 2つの行列の差を出力先へ格納する。
    static void Subtract(const Matrix3x3_SIMD& a, const Matrix3x3_SIMD& b, Matrix3x3_SIMD& result)
    {
        for (std::size_t row = 0; row < 3; ++row)
        {
            result.m_rows[row].value = _mm_sub_ps(a.m_rows[row].value, b.m_rows[row].value);
        }
    }

    /// @brief 2つの行列を乗算して出力先へ格納する。
    static void Multiply(const Matrix3x3_SIMD& a, const Matrix3x3_SIMD& b, Matrix3x3_SIMD& result)
    {
        const __m128 lhs0 = a.m_rows[0].value;
        const __m128 lhs1 = a.m_rows[1].value;
        const __m128 lhs2 = a.m_rows[2].value;
        const __m128 x0 = _mm_shuffle_ps(lhs0, lhs0, _MM_SHUFFLE(0, 0, 0, 0));
        const __m128 y0 = _mm_shuffle_ps(lhs0, lhs0, _MM_SHUFFLE(1, 1, 1, 1));
        const __m128 z0 = _mm_shuffle_ps(lhs0, lhs0, _MM_SHUFFLE(2, 2, 2, 2));
        const __m128 x1 = _mm_shuffle_ps(lhs1, lhs1, _MM_SHUFFLE(0, 0, 0, 0));
        const __m128 y1 = _mm_shuffle_ps(lhs1, lhs1, _MM_SHUFFLE(1, 1, 1, 1));
        const __m128 z1 = _mm_shuffle_ps(lhs1, lhs1, _MM_SHUFFLE(2, 2, 2, 2));
        const __m128 x2 = _mm_shuffle_ps(lhs2, lhs2, _MM_SHUFFLE(0, 0, 0, 0));
        const __m128 y2 = _mm_shuffle_ps(lhs2, lhs2, _MM_SHUFFLE(1, 1, 1, 1));
        const __m128 z2 = _mm_shuffle_ps(lhs2, lhs2, _MM_SHUFFLE(2, 2, 2, 2));

        __m128 result0 = _mm_mul_ps(x0, b.m_rows[0].value);
        result0 = _mm_add_ps(result0, _mm_mul_ps(y0, b.m_rows[1].value));
        result0 = _mm_add_ps(result0, _mm_mul_ps(z0, b.m_rows[2].value));
        __m128 result1 = _mm_mul_ps(x1, b.m_rows[0].value);
        result1 = _mm_add_ps(result1, _mm_mul_ps(y1, b.m_rows[1].value));
        result1 = _mm_add_ps(result1, _mm_mul_ps(z1, b.m_rows[2].value));
        __m128 result2 = _mm_mul_ps(x2, b.m_rows[0].value);
        result2 = _mm_add_ps(result2, _mm_mul_ps(y2, b.m_rows[1].value));
        result2 = _mm_add_ps(result2, _mm_mul_ps(z2, b.m_rows[2].value));
        result.m_rows[0].value = result0;
        result.m_rows[1].value = result1;
        result.m_rows[2].value = result2;
    }

    /// @brief スカラー倍した結果を出力先へ格納する。
    static void Multiply(const Matrix3x3_SIMD& value, const float scalar, Matrix3x3_SIMD& result)
    {
        const __m128 factor = _mm_set1_ps(scalar);
        result.m_rows[0].value = _mm_mul_ps(value.m_rows[0].value, factor);
        result.m_rows[1].value = _mm_mul_ps(value.m_rows[1].value, factor);
        result.m_rows[2].value = _mm_mul_ps(value.m_rows[2].value, factor);
    }

    /// @brief スカラーで除算して出力先へ格納する。
    static void Divide(const Matrix3x3_SIMD& value, const float scalar, Matrix3x3_SIMD& result)
    {
        assert(scalar != 0.0f && "Division by zero in Matrix3x3_SIMD::Divide");
        if (scalar == 0.0f)
        {
            result = value;
            return;
        }
        const __m128 divisor = _mm_set1_ps(scalar);
        result.m_rows[0].value = _mm_div_ps(value.m_rows[0].value, divisor);
        result.m_rows[1].value = _mm_div_ps(value.m_rows[1].value, divisor);
        result.m_rows[2].value = _mm_div_ps(value.m_rows[2].value, divisor);
    }
};
