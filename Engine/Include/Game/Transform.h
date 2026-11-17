/*=============================================================================

 File   : Transform.h
 Desc   : EntityHandleを通してEngine.dll内のTransformを操作するGame向けFacade

------------------------------------------------------------------------------

 Date   : 2026/11/05
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _GAME_TRANSFORM_H_
#define _GAME_TRANSFORM_H_

#include <cassert>

#include "Engine/Include/API/ECSProcess/TransformAPI.h"

namespace HestiaGame
{
    class World;

    /// @brief Engine.dllの必須Transformへの非所有Facade
    /// @note 生成元ECSの寿命内で使用。Component移動をまたいで保持できる
    /// Getは有効な対象を前提とする。無効時はDebug assert、assert無効時はIdentity相当
    /// Setは失敗時にfalseを返し、正本と親子関係を変更しない。接続済みFacadeではEngineが失敗をログに残す
    class Transform
    {
    public:
        using API = Hestia::TransformAPI;

        /// @brief 親なしを表す空のFacadeを作る
        Transform() = default;

        /// @brief Engine側の操作テーブルを使ってHandleへの非所有参照を作る
        /// @param entity 生成元ECSのEntityHandle
        /// @note Game.dllではWorld::GetProcessを使う
        explicit Transform(Hestia::EntityHandle entity) noexcept;

        /// @brief 生成済みかつ破棄未予約のEntityか確認する。ログやassertは行わない
        /// @return 有効: true / 不在・生成待ち・破棄予約済み・ECS未接続: false
        bool IsValid() const noexcept { return m_api != nullptr && m_api->IsValid(m_entity); }

        /// @brief Local位置を値で取得する
        Vector3 GetLocalPosition() const
        {
            return HasAPIForGet() ? m_api->GetLocalPosition(m_entity) : Vector3::Zero();
        }
        /// @brief Local Quaternionを値で取得する
        Quaternion GetLocalRotation() const
        {
            return HasAPIForGet() ? m_api->GetLocalRotation(m_entity) : Quaternion::Identity();
        }
        /// @brief Local拡縮を値で取得する
        Vector3 GetLocalScale() const
        {
            return HasAPIForGet() ? m_api->GetLocalScale(m_entity) : Vector3::One();
        }

        /// @brief Local位置を変更し、子孫のWorldをdirtyにする
        /// @param position 有限な位置
        /// @return 成功: true / 対象・入力が無効: false
        bool SetLocalPosition(const Vector3& position)
        {
            return m_api != nullptr && m_api->SetLocalPosition(m_entity, position);
        }
        /// @brief Local回転を正規化して変更する。ゼロQuaternionはIdentity
        /// @param rotation 有限なQuaternion
        /// @return 成功: true / 対象・入力が無効: false
        bool SetLocalRotation(const Quaternion& rotation)
        {
            return m_api != nullptr && m_api->SetLocalRotation(m_entity, rotation);
        }
        /// @brief Local拡縮を変更する。負値と0も受け付ける
        /// @param scale 有限な拡縮
        /// @return 成功: true / 対象・入力が無効: false
        bool SetLocalScale(const Vector3& scale)
        {
            return m_api != nullptr && m_api->SetLocalScale(m_entity, scale);
        }

        /// @brief 必要な行列を更新してWorld位置を値で取得する
        Vector3 GetWorldPosition() const
        {
            return HasAPIForGet() ? m_api->GetWorldPosition(m_entity) : Vector3::Zero();
        }
        /// @brief 親子Quaternionを合成したWorld回転を取得する
        Quaternion GetWorldRotation() const
        {
            return HasAPIForGet() ? m_api->GetWorldRotation(m_entity) : Quaternion::Identity();
        }
        /// @brief 変換後の基底長を使う符号付き近似World拡縮を取得する
        Vector3 GetWorldScale() const
        {
            return HasAPIForGet() ? m_api->GetWorldScale(m_entity) : Vector3::One();
        }
        /// @brief dirtyなら親を先に更新してWorld行列を値で取得する
        Matrix4x4_SIMD GetWorldMatrix() const
        {
            return HasAPIForGet() ? m_api->GetWorldMatrix(m_entity) : Matrix4x4_SIMD::Identity();
        }

        /// @brief World位置からLocal位置を求める。特異な親では解ける成分だけ補正する
        /// @param position 有限なWorld位置
        /// @return 成功: true / 対象・入力が無効: false
        bool SetWorldPosition(const Vector3& position)
        {
            return m_api != nullptr && m_api->SetWorldPosition(m_entity, position);
        }
        /// @brief 親のQuaternionを除いてLocal回転を設定する
        /// @param rotation 有限なWorld回転
        /// @return 成功: true / 対象・入力が無効: false
        bool SetWorldRotation(const Quaternion& rotation)
        {
            return m_api != nullptr && m_api->SetWorldRotation(m_entity, rotation);
        }
        /// @brief World拡縮を逆算する。逆算不能なLocal成分は従来値を保持する
        /// @param scale 有限な符号付きWorld拡縮
        /// @return 成功: true / 対象・入力が無効: false
        bool SetWorldScale(const Vector3& scale)
        {
            return m_api != nullptr && m_api->SetWorldScale(m_entity, scale);
        }
        /// @brief World行列をTRSへ近似し、Local TRSへ変換して設定する
        /// @param matrix 有限な行ベクトル用アフィン行列。せん断の完全維持は保証しない
        /// @return 成功: true / 対象・入力が無効: false。ゼロスケールは失敗理由にしない
        bool SetWorldMatrix(const Matrix4x4_SIMD& matrix)
        {
            return m_api != nullptr && m_api->SetWorldMatrix(m_entity, matrix);
        }

        /// @brief 親のFacadeを値で取得する。rootなら空のFacade
        Transform GetParent() const
        {
            return HasAPIForGet() ? Transform(m_api->GetParent(m_entity), m_api) : Transform{};
        }
        /// @brief 親を変更する。KeepWorldはTRSで維持可能な範囲で維持する
        /// @param parent 新しい親。空のFacadeなら親解除
        /// @param mode KeepWorldまたはKeepLocal
        /// @return 成功: true / 無効な対象・親・mode、自身または子孫への指定: false
        bool SetParent(const Transform& parent, Hestia::ParentMode mode = Hestia::ParentMode::KeepWorld)
        {
            return m_api != nullptr && m_api->SetParent(m_entity, parent.m_entity, mode);
        }

    private:
        friend class World;

        Transform(Hestia::EntityHandle entity, const API* api) noexcept
            : m_entity(entity), m_api(api)
        {
        }

        bool HasAPIForGet() const noexcept
        {
            assert(m_api != nullptr && "Transform Get requires a live Entity");
            return m_api != nullptr;
        }

        Hestia::EntityHandle m_entity;
        const API* m_api = nullptr;
    };
}

#endif // _GAME_TRANSFORM_H_
