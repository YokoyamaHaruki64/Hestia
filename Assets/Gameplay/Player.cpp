HEADER_BEGIN
/*=============================================================================

 File   : Player.cpp
 Desc   : 仮のプレイヤーを実装

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#include "Game/Include/World.h"
#include "Engine/Include/Game/Transform.h"

namespace HestiaGame
{
    class Player
    {
    public:
        /// @brief 自身と追従カメラの Transform を取得する
        void OnStart(Hestia::EntityHandle self);
        /// @brief 移動とカメラの旋回を更新する
        void Update(Hestia::EntityHandle);
        /// @brief 無効化時にカメラのカーソル固定を解除する
        void OnDisable(Hestia::EntityHandle);
        /// @brief 破棄時にカメラのカーソル固定を解除する
        void OnDestroy(Hestia::EntityHandle);

    private:
        void SetCameraCapture(bool captured);

        Transform m_transform;
        Transform m_camera;
        float m_cameraYaw = 0.0f;
        float m_cameraPitch = 0.0f;
        bool m_cameraCaptured = false;
    };
}
HEADER_END

SOURCE_BEGIN
#include <cmath>

#include "Game/Include/GameInput.h"
#include "Game/Include/GameTime.h"

namespace HestiaGame
{
    void Player::OnStart(Hestia::EntityHandle self)
    {
        // プレイヤーとシーンに配置済みのカメラを取得する
        m_transform = World::GetProcess<Transform>(self);
        const auto camera = World::FindFirstEntityByName("Camera");
        m_transform.SetLocalPosition({ 0.0f, 0.0f, 0.0f });

        if (!camera.IsValid())
            return;

        m_camera = World::GetProcess<Transform>(camera);
        if (m_camera.IsValid() && World::IsActive(self))
            SetCameraCapture(true);
    }

    void Player::Update(Hestia::EntityHandle)
    {
        // 斜めでも移動速度が一定になるように XZ 平面の入力を正規化する
        const float deltaTime = Time::DeltaTime();
        const Vector3 direction{
            static_cast<float>(Input::KeyHold(Key::D)) - static_cast<float>(Input::KeyHold(Key::A)),
            0.0f,
            static_cast<float>(Input::KeyHold(Key::W)) - static_cast<float>(Input::KeyHold(Key::S)),
        };

        constexpr float moveSpeed = 50.0f;
        if (direction.LengthSqr() > 0.0f)
            m_transform.SetLocalPosition(m_transform.GetLocalPosition() + direction.Normalized() * (moveSpeed * deltaTime));

        if (!m_camera.IsValid())
        {
            SetCameraCapture(false);
            return;
        }

        // 切り替えたフレームは、切り替え前に届いたマウス差分を使わない
        if (Input::KeyDown(Key::Escape))
            SetCameraCapture(!m_cameraCaptured);
        else if (m_cameraCaptured)
        {
            // Raw Input はフレーム内で積算済み。角度はカウントに比例させる
            constexpr float lookRadiansPerCount = 0.0025f;
            constexpr float minPitch = -15.0f * Math::DEG2RAD;
            constexpr float maxPitch = 80.0f * Math::DEG2RAD;
            const Vector2 mouseDelta = Input::DeltaPosition();
            m_cameraYaw = std::remainder(m_cameraYaw + mouseDelta.x * lookRadiansPerCount, Math::PI_2);
            m_cameraPitch = Math::Clamp(m_cameraPitch + mouseDelta.y * lookRadiansPerCount, minPitch, maxPitch);
        }

        // プレイヤーの上半身を中心に一定距離の位置を求め、そこから中心を向く
        constexpr float cameraDistance = 5.0f;
        constexpr float targetHeight = 1.5f;
        const Vector3 target = m_transform.GetWorldPosition() + Vector3(0.0f, targetHeight, 0.0f);
        const float horizontalDistance = cameraDistance * Math::Cos(m_cameraPitch);
        const Vector3 cameraOffset{
            -horizontalDistance * Math::Sin(m_cameraYaw),
            cameraDistance * Math::Sin(m_cameraPitch),
            -horizontalDistance * Math::Cos(m_cameraYaw),
        };
        m_camera.SetWorldPosition(target + cameraOffset);
        m_camera.SetWorldRotation(Quaternion::LookRotation(cameraOffset * -1.0f));
    }

    void Player::OnDisable(Hestia::EntityHandle)
    {
        SetCameraCapture(false);
    }

    void Player::OnDestroy(Hestia::EntityHandle)
    {
        SetCameraCapture(false);
    }

    void Player::SetCameraCapture(bool captured)
    {
        if (m_cameraCaptured == captured)
            return;

        m_cameraCaptured = captured;
        Input::SetCursorLockMode(captured ? CursorLockMode::Locked : CursorLockMode::None);
    }
}
SOURCE_END
