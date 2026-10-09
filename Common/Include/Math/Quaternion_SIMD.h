#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

#include <immintrin.h>

#include "math/Matrix4x4_SIMD.h"
#include "Math/MathCommon.h"

struct Quaternion;

struct Quaternion_SIMD
{
private:
    union alignas(16) Storage
    {
        __m128 value;
        float lanes[4];
    } m_storage;

public:

    /// @brief 単位クォータニオンで初期化する。
    Quaternion_SIMD()
    {
        m_storage.value = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);
    }
    /// @brief 4要素からSIMDクォータニオンを作る。
    Quaternion_SIMD(const float x, const float y, const float z, const float w)
    {
        m_storage.value = _mm_setr_ps(x, y, z, w);
    }

    /// @brief 既存型から値を読み込む。
    static Quaternion_SIMD Load(const Quaternion& source);
    /// @brief 既存型の値として返す。
    Quaternion Store() const;
    /// @brief 既存型の出力先へ値を保存する。
    void Store(Quaternion& destination) const;

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
    explicit Quaternion_SIMD(const __m128 value) { m_storage.value = value; }

public:
    /// @brief 単位クォータニオンを返す。
    static Quaternion_SIMD Identity()
    {
        return Quaternion_SIMD(0.0f, 0.0f, 0.0f, 1.0f);
    }

    /// @brief 長さの二乗を返す。
    float LengthSqr() const
    {
        return m_storage.lanes[0] * m_storage.lanes[0] +
            m_storage.lanes[1] * m_storage.lanes[1] +
            m_storage.lanes[2] * m_storage.lanes[2] +
            m_storage.lanes[3] * m_storage.lanes[3];
    }

    /// @brief 正規化した値を出力先へ格納する。
    void Normalized(Quaternion_SIMD& result) const
    {
        const float lengthSqr =
            m_storage.lanes[0] * m_storage.lanes[0] +
            m_storage.lanes[1] * m_storage.lanes[1] +
            m_storage.lanes[2] * m_storage.lanes[2] +
            m_storage.lanes[3] * m_storage.lanes[3];
        if (lengthSqr == 0.0f)
        {
            result.m_storage.value = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);
            return;
        }
        result.m_storage.value = _mm_div_ps(m_storage.value, _mm_set1_ps(Math::Sqrt(lengthSqr)));
    }

    /// @brief 逆クォータニオンを出力先へ格納する。
    void Inversed(Quaternion_SIMD& result) const
    {
        Quaternion_SIMD normalized;
        Normalized(normalized);
        result.m_storage.value = _mm_setr_ps(-normalized.x(), -normalized.y(), -normalized.z(), normalized.w());
    }

    /// @brief 回転を行列へ変換する。
    Matrix4x4_SIMD ToMatrix() const
    {
        Quaternion_SIMD q;
        Normalized(q);
        const float xx = q.x() * q.x();
        const float yy = q.y() * q.y();
        const float zz = q.z() * q.z();
        const float xy = q.x() * q.y();
        const float xz = q.x() * q.z();
        const float yz = q.y() * q.z();
        const float xw = q.x() * q.w();
        const float yw = q.y() * q.w();
        const float zw = q.z() * q.w();

        return Matrix4x4_SIMD(
            1.0f - 2.0f * (yy + zz), 2.0f * (xy + zw), 2.0f * (xz - yw), 0.0f,
            2.0f * (xy - zw), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + xw), 0.0f,
            2.0f * (xz + yw), 2.0f * (yz - xw), 1.0f - 2.0f * (xx + yy), 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    /// @brief 回転行列からクォータニオンを作る。
    static Quaternion_SIMD FromRotationMatrix(const Matrix4x4_SIMD& matrix)
    {
        const float trace = matrix[0][0] + matrix[1][1] + matrix[2][2];
        if (trace > 0.0f)
        {
            const float s = Math::Sqrt(trace + 1.0f) * 2.0f;
            return Quaternion_SIMD(
                (matrix[1][2] - matrix[2][1]) / s,
                (matrix[2][0] - matrix[0][2]) / s,
                (matrix[0][1] - matrix[1][0]) / s,
                0.25f * s);
        }
        if (matrix[0][0] > matrix[1][1] && matrix[0][0] > matrix[2][2])
        {
            const float s = Math::Sqrt(1.0f + matrix[0][0] - matrix[1][1] - matrix[2][2]) * 2.0f;
            return Quaternion_SIMD(
                0.25f * s,
                (matrix[0][1] + matrix[1][0]) / s,
                (matrix[0][2] + matrix[2][0]) / s,
                (matrix[1][2] - matrix[2][1]) / s);
        }
        if (matrix[1][1] > matrix[2][2])
        {
            const float s = Math::Sqrt(1.0f + matrix[1][1] - matrix[0][0] - matrix[2][2]) * 2.0f;
            return Quaternion_SIMD(
                (matrix[0][1] + matrix[1][0]) / s,
                0.25f * s,
                (matrix[1][2] + matrix[2][1]) / s,
                (matrix[2][0] - matrix[0][2]) / s);
        }
        const float s = Math::Sqrt(1.0f + matrix[2][2] - matrix[0][0] - matrix[1][1]) * 2.0f;
        return Quaternion_SIMD(
            (matrix[0][2] + matrix[2][0]) / s,
            (matrix[1][2] + matrix[2][1]) / s,
            0.25f * s,
            (matrix[0][1] - matrix[1][0]) / s);
    }

    /// @brief Euler角からクォータニオンを作る。
    static Quaternion_SIMD EulerToQuat(const Vector3_SIMD& eulerRadians)
    {
        const float halfPitch = eulerRadians.x() * 0.5f;
        const float halfYaw = eulerRadians.y() * 0.5f;
        const float halfRoll = eulerRadians.z() * 0.5f;
        const float cp = Math::Cos(halfPitch);
        const float sp = Math::Sin(halfPitch);
        const float cy = Math::Cos(halfYaw);
        const float sy = Math::Sin(halfYaw);
        const float cr = Math::Cos(halfRoll);
        const float sr = Math::Sin(halfRoll);
        return Quaternion_SIMD(
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cp * cy * sr - sp * sy * cr,
            cp * cy * cr + sp * sy * sr);
    }

    /// @brief クォータニオンをEuler角へ変換する。
    Vector3_SIMD QuatToEuler() const
    {
        Quaternion_SIMD rotation;
        Normalized(rotation);
        const float pitchSin = 2.0f * (rotation.w() * rotation.x() - rotation.y() * rotation.z());
        const float pitch = Math::Asin(Math::Clamp(pitchSin, -1.0f, 1.0f));
        const float yaw = Math::Atan2(
            2.0f * (rotation.w() * rotation.y() + rotation.z() * rotation.x()),
            1.0f - 2.0f * (rotation.x() * rotation.x() + rotation.y() * rotation.y()));
        const float roll = Math::Atan2(
            2.0f * (rotation.w() * rotation.z() + rotation.x() * rotation.y()),
            1.0f - 2.0f * (rotation.x() * rotation.x() + rotation.z() * rotation.z()));
        return Vector3_SIMD(pitch, yaw, roll);
    }

    /// @brief 軸と角度からクォータニオンを作る。
    static Quaternion_SIMD FromAxisAngle(const Vector3_SIMD& axis, const float radians)
    {
        const float length = axis.Length();
        if (length == 0.0f)
        {
            return Identity();
        }
        const Vector3_SIMD normalizedAxis(axis.x() / length, axis.y() / length, axis.z() / length);
        const float halfAngle = radians * 0.5f;
        const float sine = Math::Sin(halfAngle);
        return Quaternion_SIMD(
            normalizedAxis.x() * sine,
            normalizedAxis.y() * sine,
            normalizedAxis.z() * sine,
            Math::Cos(halfAngle));
    }

    /// @brief 前方と上方向からクォータニオンを作る。
    static Quaternion_SIMD LookRotation(const Vector3_SIMD& forward, const Vector3_SIMD& up = Vector3_SIMD(0.0f, 1.0f, 0.0f))
    {
        if (forward.LengthSqr() == 0.0f)
        {
            return Identity();
        }

        Vector3_SIMD normalizedForward;
        forward.Normalized(normalizedForward);
        Vector3_SIMD normalizedUp;
        if (up.LengthSqr() == 0.0f)
        {
            normalizedUp = Vector3_SIMD(0.0f, 1.0f, 0.0f);
        }
        else
        {
            up.Normalized(normalizedUp);
        }
        if (Math::Abs(Vector3_SIMD::Dot(normalizedForward, normalizedUp)) > 0.999f)
        {
            normalizedUp = Math::Abs(normalizedForward.y()) < 0.999f
                ? Vector3_SIMD(0.0f, 1.0f, 0.0f)
                : Vector3_SIMD(1.0f, 0.0f, 0.0f);
        }

        Vector3_SIMD right;
        Vector3_SIMD::Cross(normalizedUp, normalizedForward, right);
        right.Normalize();
        Vector3_SIMD correctedUp;
        Vector3_SIMD::Cross(normalizedForward, right, correctedUp);
        const Matrix4x4_SIMD rotationMatrix(
            right.x(), right.y(), right.z(), 0.0f,
            correctedUp.x(), correctedUp.y(), correctedUp.z(), 0.0f,
            normalizedForward.x(), normalizedForward.y(), normalizedForward.z(), 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
        Quaternion_SIMD result;
        FromRotationMatrix(rotationMatrix).Normalized(result);
        return result;
    }

    /// @brief 2つの単位回転を最短経路で球面線形補間し、出力先へ格納する
    /// @pre from と to は正規化済み
    static void Slerp(const Quaternion_SIMD& from, const Quaternion_SIMD& to, const float t, Quaternion_SIMD& result)
    {
        const float amount = Math::Saturate(t);
        const Quaternion_SIMD& start = from;
        Quaternion_SIMD end = to;
        float dot = start.x() * end.x() + start.y() * end.y() + start.z() * end.z() + start.w() * end.w();
        float sign = 1.0f;
        if (dot < 0.0f)
        {
            sign = -1.0f;
            dot = -dot;
        }
        dot = Math::Saturate(dot);

        float fromWeight = 1.0f - amount;
        float toWeight = amount;
        if (dot < 1.0f - 0.00001f)
        {
            const float sinOmega = Math::Sqrt(1.0f - dot * dot);
            const float omega = Math::Atan2(sinOmega, dot);
            const float inverseSinOmega = 1.0f / sinOmega;
            fromWeight = Math::Sin((1.0f - amount) * omega) * inverseSinOmega;
            toWeight = Math::Sin(amount * omega) * inverseSinOmega;
        }
        toWeight *= sign;

        result.m_storage.value = _mm_setr_ps(
            start.x() * fromWeight + end.x() * toWeight,
            start.y() * fromWeight + end.y() * toWeight,
            start.z() * fromWeight + end.z() * toWeight,
            start.w() * fromWeight + end.w() * toWeight);
    }

    /// @brief ベクトルをクォータニオンで回転する。
    void RotateVector(const Vector3_SIMD& vector, Vector3_SIMD& result) const
    {
        Quaternion_SIMD q;
        Normalized(q);
        const Vector3_SIMD axis(q.x(), q.y(), q.z());
        Vector3_SIMD twiceCross;
        Vector3_SIMD::Cross(axis, vector, twiceCross);
        twiceCross *= 2.0f;
        Vector3_SIMD scaledCross;
        Vector3_SIMD::Multiply(twiceCross, Vector3_SIMD(q.w(), q.w(), q.w()), scaledCross);
        Vector3_SIMD axisCross;
        Vector3_SIMD::Cross(axis, twiceCross, axisCross);
        Vector3_SIMD::Add(vector, scaledCross, result);
        Vector3_SIMD::Add(result, axisCross, result);
    }

    /// @brief a の後に b を適用する回転積を出力先へ格納する
    /// @note 行ベクトルの行列積 a.ToMatrix() * b.ToMatrix() と同じ適用順。Hamilton 積は b ⊗ a
    static void HamiltonProduct(const Quaternion_SIMD& a, const Quaternion_SIMD& b, Quaternion_SIMD& result)
    {
        result.m_storage.value = _mm_setr_ps(
            b.w() * a.x() + b.x() * a.w() + b.y() * a.z() - b.z() * a.y(),
            b.w() * a.y() - b.x() * a.z() + b.y() * a.w() + b.z() * a.x(),
            b.w() * a.z() + b.x() * a.y() - b.y() * a.x() + b.z() * a.w(),
            a.w() * b.w() - a.x() * b.x() - a.y() * b.y() - a.z() * b.z());
    }
};
