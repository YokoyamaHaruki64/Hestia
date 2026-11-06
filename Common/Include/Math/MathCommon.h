#pragma once

#include <cmath>


namespace Math
{
    static constexpr float PI = 3.14159265358979323846f;
    static constexpr float PI_2 = 2.0f * PI;        // 度数法:360度
    static constexpr float PI_DIV2 = 0.5f * PI;     // 度数法:90度
    static constexpr float PI_DIV4 = 0.25f * PI;    // 度数法:45度
    static constexpr float DEG2RAD = PI / 180.0f;
    static constexpr float RAD2DEG = 180.0f / PI;


    inline static constexpr float Abs(const float value)
    {
        return (value < 0.0f) ? -value : value;
    }

    inline static float Floor(const float value)
    {
        return std::floor(value);
    }

    inline static float Ceil(const float value)
    {
        return std::ceil(value);
    }

    inline static float Round(const float value)
    {
        return std::round(value);
    }
    

    template <typename T>
    inline constexpr T Min(const T a, const T b)
    {
        return (a < b) ? a : b;
    }
    template <typename T>
    inline constexpr T Max(const T a, const T b)
    {
        return (a > b) ? a : b;
    }

    template <typename T>
    inline constexpr T Clamp(const T value, const T min, const T max)
    {
        return value < min ? min : (value > max ? max : value);
    }

    inline constexpr float Saturate(const float value)
    {
        return Clamp(value, 0.0f, 1.0f);
    }

    template <typename T>
    inline constexpr T Lerp(const T begin, const T end, const float t)
    {
        return begin + (end - begin) * Saturate(t);
    }

    template <typename T>
    inline constexpr T LerpUnclamped(const T begin, const T end, const float t)
    {
        return begin + (end - begin) * t;
    }

    template <typename T>
    inline constexpr T InverseLerp(const T begin, const T end, const float value)
    {
        return (value - begin) / (end - begin);
    }

    template <typename T>
    inline constexpr T Remap(
        const T fromBegin,
        const T fromEnd,
        const T toBegin,
        const T toEnd,
        const float value)
    {
        return Lerp(toBegin, toEnd, InverseLerp(fromBegin, fromEnd, value));
    }


    inline constexpr float LerpAngle(const float begin,const float end,const float t)
    {
        float delta = end - begin;

        while(delta > PI)
            delta -= PI_2;

        while (delta < -PI)
            delta += PI_2;

        return begin + delta * Saturate(t);

    }

    inline float Sin(const float angle)
    {
        return std::sin(angle);
    }

    inline float Cos(const float angle)
    {
        return std::cos(angle);
    }

    inline float Tan(const float angle)
    {
        return std::tan(angle);
    }

    inline float Asin(const float value)
    {
        return std::asin(value);
    }

    inline float Acos(const float value)
    {
        return std::acos(value);
    }

    inline float Atan(const float value)
    {
        return std::atan(value);
    }

    inline float Atan2(const float y, const float x)
    {
        return std::atan2(y, x);
    }

    inline float Sqrt(const float value)
    {
        return std::sqrt(value);
    }
}
