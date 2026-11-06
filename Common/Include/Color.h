/*=============================================================================

 File   : [Color.h]
 Desc   : 色を表す構造体の宣言

 ------------------------------------------------------------------------------

 Date   : 2026/04/19
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _COLOR_H_
#define _COLOR_H_
#include <cmath>
#include <algorithm>
#include <assert.h>
#include <math.h>


struct ColorHSV
{
    float h{ 0.0f }; // 色相 (0-360)
    float s{ 0.0f }; // 彩度 (0-1)
    float v{ 0.0f }; // 明度 (0-1)
    float a{ 1.0f }; // アルファ (0-1)
    constexpr ColorHSV() = default;
    constexpr ColorHSV(float h, float s, float v, float a = 1.0f) : h(h), s(s), v(v), a(a) {}
};

struct Color
{
    float r{ 1.0f };
    float g{ 1.0f };
    float b{ 1.0f };
    float a{ 1.0f }; // デフォルトは不透明な白
    constexpr Color() = default;
    constexpr Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    // ===============================
    // 演算子オーバーロード
    // ===============================

    inline constexpr Color operator+(const Color& other) const
    {
        return Color(r + other.r, g + other.g, b + other.b, a + other.a);
    }

    inline constexpr Color& operator+=(const Color& other)
    {
        r += other.r;
        g += other.g;
        b += other.b;
        a += other.a;
        return *this;
    }

    inline constexpr Color operator-(const Color& other) const
    {
        return Color(r - other.r, g - other.g, b - other.b, a - other.a);
    }

    inline constexpr Color& operator-=(const Color& other)
    {
        r -= other.r;
        g -= other.g;
        b -= other.b;
        a -= other.a;
        return *this;
    }

    inline constexpr Color operator*(const Color& other) const
    {
        return Color(r * other.r, g * other.g, b * other.b, a * other.a);
    }

    inline constexpr Color& operator*=(const Color& other)
    {
        r *= other.r;
        g *= other.g;
        b *= other.b;
        a *= other.a;
        return *this;
    }

    inline constexpr Color operator*(float scalar) const
    {
        return Color(r * scalar, g * scalar, b * scalar, a * scalar);
    }

    inline constexpr Color& operator*=(float scalar)
    {
        r *= scalar;
        g *= scalar;
        b *= scalar;
        a *= scalar;
        return *this;
    }

    inline constexpr Color operator/(const Color& other) const
    {
        // 0除算チェック
        assert(other.r != 0.0f && other.g != 0.0f && other.b != 0.0f && other.a != 0.0f && "Division by zero in Color operator/");
        if (other.r == 0.0f || other.g == 0.0f || other.b == 0.0f || other.a == 0.0f)
        {
            return *this; // 0除算の場合は元の色を返す
        }

        return Color(r / other.r, g / other.g, b / other.b, a / other.a);
    }

    inline constexpr Color& operator/=(const Color& other)
    {
        // 0除算チェック
        assert(other.r != 0.0f && other.g != 0.0f && other.b != 0.0f && other.a != 0.0f && "Division by zero in Color operator/=");
        if (other.r == 0.0f || other.g == 0.0f || other.b == 0.0f || other.a == 0.0f)
        {
            return *this; // 0除算の場合は元の色を返す
        }

        r /= other.r;
        g /= other.g;
        b /= other.b;
        a /= other.a;
        return *this;
    }

    inline constexpr Color operator/(float scalar) const
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Color operator/");
        if (scalar == 0.0f)
        {
            return *this; // 0除算の場合は元の色を返す
        }
        return Color(r / scalar, g / scalar, b / scalar, a / scalar);
    }

    inline constexpr Color& operator/=(float scalar)
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Color operator/=");
        if (scalar == 0.0f)
        {
            return *this; // 0除算の場合は元の色を返す
        }
        r /= scalar;
        g /= scalar;
        b /= scalar;
        a /= scalar;
        return *this;
    }

    // ===============================
    // 静的関数
    // ===============================

    inline static constexpr Color White()
    {
        return Color(1.0f, 1.0f, 1.0f, 1.0f);
    }

    inline static constexpr Color Black()
    {
        return Color(0.0f, 0.0f, 0.0f, 1.0f);
    }

    inline static constexpr Color Red()
    {
        return Color(1.0f, 0.0f, 0.0f, 1.0f);
    }

    inline static constexpr Color Green()
    {
        return Color(0.0f, 1.0f, 0.0f, 1.0f);
    }

    inline static constexpr Color Blue()
    {
        return Color(0.0f, 0.0f, 1.0f, 1.0f);
    }

    inline static constexpr Color Yellow()
    {
        return Color(1.0f, 1.0f, 0.0f, 1.0f);
    }

    inline static constexpr Color Cyan()
    {
        return Color(0.0f, 1.0f, 1.0f, 1.0f);
    }

    inline static constexpr Color Magenta()
    {
        return Color(1.0f, 0.0f, 1.0f, 1.0f);
    }

    inline static constexpr Color Gray()
    {
        return Color(0.5f, 0.5f, 0.5f, 1.0f);
    }

    inline static constexpr Color Transparent()
    {
        return Color(0.0f, 0.0f, 0.0f, 0.0f);
    }

    /// @brief 2つの色を線形補間する関数
    /// @param a 開始色
    /// @param b 終了色
    /// @param t 補間係数（0.0fから1.0fの範囲）
    /// @return 色aとbの線形補間結果
    inline static constexpr Color Lerp(const Color& a, const Color& b, float t)
    {
        return a + (b - a) * t;
    }

    /// @brief RGBカラーをHSVカラーに変換する関数
    /// @param c RGBカラー
    /// @return HSVカラー
    inline static ColorHSV RGBToHSV(const Color& c)
    {
        float r = c.r;
        float g = c.g;
        float b = c.b;

        float max = std::max({ r, g, b });
        float min = std::min({ r, g, b });
        float delta = max - min;

        ColorHSV hsv{};
        hsv.v = max;

        // Saturation
        if (max == 0.0f)
            hsv.s = 0.0f;
        else
            hsv.s = delta / max;

        // Hue
        if (delta == 0.0f)
        {
            hsv.h = 0.0f; // 無彩色
        }
        else
        {
            if (max == r)
                hsv.h = 60.0f * fmodf(((g - b) / delta), 6.0f);
            else if (max == g)
                hsv.h = 60.0f * (((b - r) / delta) + 2.0f);
            else
                hsv.h = 60.0f * (((r - g) / delta) + 4.0f);

            if (hsv.h < 0.0f)
                hsv.h += 360.0f;
        }

        hsv.a = c.a;
        return hsv;
    }

    /// @brief HSVカラーをRGBカラーに変換する関数
    /// @param hsv HSVカラー
    /// @return RGBカラー
    inline static Color HSVToRGB(const ColorHSV& hsv)
    {
        float h = hsv.h;
        float s = hsv.s;
        float v = hsv.v;

        // HSV to RGB変換のアルゴリズム
        float c = v * s;
        float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
        float m = v - c;

        float r = 0, g = 0, b = 0;

        if (h < 60)
        {
            r = c; 
            g = x; 
            b = 0;
        }
        else if (h < 120)
        {
            r = x; 
            g = c;
            b = 0;
        }
        else if (h < 180)
        {
            r = 0; 
            g = c;
            b = x;
        }
        else if (h < 240)
        {
            r = 0;
            g = x;
            b = c;
        }
        else if (h < 300)
        {
            r = x;
            g = 0;
            b = c;
        }
        else
        {
            r = c;
            g = 0;
            b = x;
        }

        return Color(
            r + m,
            g + m,
            b + m,
            hsv.a
        );
    }


    /// @brief 0-255の範囲のカラーを0-1の範囲のカラーに変換する関数
    /// @param c 0-255の範囲のカラー
    /// @return 0-1の範囲のカラー
    inline static constexpr Color Color255To01(const Color& c)
    {
        return Color(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
    }

    /// @brief 0-1の範囲のカラーを0-255の範囲のカラーに変換する関数
    /// @param c 0-1の範囲のカラー
    /// @return 0-255の範囲のカラー
    inline static constexpr Color Color01To255(const Color& c)
    {
        return Color(c.r * 255.0f, c.g * 255.0f, c.b * 255.0f, c.a * 255.0f);
    }

    // ===============================
    // メンバ関数
    // ===============================

    /// @brief RGB成分のみを別の色のRGB成分で加算する関数（このカラーは変更されない）
    /// @param other 加算する色
    /// @return 加算結果の新しい色
    inline Color AddedRGB(const Color& other) const
    {
        return Color(r + other.r, g + other.g, b + other.b, a); // アルファは加算しない
    }

    /// @brief RGB成分のみをスカラー値で加算する関数（このカラーは変更されない）
    /// @param scalar 加算するスカラー値
    /// @return 加算結果の新しい色
    inline Color AddedRGB(float scalar) const
    {
        return Color(r + scalar, g + scalar, b + scalar, a); // アルファは加算しない
    }

    /// @brief RGB成分のみを別の色のRGB成分で加算する関数（このカラー自体が変更される）
    /// @param other 加算する色
    /// @return この色への参照
    inline Color& AddRGB(const Color& other)
    {
        r += other.r;
        g += other.g;
        b += other.b;
        return *this; // アルファは加算しない
    }

    /// @brief RGB成分のみをスカラー値で加算する関数（このカラー自体が変更される）
    /// @param scalar 加算するスカラー値
    /// @return この色への参照
    inline Color& AddRGB(float scalar)
    {
        r += scalar;
        g += scalar;
        b += scalar;
        return *this; // アルファは加算しない
    }

    /// @brief RGB成分のみを別の色のRGB成分で減算する関数（このカラーは変更されない）
    /// @param other 減算する色
    /// @return 減算結果の新しい色
    inline Color SubtractedRGB(const Color& other) const
    {
        return Color(r - other.r, g - other.g, b - other.b, a); // アルファは減算しない
    }

    /// @brief RGB成分のみをスカラー値で減算する関数（このカラーは変更されない）
    /// @param scalar 減算するスカラー値
    /// @return 減算結果の新しい色
    inline Color SubtractedRGB(float scalar) const
    {
        return Color(r - scalar, g - scalar, b - scalar, a); // アルファは減算しない
    }

    /// @brief RGB成分のみを別の色のRGB成分で減算する関数（このカラー自体が変更される）
    /// @param other 減算する色
    /// @return この色への参照
    inline Color& SubtractRGB(const Color& other)
    {
        r -= other.r;
        g -= other.g;
        b -= other.b;
        return *this; // アルファは減算しない
    }

    /// @brief RGB成分のみをスカラー値で減算する関数（このカラー自体が変更される）
    /// @param scalar 減算するスカラー値
    /// @return この色への参照
    inline Color& SubtractRGB(float scalar)
    {
        r -= scalar;
        g -= scalar;
        b -= scalar;
        return *this; // アルファは減算しない
    }

    /// @brief RGB成分のみを別の色のRGB成分で乗算する関数（このカラーは変更されない）
    /// @param other 乗算する色
    /// @return 乗算結果の新しい色
    inline Color MultipliedRGB(const Color& other) const
    {
        return Color(r * other.r, g * other.g, b * other.b, a); // アルファは乗算しない
    }

    /// @brief RGB成分のみをスカラー値で乗算する関数（このカラーは変更されない）
    /// @param scalar 乗算するスカラー値
    /// @return 乗算結果の新しい色
    inline Color MultipliedRGB(float scalar) const
    {
        return Color(r * scalar, g * scalar, b * scalar, a); // アルファは乗算しない
    }

    /// @brief RGB成分のみを別の色のRGB成分で乗算する関数（このカラー自体が変更される）
    /// @param other 乗算する色
    /// @return この色への参照
    inline Color& MultiplyRGB(const Color& other)
    {
        r *= other.r;
        g *= other.g;
        b *= other.b;
        return *this; // アルファは乗算しない
    }

    /// @brief RGB成分のみをスカラー値で乗算する関数（このカラー自体が変更される）
    /// @param scalar 乗算するスカラー値
    /// @return この色への参照
    inline Color& MultiplyRGB(float scalar)
    {
        r *= scalar;
        g *= scalar;
        b *= scalar;
        return *this; // アルファは乗算しない
    }

    /// @brief RGB成分のみを別の色のRGB成分で除算する関数（このカラーは変更されない）
    /// @param other 除算する色
    /// @return 除算結果の新しい色
    inline Color DividedRGB(const Color& other) const
    {
        // 0除算チェック
        assert(other.r != 0.0f && other.g != 0.0f && other.b != 0.0f && "Division by zero in Color DividedRGB");
        if (other.r == 0.0f || other.g == 0.0f || other.b == 0.0f)
        {
            return *this; // 0除算の場合は元の色を返す
        }
        return Color(r / other.r, g / other.g, b / other.b, a); // アルファは除算しない
    }

    /// @brief RGB成分のみをスカラー値で除算する関数（このカラーは変更されない）
    /// @param scalar 除算するスカラー値
    /// @return 除算結果の新しい色
    inline Color DividedRGB(float scalar) const
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Color DividedRGB");
        if (scalar == 0.0f)
        {
            return *this; // 0除算の場合は元の色を返す
        }
        return Color(r / scalar, g / scalar, b / scalar, a); // アルファは除算しない
    }

    /// @brief RGB成分のみを別の色のRGB成分で除算する関数（このカラー自体が変更される）
    /// @param other 除算する色
    /// @return この色への参照
    inline Color& DivideRGB(const Color& other)
    {
        // 0除算チェック
        assert(other.r != 0.0f && other.g != 0.0f && other.b != 0.0f && "Division by zero in Color DivideRGB");
        if (other.r == 0.0f || other.g == 0.0f || other.b == 0.0f)
        {
            return *this; // 0除算の場合は元の色を返す
        }
        r /= other.r;
        g /= other.g;
        b /= other.b;
        return *this; // アルファは除算しない
    }

    /// @brief RGB成分のみをスカラー値で除算する関数（このカラー自体が変更される）
    /// @param scalar 除算するスカラー値
    /// @return この色への参照
    inline Color& DivideRGB(float scalar)
    {
        // 0除算チェック
        assert(scalar != 0.0f && "Division by zero in Color DivideRGB");
        if (scalar == 0.0f)
        {
            return *this; // 0除算の場合は元の色を返す
        }
        r /= scalar;
        g /= scalar;
        b /= scalar;
        return *this; // アルファは除算しない
    }
};




#endif // _COLOR_H_
