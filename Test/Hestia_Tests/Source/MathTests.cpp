/*=============================================================================

 File   : MathTests.cpp
 Desc   : 通常型と SIMD 型の行ベクトル・row-major 契約を検証する

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

===============================================================================*/

#include <cmath>
#include <limits>
#include <numbers>

#include "Common/Include/Math/Curve.h"

#include <gtest/gtest.h>

#include "Common/Include/Math/SimdInterop.h"

namespace
{
    constexpr float tolerance = 0.00001f;
    constexpr float halfPi = std::numbers::pi_v<float> * 0.5f;

    Vector3 TransformPoint(const Vector3& point, const Matrix4x4& matrix)
    {
        return Vector3(
            point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0],
            point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1],
            point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2]);
    }

    void ExpectVector(const Vector3& actual, const Vector3& expected)
    {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }

    void ExpectMatrix(const Matrix4x4& actual, const Matrix4x4& expected)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                SCOPED_TRACE(::testing::Message() << row << "," << column);
                EXPECT_NEAR(actual.m[row][column], expected.m[row][column], tolerance);
            }
        }
    }

    TEST(MathTests, SrtAppliesScaleThenRotationThenTranslation)
    {
        const Matrix4x4 local = Matrix4x4::Scaling(2.0f, 3.0f, 4.0f) *
            Matrix4x4::RotationZ(halfPi) * Matrix4x4::Translation(5.0f, 7.0f, 11.0f);
        ExpectVector(TransformPoint(Vector3(1.0f, 2.0f, 3.0f), local), Vector3(-1.0f, 9.0f, 23.0f));

        Matrix4x4_SIMD simdLocal = Matrix4x4_SIMD::Scaling(2.0f, 3.0f, 4.0f);
        simdLocal.RotateZ(halfPi);
        simdLocal.Translate(5.0f, 7.0f, 11.0f);
        ExpectMatrix(simdLocal.Store(), local);
    }

    TEST(MathTests, WorldAppliesLocalBeforeParent)
    {
        const Matrix4x4 local = Matrix4x4::Translation(1.0f, 0.0f, 0.0f);
        const Matrix4x4 parent = Matrix4x4::RotationZ(halfPi) * Matrix4x4::Translation(10.0f, 20.0f, 30.0f);
        ExpectVector(TransformPoint(Vector3::Zero(), local * parent), Vector3(10.0f, 21.0f, 30.0f));

        Matrix4x4_SIMD world;
        Matrix4x4_SIMD::Multiply(Matrix4x4_SIMD::Load(local), Matrix4x4_SIMD::Load(parent), world);
        ExpectMatrix(world.Store(), local * parent);
    }

    TEST(MathTests, SimdStorePreservesRowMajorOrderWithoutTranspose)
    {
        const Matrix4x4 source(
            1, 2, 3, 4,
            5, 6, 7, 8,
            9, 10, 11, 12,
            13, 14, 15, 16);
        Matrix4x4 destination;
        Matrix4x4_SIMD::Load(source).Store(destination);
        ExpectMatrix(destination, source);
        EXPECT_EQ(sizeof(Matrix4x4), 16 * sizeof(float));
    }

    TEST(MathTests, QuaternionMatchesLeftHandedAxisRotation)
    {
        const Quaternion rotation = Quaternion::FromAxisAngle(Vector3::Up(), halfPi);
        ExpectVector(rotation.RotateVector(Vector3::Forward()), Vector3::Right());
        ExpectMatrix(rotation.ToMatrix(), Matrix4x4::RotationY(halfPi));

        const Quaternion_SIMD simd = Quaternion_SIMD::FromAxisAngle(Vector3_SIMD(0, 1, 0), halfPi);
        Vector3_SIMD rotated;
        simd.RotateVector(Vector3_SIMD(0, 0, 1), rotated);
        ExpectVector(rotated.Store(), Vector3::Right());
        ExpectMatrix(simd.ToMatrix().Store(), rotation.ToMatrix());
    }

    TEST(MathTests, QuaternionEulerMatchesZxyMatrixComposition)
    {
        const Vector3 angles(0.3f, -0.7f, 1.1f);
        const Quaternion rotation = Quaternion::EulerToQuat(angles);
        ExpectMatrix(rotation.ToMatrix(), Matrix4x4::RotationRollPitchYaw(angles.x, angles.y, angles.z));
        ExpectVector(rotation.QuatToEuler(), angles);
        const Quaternion_SIMD simd = Quaternion_SIMD::EulerToQuat(Vector3_SIMD::Load(angles));
        ExpectMatrix(simd.ToMatrix().Store(), rotation.ToMatrix());
        ExpectVector(simd.QuatToEuler().Store(), angles);
    }

    TEST(MathTests, QuaternionCompositionMatchesRowVectorMatrixOrder)
    {
        const Quaternion first = Quaternion::FromAxisAngle(Vector3::Right(), 0.7f);
        const Quaternion second = Quaternion::FromAxisAngle(Vector3::Up(), -0.4f);
        const Matrix4x4 expected = first.ToMatrix() * second.ToMatrix();
        ExpectMatrix((first * second).ToMatrix(), expected);

        Quaternion_SIMD combined;
        Quaternion_SIMD::HamiltonProduct(Quaternion_SIMD::Load(first), Quaternion_SIMD::Load(second), combined);
        ExpectMatrix(combined.ToMatrix().Store(), expected);
    }

    TEST(MathTests, QuaternionLookRotationAlignsForward)
    {
        const Vector3 directions[] = {
            Vector3(1.0f, 2.0f, 3.0f).Normalized(), Vector3::Backward(), Vector3::Up(), Vector3::Down()
        };
        for (const Vector3& forward : directions)
        {
            const Quaternion rotation = Quaternion::LookRotation(forward);
            ExpectVector(rotation.RotateVector(Vector3::Forward()), forward);
            const Quaternion_SIMD simd = Quaternion_SIMD::LookRotation(Vector3_SIMD::Load(forward));
            ExpectMatrix(simd.ToMatrix().Store(), rotation.ToMatrix());
        }
    }

    TEST(MathTests, QuaternionHandlesZeroAndInverse)
    {
        ExpectMatrix(Quaternion(0, 0, 0, 0).ToMatrix(), Matrix4x4::Identity());
        ExpectMatrix(Quaternion::FromAxisAngle(Vector3::Zero(), 0.7f).ToMatrix(), Matrix4x4::Identity());
        const Quaternion rotation = Quaternion::EulerToQuat(Vector3(0.2f, -0.5f, 0.8f));
        ExpectMatrix((rotation * rotation.Inversed()).ToMatrix(), Matrix4x4::Identity());
        Quaternion_SIMD inverse;
        Quaternion_SIMD::Load(rotation).Inversed(inverse);
        ExpectMatrix(inverse.ToMatrix().Store(), rotation.Inversed().ToMatrix());
    }

    TEST(MathTests, SlerpKeepsRotationForOppositeQuaternionSigns)
    {
        const Quaternion rotation = Quaternion::FromAxisAngle(Vector3::Forward(), halfPi);
        const Quaternion opposite(-rotation.x, -rotation.y, -rotation.z, -rotation.w);
        ExpectVector(Quaternion::Slerp(rotation, opposite, 0.5f).RotateVector(Vector3::Right()), Vector3::Up());

        Quaternion_SIMD midpoint;
        Quaternion_SIMD::Slerp(Quaternion_SIMD::Load(rotation), Quaternion_SIMD::Load(opposite), 0.5f, midpoint);
        ExpectVector(TransformPoint(Vector3::Right(), midpoint.ToMatrix().Store()), Vector3::Up());
    }

    TEST(MathTests, SlerpInterpolatesAndClamps)
    {
        const Quaternion from = Quaternion::Identity();
        const Quaternion to = Quaternion::FromAxisAngle(Vector3::Up(), halfPi);
        const Quaternion midpoint = Quaternion::Slerp(from, to, 0.5f);
        ExpectMatrix(midpoint.ToMatrix(), Matrix4x4::RotationY(halfPi * 0.5f));
        ExpectMatrix(Quaternion::Slerp(from, to, -1.0f).ToMatrix(), from.ToMatrix());
        ExpectMatrix(Quaternion::Slerp(from, to, 2.0f).ToMatrix(), to.ToMatrix());
        Quaternion_SIMD simdMidpoint;
        Quaternion_SIMD::Slerp(Quaternion_SIMD::Load(from), Quaternion_SIMD::Load(to), 0.5f, simdMidpoint);
        ExpectMatrix(simdMidpoint.ToMatrix().Store(), midpoint.ToMatrix());
    }

    TEST(MathTests, PerspectiveMapsLeftHandedNearAndFarToZeroAndOne)
    {
        const Matrix4x4 projection = Matrix4x4::PerspectiveFovLH(1.0f, 1.5f, 0.1f, 100.0f);
        const auto depth = [&](float z)
        {
            return (z * projection.m[2][2] + projection.m[3][2]) /
                (z * projection.m[2][3] + projection.m[3][3]);
        };
        EXPECT_NEAR(depth(0.1f), 0.0f, tolerance);
        EXPECT_NEAR(depth(100.0f), 1.0f, tolerance);
        ExpectMatrix(Matrix4x4_SIMD::PerspectiveFovLH(1.0f, 1.5f, 0.1f, 100.0f).Store(), projection);
    }

    TEST(MathTests, OrthographicMapsBoundsAndDepthWithoutTranspose)
    {
        const Matrix4x4 projection = Matrix4x4::OrthographicLH(20.0f, 10.0f, 0.1f, 100.0f);
        ExpectVector(TransformPoint(Vector3(-10.0f, -5.0f, 0.1f), projection), Vector3(-1.0f, -1.0f, 0.0f));
        ExpectVector(TransformPoint(Vector3(10.0f, 5.0f, 100.0f), projection), Vector3(1.0f, 1.0f, 1.0f));
        EXPECT_FLOAT_EQ(projection.m[3][3], 1.0f);
    }

    TEST(MathTests, Matrix3x3AlsoAppliesRowVectorSrt)
    {
        const Matrix3x3 local = Matrix3x3::Scaling(2.0f, 3.0f) *
            Matrix3x3::Rotation(halfPi) * Matrix3x3::Translation(5.0f, 7.0f);
        const float x = local.m[0][0] + 2.0f * local.m[1][0] + local.m[2][0];
        const float y = local.m[0][1] + 2.0f * local.m[1][1] + local.m[2][1];
        EXPECT_NEAR(x, -1.0f, tolerance);
        EXPECT_NEAR(y, 9.0f, tolerance);

        Matrix3x3_SIMD simd = Matrix3x3_SIMD::Scaling(2.0f, 3.0f);
        simd.Rotate(halfPi);
        simd.Translate(5.0f, 7.0f);
        const Matrix3x3 stored = simd.Store();
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                EXPECT_NEAR(stored.m[row][column], local.m[row][column], tolerance);
    }

    TEST(MathTests, Vector2AngleHandlesNormalizationRounding)
    {
        // この入力は正規化後の内積が float の丸めで 1 を超える。
        const Vector2 value(0.1f, 0.6f);
        EXPECT_NEAR(Vector2::Angle(value, value), 0.0f, tolerance);
        EXPECT_NEAR(Vector2::Angle(value, Vector2(-0.1f, -0.6f)), Math::PI, tolerance);

        const Vector2_SIMD simdValue(0.1f, 0.6f);
        EXPECT_NEAR(Vector2_SIMD::Angle(simdValue, simdValue), 0.0f, tolerance);
        EXPECT_NEAR(Vector2_SIMD::Angle(simdValue, Vector2_SIMD(-0.1f, -0.6f)), Math::PI, tolerance);
    }

    TEST(MathTests, Vector3AngleHandlesNormalizationRounding)
    {
        const Vector3 value(1.0f, 1.0f, 4.0f);
        EXPECT_NEAR(Vector3::Angle(value, value), 0.0f, tolerance);
        EXPECT_NEAR(Vector3::Angle(value, Vector3(-1.0f, -1.0f, -4.0f)), Math::PI, tolerance);

        const Vector3_SIMD simdValue(1.0f, 1.0f, 4.0f);
        EXPECT_NEAR(Vector3_SIMD::Angle(simdValue, simdValue), 0.0f, tolerance);
        EXPECT_NEAR(Vector3_SIMD::Angle(simdValue, Vector3_SIMD(-1.0f, -1.0f, -4.0f)), Math::PI, tolerance);
    }

    TEST(MathTests, Vector4SimdAngleHandlesNormalizationRounding)
    {
        const Vector4_SIMD value(1.0f, 1.0f, 1.0f, 2.0f);
        EXPECT_NEAR(Vector4_SIMD::Angle(value, value), 0.0f, tolerance);
        EXPECT_NEAR(Vector4_SIMD::Angle(value, Vector4_SIMD(-1.0f, -1.0f, -1.0f, -2.0f)), Math::PI, tolerance);
    }

    TEST(MathTests, MathStandardWrappersReturnExpectedValues)
    {
        EXPECT_FLOAT_EQ(Math::Floor(-1.25f), -2.0f);
        EXPECT_FLOAT_EQ(Math::Ceil(-1.25f), -1.0f);
        EXPECT_FLOAT_EQ(Math::Round(1.5f), 2.0f);
        EXPECT_NEAR(Math::Sin(halfPi), 1.0f, tolerance);
        EXPECT_FLOAT_EQ(Math::Cos(0.0f), 1.0f);
        EXPECT_FLOAT_EQ(Math::Tan(0.0f), 0.0f);
        EXPECT_NEAR(Math::Asin(1.0f), halfPi, tolerance);
        EXPECT_FLOAT_EQ(Math::Acos(1.0f), 0.0f);
        EXPECT_NEAR(Math::Atan(1.0f), Math::PI_DIV4, tolerance);
        EXPECT_NEAR(Math::Atan2(1.0f, 0.0f), halfPi, tolerance);
        EXPECT_FLOAT_EQ(Math::Sqrt(4.0f), 2.0f);
    }

    TEST(MathTests, MathClampPreservesNanAndClampsFiniteValues)
    {
        EXPECT_FLOAT_EQ(Math::Clamp(-2.0f, -1.0f, 1.0f), -1.0f);
        EXPECT_FLOAT_EQ(Math::Clamp(0.25f, -1.0f, 1.0f), 0.25f);
        EXPECT_FLOAT_EQ(Math::Clamp(2.0f, -1.0f, 1.0f), 1.0f);

        const float nan = std::numeric_limits<float>::quiet_NaN();
        EXPECT_TRUE(std::isnan(Math::Clamp(nan, -1.0f, 1.0f)));
    }

    TEST(MathTests, CurveDefaultKeysEvaluateClampedLinearValues)
    {
        const Curve<float> curve;

        EXPECT_FLOAT_EQ(curve.Evaluate(-1.0f), 0.0f);
        EXPECT_FLOAT_EQ(curve.Evaluate(0.5f), 0.5f);
        EXPECT_FLOAT_EQ(curve.Evaluate(2.0f), 1.0f);
    }

    TEST(MathTests, MathInterpolationClampsAndWrapsAngles)
    {
        EXPECT_FLOAT_EQ(Math::Lerp(0.0f, 10.0f, -1.0f), 0.0f);
        EXPECT_FLOAT_EQ(Math::Lerp(0.0f, 10.0f, 2.0f), 10.0f);
        EXPECT_FLOAT_EQ(Math::LerpUnclamped(0.0f, 10.0f, 1.5f), 15.0f);
        EXPECT_NEAR(Math::LerpAngle(170.0f * Math::PI / 180.0f,
            -170.0f * Math::PI / 180.0f, 0.5f), Math::PI, tolerance);
    }

}
