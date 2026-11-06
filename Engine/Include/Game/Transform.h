/*=============================================================================

 File   : Transform.h
 Desc   : EntityHandleを通してEngine.dll内のTransformを操作するGame向けFacade

------------------------------------------------------------------------------

 Date   : 2026/11/05
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _GAME_TRANSFORM_H_
#define _GAME_TRANSFORM_H_

#include "Common/Include/DllAPI.h"
#include "Common/Include/HMath.h"
#include "ECS/Entity.h"

namespace Hestia
{
    class ECSWorld;

    /// @brief 親変更時に維持する座標系
    enum class ParentMode
    {
        KeepWorld,
        KeepLocal,
    };
}

namespace HestiaGame
{
    /// @brief Engine.dllの必須Transformへの非所有Facade
    /// @note 生成元ECSの寿命内で使用。Component移動をまたいで保持できる
    /// Getは有効な対象を前提とする。無効時はログ、Debug assert、assert無効時はIdentity相当
    /// Setは失敗時にログとfalseを返し、正本と親子関係を変更しない
    class Transform
    {
    public:
        /// @brief 親なしを表す空のFacadeを作る
        Transform() = default;

        /// @brief Handleへの非所有参照を作る。ここでは生存確認しない
        /// @param entity 生成元ECSのEntityHandle
        explicit Transform(Hestia::EntityHandle entity) noexcept : m_entity(entity) {}

        /// @brief 生成済みかつ破棄未予約のEntityか確認する。ログやassertは行わない
        /// @return 有効: true / 不在・生成待ち・破棄予約済み・ECS未接続: false
        HESTIA_ENGINE_API bool IsValid() const noexcept;

        /// @brief Local位置を値で取得する
        HESTIA_ENGINE_API Vector3 GetLocalPosition() const;
        /// @brief Local Quaternionを値で取得する
        HESTIA_ENGINE_API Quaternion GetLocalRotation() const;
        /// @brief Local拡縮を値で取得する
        HESTIA_ENGINE_API Vector3 GetLocalScale() const;

        /// @brief Local位置を変更し、子孫のWorldをdirtyにする
        /// @param position 有限な位置
        /// @return 成功: true / 対象・入力が無効: false
        HESTIA_ENGINE_API bool SetLocalPosition(const Vector3& position);
        /// @brief Local回転を正規化して変更する。ゼロQuaternionはIdentity
        /// @param rotation 有限なQuaternion
        /// @return 成功: true / 対象・入力が無効: false
        HESTIA_ENGINE_API bool SetLocalRotation(const Quaternion& rotation);
        /// @brief Local拡縮を変更する。負値と0も受け付ける
        /// @param scale 有限な拡縮
        /// @return 成功: true / 対象・入力が無効: false
        HESTIA_ENGINE_API bool SetLocalScale(const Vector3& scale);

        /// @brief 必要な行列を更新してWorld位置を値で取得する
        HESTIA_ENGINE_API Vector3 GetWorldPosition() const;
        /// @brief 親子Quaternionを合成したWorld回転を取得する
        HESTIA_ENGINE_API Quaternion GetWorldRotation() const;
        /// @brief 変換後の基底長を使う符号付き近似World拡縮を取得する
        HESTIA_ENGINE_API Vector3 GetWorldScale() const;
        /// @brief dirtyなら親を先に更新してWorld行列を値で取得する
        HESTIA_ENGINE_API Matrix4x4_SIMD GetWorldMatrix() const;

        /// @brief World位置からLocal位置を求める。特異な親では解ける成分だけ補正する
        /// @param position 有限なWorld位置
        /// @return 成功: true / 対象・入力が無効: false
        HESTIA_ENGINE_API bool SetWorldPosition(const Vector3& position);
        /// @brief 親のQuaternionを除いてLocal回転を設定する
        /// @param rotation 有限なWorld回転
        /// @return 成功: true / 対象・入力が無効: false
        HESTIA_ENGINE_API bool SetWorldRotation(const Quaternion& rotation);
        /// @brief World拡縮を逆算する。逆算不能なLocal成分は従来値を保持する
        /// @param scale 有限な符号付きWorld拡縮
        /// @return 成功: true / 対象・入力が無効: false
        HESTIA_ENGINE_API bool SetWorldScale(const Vector3& scale);
        /// @brief World行列をTRSへ近似し、Local TRSへ変換して設定する
        /// @param matrix 有限な行ベクトル用アフィン行列。せん断の完全維持は保証しない
        /// @return 成功: true / 対象・入力が無効: false。ゼロスケールは失敗理由にしない
        HESTIA_ENGINE_API bool SetWorldMatrix(const Matrix4x4_SIMD& matrix);

        /// @brief 親のFacadeを値で取得する。rootなら空のFacade
        HESTIA_ENGINE_API Transform GetParent() const;
        /// @brief 親を変更する。KeepWorldはTRSで維持可能な範囲で維持する
        /// @param parent 新しい親。空のFacadeなら親解除
        /// @param mode KeepWorldまたはKeepLocal
        /// @return 成功: true / 無効な対象・親・mode、自身または子孫への指定: false
        HESTIA_ENGINE_API bool SetParent(const Transform& parent,
            Hestia::ParentMode mode = Hestia::ParentMode::KeepWorld);

    private:
        Hestia::ECSWorld* ResolveForGet() const;
        Hestia::ECSWorld* ResolveForSet() const;

        Hestia::EntityHandle m_entity;
    };
}

#endif // _GAME_TRANSFORM_H_
