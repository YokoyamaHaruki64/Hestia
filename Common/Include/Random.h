#pragma once
#include <random>
#include <algorithm>
#include <cmath>

#include "math/MathCommon.h"
#include "math/Vector3.h"

class Random
{
private:
    inline static std::mt19937& Engine()
    {
        static std::random_device rd;
        static std::mt19937 engine(rd());
        return engine;
    }

public:
    inline static float Range(float min, float max)
    {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(Engine());
    }

    inline static int Range(int min, int max)
    {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(Engine());
    }

    inline static Vector3 InsideUnitSphere()
    {
        const int MAX_ATTEMPTS = 32; // 最大試行回数
        for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
        {
            Vector3 v(
                Range(-1.0f, 1.0f),
                Range(-1.0f, 1.0f),
                Range(-1.0f, 1.0f)
            );

            if (v.LengthSqr() <= 1.0f)
                return v;
        }
        return Vector3::Up(); // 最大試行回数を超えた場合は上方向の単位ベクトルを返す
    }

    inline static Vector3 OnUnitSphere()
    {
        Vector3 v = InsideUnitSphere();

        if (v.LengthSqr() <= 0.0001f) // 長さが0に近い場合は上方向の単位ベクトルを返す
            return Vector3::Up();

        return v.Normalized();
    }

    /// @brief 指定された方向と角度のコーン内のランダムな方向ベクトルを生成する関数
    /// @param direction コーンの中心方向を表すベクトル
    /// @param angleDeg コーンの角度（度単位）(半角度)
    /// @return コーン内のランダムな方向ベクトル
    inline static Vector3 RandomCone(const Vector3& direction, float angleDeg)
    {
        Vector3 forward = direction.Normalized();

        // forward と平行じゃない基準ベクトルを選ぶ
        Vector3 referenceUp = Math::Abs(Vector3::Dot(forward, Vector3::Up())) > 0.99f
            ? Vector3::Right()
            : Vector3::Up();

        // コーン軸に対する right / up を作る
        Vector3 right = Vector3::Cross(referenceUp, forward).Normalized();
        Vector3 up = Vector3::Cross(forward, right).Normalized();
        float angleRad = angleDeg * Math::DEG2RAD;

        // コーン内のランダムな方向を生成
        float minCos = Math::Cos(angleRad);
        float cosTheta = Random::Range(minCos, 1.0f);

        float sinTheta = Math::Sqrt(1.0f - cosTheta * cosTheta);

        float phi = Random::Range(0.0f, 2.0f * Math::PI);

        float x = sinTheta * Math::Cos(phi);
        float y = sinTheta * Math::Sin(phi);
        float z = cosTheta;

        // コーン軸に沿った方向ベクトルを計算
        Vector3 randomDirection = (right * x + up * y + forward * z).Normalized();

        return randomDirection;


    }

};
