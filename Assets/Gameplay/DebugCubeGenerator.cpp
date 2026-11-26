HEADER_BEGIN
#include <cstddef>
#include <vector>

#include "Common/Include/EntityHandle.h"

namespace HestiaGame
{
    class DebugCubeGenerator
    {
    public:
        void OnStart(Hestia::EntityHandle self);
        void Update(Hestia::EntityHandle self);

    private:
        std::vector<Hestia::EntityHandle> m_pendingRotation;
        std::size_t m_rotationScriptCount = 0;
    };
}
HEADER_END

SOURCE_BEGIN
#include <algorithm>
#include <iterator>
#include <string>

#include "Engine/Include/Game/MeshRenderer.h"
#include "Game/Include/GameAssets.h"
#include "Game/Include/GameLogger.h"
#include "Game/Include/World.h"
#include "GeneratedScript/Scripts/Gameplay/RotateTransformScript.generated.h"

namespace HestiaGame
{
    void DebugCubeGenerator::OnStart(Hestia::EntityHandle)
    {
        // 全Cubeで同じMeshとMaterialを共有し、インスタンス描画にまとめる
        const auto mesh = Assets::Load<Hestia::Mesh>("Cube");
        const auto material = Assets::LoadMaterial("Materials/DebugCube.material");
        if (!mesh.IsValid() || !material.IsValid())
        {
            Logger::Error("DebugCubeGenerator: Cube mesh or DebugCubeMaterial is missing");
            return;
        }

        // 旧デバッグ描画と同じ格子、初期回転、配色を再現する
        constexpr int columns = 100;
        constexpr int rows = 25;
        constexpr int layers = 40;
        constexpr int cubeCount = columns * rows * layers;
        constexpr float spacing = 10.0f;
        constexpr float scale = 5.5f;
        constexpr int colorBlockSize = 5;
        const Vector4 colors[] = {
            { 1.0f, 0.25f, 0.30f, 1.0f }, { 0.25f, 0.85f, 0.40f, 1.0f },
            { 0.30f, 0.60f, 1.0f, 1.0f }, { 1.0f, 0.75f, 0.25f, 1.0f },
            { 0.75f, 0.40f, 1.0f, 1.0f }, { 0.20f, 0.85f, 0.85f, 1.0f },
        };
        const auto rotation = Quaternion::EulerToQuat({ 0.5f, 0.65f, 0.0f });
        m_pendingRotation.reserve(cubeCount);
        for (int index = 0; index < cubeCount; ++index)
        {
            const auto cube = World::CreateEntity();
            if (!cube.IsValid())
            {
                Logger::Error("DebugCubeGenerator: could not create Cube " + std::to_string(index));
                return;
            }

            // 格子の中心を原点に揃え、5セル単位でインスタンス色を切り替える
            const int column = index % columns;
            const int row = index / columns % rows;
            const int layer = index / (columns * rows);
            const Vector3 position{
                (column - (columns - 1) * 0.5f) * spacing,
                (row - (rows - 1) * 0.5f) * spacing,
                (layer - (layers - 1) * 0.5f) * spacing,
            };
            const auto& color = colors[(column / colorBlockSize + row / colorBlockSize + layer / colorBlockSize) % std::size(colors)];
            auto transform = World::GetProcess<Transform>(cube);
            auto renderer = World::GetProcess<MeshRenderer>(cube);
            if (!transform.SetLocalPosition(position) || !transform.SetLocalScale({ scale, scale, scale }) ||
                !transform.SetLocalRotation(rotation) || !World::AddProcess<MeshRenderer>(cube) ||
                !renderer.SetMesh(mesh) || !renderer.SetMaterial(0, material) || !renderer.SetInstanceColor(color))
            {
                World::DestroyEntity(cube);
                Logger::Error("DebugCubeGenerator: could not configure Cube " + std::to_string(index));
                return;
            }
            m_pendingRotation.push_back(cube);
        }

        Logger::Log("DebugCubeGenerator spawned " + std::to_string(cubeCount) + " cubes");
    }

    void DebugCubeGenerator::Update(Hestia::EntityHandle)
    {
        Logger::Log("DebugCubeGenerator: " + std::to_string(m_pendingRotation.size()) + " cubes pending rotation");

        if (m_pendingRotation.empty())
            return;

        // 保留中のScriptの重複探索を抑えるため、追加はフレームごとに上限を設ける
        constexpr std::size_t rotationBatchSize = 1024;
        const auto count = std::min(rotationBatchSize, m_pendingRotation.size());
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto cube = m_pendingRotation.back();
            if (World::AddScript<RotateTransformScript>(cube))
                ++m_rotationScriptCount;
            else
                Logger::Warning("DebugCubeGenerator: could not add rotation to Entity " + std::to_string(cube.m_index));
            m_pendingRotation.pop_back();
        }

        // 追加し終えたら、生成役が一時保持していたHandleを解放する
        if (m_pendingRotation.empty())
        {
            Logger::Log("DebugCubeGenerator queued " + std::to_string(m_rotationScriptCount) + " rotation scripts");
            m_pendingRotation = std::vector<Hestia::EntityHandle>{};
        }
    }
}
SOURCE_END
