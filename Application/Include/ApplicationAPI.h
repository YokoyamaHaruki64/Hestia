/*=============================================================================

 File   : ApplicationAPI.h
 Desc   : Application が Engine 境界へ公開する共有 API 型を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/05
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _APPLICATION_API_H_
#define _APPLICATION_API_H_

class Application;

namespace Hestia
{
    /// @brief Engine から Application の限定された機能を呼び出す API。
    /// 
    /// 関数ポインタと Context は Application が構成する。Engine が値としてコピーしても、
    /// Context は非所有参照のため、呼び出し先の Application は Engine の破棄まで有効であること。
    struct ApplicationAPI
    {
        /// @brief 目標フレームレートを設定する。
        /// @param fps 目標フレームレート（frames per second）。
        void SetTargetFPS(double fps) const;

        /// @brief 目標フレームレートを取得する。
        /// @return 目標フレームレート（frames per second）。
        double GetTargetFPS() const;

        /// @brief フレームレート制限の有効状態を設定する。
        /// @param unlimited true の場合は上限を設けない。
        void SetUnlimitedFrameRate(bool unlimited) const;

        /// @brief フレームレート制限を行わない設定かを取得する。
        /// @return 上限を設けない場合は true。
        bool IsUnlimitedFrameRate() const;

        /// @brief 固定更新の時間間隔を秒単位で設定する。
        /// @param deltaTime 固定更新の時間間隔（秒）。
        void SetFixedDeltaTime(double deltaTime) const;

        /// @brief 固定更新の時間間隔を秒単位で取得する。
        /// @return 固定更新の時間間隔（秒）。
        double GetFixedDeltaTime() const;

        /// @brief Application に終了を要求する。
        void RequestQuit() const;

    private:
        void (*m_setTargetFPS)(void* context, double fps) = nullptr;
        double (*m_getTargetFPS)(void* context) = nullptr;

        void (*m_setUnlimitedFrameRate)(void* context, bool unlimited) = nullptr;
        bool (*m_isUnlimitedFrameRate)(void* context) = nullptr;

        void (*m_setFixedDeltaTime)(void* context, double deltaTime) = nullptr;
        double (*m_getFixedDeltaTime)(void* context) = nullptr;
        void (*m_requestQuit)(void* context) = nullptr;

        void* m_context = nullptr;

        friend class ::Application;
    };
}

#endif // _APPLICATION_API_H_
