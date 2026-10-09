/*=============================================================================

 File   : Quaternion.h
 Desc   : 行ベクトル用の回転と補間を提供する Quaternion

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _QUATERNION_H_
#define _QUATERNION_H_

#include <algorithm>
#include <cmath>

#include "math/Matrix4x4.h"
#include "math/Vector3.h"

#include "Math/MathCommon.h"

struct Quaternion
{
    float x{ 0.0f };
    float y{ 0.0f };
    float z{ 0.0f };
    float w{ 1.0f };

    constexpr Quaternion() = default;
    constexpr Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    static constexpr Quaternion Identity()
    {
        return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    }

    /// @brief Z → X → Y の順で適用する Euler 角から回転を作る
    /// @param eulerRadians X、Y、Z 軸回転の角度（ラジアン）
    static Quaternion EulerToQuat(const Vector3& eulerRadians)
    {
        const float halfPitch = eulerRadians.x * 0.5f;
        const float halfYaw = eulerRadians.y * 0.5f;
        const float halfRoll = eulerRadians.z * 0.5f;
        const float cp = Math::Cos(halfPitch);
        const float sp = Math::Sin(halfPitch);
        const float cy = Math::Cos(halfYaw);
        const float sy = Math::Sin(halfYaw);
        const float cr = Math::Cos(halfRoll);
        const float sr = Math::Sin(halfRoll);

        return Quaternion(
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cp * cy * sr - sp * sy * cr,
            cp * cy * cr + sp * sy * sr);
    }

    Vector3 QuatToEuler() const
    {
        const Quaternion rotation = Normalized();
        const float pitchSin = 2.0f * (rotation.w * rotation.x - rotation.y * rotation.z);
        const float pitch = Math::Asin(Math::Clamp(pitchSin, -1.0f, 1.0f));
        const float yaw = Math::Atan2(
            2.0f * (rotation.w * rotation.y + rotation.z * rotation.x),
            1.0f - 2.0f * (rotation.x * rotation.x + rotation.y * rotation.y));
        const float roll = Math::Atan2(
            2.0f * (rotation.w * rotation.z + rotation.x * rotation.y),
            1.0f - 2.0f * (rotation.x * rotation.x + rotation.z * rotation.z));

        return Vector3(pitch, yaw, roll);
    }

    /// @brief 軸と角度から回転を作る。ゼロ軸の場合は単位回転
    static Quaternion FromAxisAngle(const Vector3& axis, float radians)
    {
        if (axis.LengthSqr() == 0.0f)
        {
            return Identity();
        }

        const Vector3 normalizedAxis = axis.Normalized();
        const float halfAngle = radians * 0.5f;
        const float sine = Math::Sin(halfAngle);
        return Quaternion(normalizedAxis.x * sine, normalizedAxis.y * sine, normalizedAxis.z * sine, Math::Cos(halfAngle));
    }

    /// @brief +Z を forward へ向ける回転を作る。ゼロ forward は単位回転
    static Quaternion LookRotation(const Vector3& forward, const Vector3& up = Vector3::Up())
    {
        if (forward.LengthSqr() == 0.0f)
        {
            return Identity();
        }

        const Vector3 normalizedForward = forward.Normalized();
        Vector3 normalizedUp = up.LengthSqr() == 0.0f ? Vector3::Up() : up.Normalized();

        if (Math::Abs(Vector3::Dot(normalizedForward, normalizedUp)) > 0.999f)
        {
            normalizedUp = Math::Abs(normalizedForward.y) < 0.999f ? Vector3::Up() : Vector3::Right();
        }

        const Vector3 right = Vector3::Cross(normalizedUp, normalizedForward).Normalized();
        const Vector3 correctedUp = Vector3::Cross(normalizedForward, right);
        const Matrix4x4 rotationMatrix(
            right.x, right.y, right.z, 0.0f,
            correctedUp.x, correctedUp.y, correctedUp.z, 0.0f,
            normalizedForward.x, normalizedForward.y, normalizedForward.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);

        return FromRotationMatrix(rotationMatrix).Normalized();
    }

    /// @brief 単位回転間を最短経路で球面線形補間する
    /// @param t 補間係数。[0, 1] に制限する
    /// @pre from と to は正規化済み
    static Quaternion Slerp(const Quaternion& from, const Quaternion& to, float t)
    {
        const float amount = Math::Saturate(t);
        float dot = from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w;
        const float sign = dot < 0.0f ? -1.0f : 1.0f;
        dot = Math::Saturate(Math::Abs(dot));

        float fromWeight = 1.0f - amount;
        float toWeight = amount;
        if (dot < 1.0f - 0.00001f)
        {
            const float sinOmega = Math::Sqrt(1.0f - dot * dot);
            const float omega = Math::Atan2(sinOmega, dot);
            fromWeight = Math::Sin((1.0f - amount) * omega) / sinOmega;
            toWeight = Math::Sin(amount * omega) / sinOmega;
        }
        toWeight *= sign;

        return Quaternion(
            from.x * fromWeight + to.x * toWeight,
            from.y * fromWeight + to.y * toWeight,
            from.z * fromWeight + to.z * toWeight,
            from.w * fromWeight + to.w * toWeight);
    }

    /// @brief 正規化した回転を返す。ゼロの場合は単位回転
    Quaternion Normalized() const
    {
        if (x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f)
        {
            return Identity();
        }

        const float length = Math::Sqrt(x * x + y * y + z * z + w * w);
        return Quaternion(x / length, y / length, z / length, w / length);
    }

    /// @brief 正規化した回転の逆回転を返す
    Quaternion Inversed() const
    {
        const Quaternion rotation = Normalized();
        return Quaternion(-rotation.x, -rotation.y, -rotation.z, rotation.w);
    }

    /// @brief 行ベクトル・row-major の回転行列を返す
    Matrix4x4 ToMatrix() const
    {
        const Quaternion q = Normalized();
        const float xx = q.x * q.x;
        const float yy = q.y * q.y;
        const float zz = q.z * q.z;
        const float xy = q.x * q.y;
        const float xz = q.x * q.z;
        const float yz = q.y * q.z;
        const float xw = q.x * q.w;
        const float yw = q.y * q.w;
        const float zw = q.z * q.w;

        return Matrix4x4(
            1.0f - 2.0f * (yy + zz), 2.0f * (xy + zw), 2.0f * (xz - yw), 0.0f,
            2.0f * (xy - zw), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + xw), 0.0f,
            2.0f * (xz + yw), 2.0f * (yz - xw), 1.0f - 2.0f * (xx + yy), 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    /// @brief この回転の後に rhs を適用する積を返す
    /// @note ToMatrix() * rhs.ToMatrix() と同じ適用順
    Quaternion operator*(const Quaternion& rhs) const
    {
        // 行ベクトルの行列合成順に合わせ、Hamilton 積は rhs ⊗ this
        return Quaternion(
            rhs.w * x + rhs.x * w + rhs.y * z - rhs.z * y,
            rhs.w * y - rhs.x * z + rhs.y * w + rhs.z * x,
            rhs.w * z + rhs.x * y - rhs.y * x + rhs.z * w,
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z);
    }

    /// @brief ベクトルを正規化した回転で回す
    Vector3 RotateVector(const Vector3& vector) const
    {
        const Quaternion rotation = Normalized();
        const Vector3 axis(rotation.x, rotation.y, rotation.z);
        const Vector3 twiceCross = Vector3::Cross(axis, vector) * 2.0f;
        return vector + twiceCross * rotation.w + Vector3::Cross(axis, twiceCross);
    }

private:
    static Quaternion FromRotationMatrix(const Matrix4x4& matrix)
    {
        const float trace = matrix.m[0][0] + matrix.m[1][1] + matrix.m[2][2];
        if (trace > 0.0f)
        {
            const float s = Math::Sqrt(trace + 1.0f) * 2.0f;
            return Quaternion(
                (matrix.m[1][2] - matrix.m[2][1]) / s,
                (matrix.m[2][0] - matrix.m[0][2]) / s,
                (matrix.m[0][1] - matrix.m[1][0]) / s,
                0.25f * s);
        }
        if (matrix.m[0][0] > matrix.m[1][1] && matrix.m[0][0] > matrix.m[2][2])
        {
            const float s = Math::Sqrt(1.0f + matrix.m[0][0] - matrix.m[1][1] - matrix.m[2][2]) * 2.0f;
            return Quaternion(
                0.25f * s,
                (matrix.m[0][1] + matrix.m[1][0]) / s,
                (matrix.m[0][2] + matrix.m[2][0]) / s,
                (matrix.m[1][2] - matrix.m[2][1]) / s);
        }
        if (matrix.m[1][1] > matrix.m[2][2])
        {
            const float s = Math::Sqrt(1.0f + matrix.m[1][1] - matrix.m[0][0] - matrix.m[2][2]) * 2.0f;
            return Quaternion(
                (matrix.m[0][1] + matrix.m[1][0]) / s,
                0.25f * s,
                (matrix.m[1][2] + matrix.m[2][1]) / s,
                (matrix.m[2][0] - matrix.m[0][2]) / s);
        }

        const float s = Math::Sqrt(1.0f + matrix.m[2][2] - matrix.m[0][0] - matrix.m[1][1]) * 2.0f;
        return Quaternion(
            (matrix.m[0][2] + matrix.m[2][0]) / s,
            (matrix.m[1][2] + matrix.m[2][1]) / s,
            0.25f * s,
            (matrix.m[0][1] - matrix.m[1][0]) / s);
    }
};

#endif // _QUATERNION_H_
