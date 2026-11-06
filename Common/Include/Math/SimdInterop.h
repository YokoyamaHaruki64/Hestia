#pragma once

// 通常型と SIMD 型の明示的な変換を集約し、行優先格納を維持する
#include <math.h>

#include "math/Matrix3x3.h"
#include "math/Matrix3x3_SIMD.h"
#include "math/Matrix4x4.h"
#include "math/Matrix4x4_SIMD.h"
#include "math/Quaternion.h"
#include "math/Quaternion_SIMD.h"
#include "math/Vector2_SIMD.h"
#include "math/Vector3_SIMD.h"
#include "math/Vector4_SIMD.h"

inline Matrix3x3_SIMD Matrix3x3_SIMD::Load(const Matrix3x3& source)
{
    return Matrix3x3_SIMD(
        source.m[0][0], source.m[0][1], source.m[0][2],
        source.m[1][0], source.m[1][1], source.m[1][2],
        source.m[2][0], source.m[2][1], source.m[2][2]);
}

inline Matrix3x3 Matrix3x3_SIMD::Store() const
{
    return Matrix3x3(
        m_rows[0].lanes[0], m_rows[0].lanes[1], m_rows[0].lanes[2],
        m_rows[1].lanes[0], m_rows[1].lanes[1], m_rows[1].lanes[2],
        m_rows[2].lanes[0], m_rows[2].lanes[1], m_rows[2].lanes[2]);
}

inline void Matrix3x3_SIMD::Store(Matrix3x3& destination) const
{
    destination.m[0][0] = m_rows[0].lanes[0];
    destination.m[0][1] = m_rows[0].lanes[1];
    destination.m[0][2] = m_rows[0].lanes[2];
    destination.m[1][0] = m_rows[1].lanes[0];
    destination.m[1][1] = m_rows[1].lanes[1];
    destination.m[1][2] = m_rows[1].lanes[2];
    destination.m[2][0] = m_rows[2].lanes[0];
    destination.m[2][1] = m_rows[2].lanes[1];
    destination.m[2][2] = m_rows[2].lanes[2];
}

inline Matrix4x4_SIMD Matrix4x4_SIMD::Load(const Matrix4x4& source)
{
    return Matrix4x4_SIMD(
        source.m[0][0], source.m[0][1], source.m[0][2], source.m[0][3],
        source.m[1][0], source.m[1][1], source.m[1][2], source.m[1][3],
        source.m[2][0], source.m[2][1], source.m[2][2], source.m[2][3],
        source.m[3][0], source.m[3][1], source.m[3][2], source.m[3][3]);
}

inline Matrix4x4 Matrix4x4_SIMD::Store() const
{
    return Matrix4x4(
        m_rows[0].lanes[0], m_rows[0].lanes[1], m_rows[0].lanes[2], m_rows[0].lanes[3],
        m_rows[1].lanes[0], m_rows[1].lanes[1], m_rows[1].lanes[2], m_rows[1].lanes[3],
        m_rows[2].lanes[0], m_rows[2].lanes[1], m_rows[2].lanes[2], m_rows[2].lanes[3],
        m_rows[3].lanes[0], m_rows[3].lanes[1], m_rows[3].lanes[2], m_rows[3].lanes[3]);
}

inline void Matrix4x4_SIMD::Store(Matrix4x4& destination) const
{
    _mm_storeu_ps(destination.m[0], m_rows[0].value);
    _mm_storeu_ps(destination.m[1], m_rows[1].value);
    _mm_storeu_ps(destination.m[2], m_rows[2].value);
    _mm_storeu_ps(destination.m[3], m_rows[3].value);
}

inline Quaternion_SIMD Quaternion_SIMD::Load(const Quaternion& source)
{
    return Quaternion_SIMD(source.x, source.y, source.z, source.w);
}

inline Quaternion Quaternion_SIMD::Store() const
{
    return Quaternion(m_storage.lanes[0], m_storage.lanes[1], m_storage.lanes[2], m_storage.lanes[3]);
}

inline void Quaternion_SIMD::Store(Quaternion& destination) const
{
    destination.x = m_storage.lanes[0];
    destination.y = m_storage.lanes[1];
    destination.z = m_storage.lanes[2];
    destination.w = m_storage.lanes[3];
}
