/*=============================================================================

 File   : GameSetup.cpp
 Desc   : Game で使う Script の型登録とデバッグシーンへの初期配置を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/19
 Author : Yokoyama Haruki

=============================================================================*/

#include "Game/Include/GameSetup.h"
#include "RotateTransformScript.h"

namespace HestiaGame::Setup
{
    bool RegisterScripts()
    {
        return World::RegisterScript<RotateTransformScript>();
    }

    bool SetupScene(const std::function<void()>& applyScripts)
    {
        // 一括追加では保留中の Script の重複探索が積み上がるため、小分けに反映する
        constexpr std::size_t batchSize = 256;
        std::size_t attachedCount = 0;
        for (const auto entity : World::FindEntitiesWithProcess<Transform>())
        {
            if (!World::AddScript<RotateTransformScript>(entity))
                return false;

            if (++attachedCount % batchSize == 0)
                applyScripts();
        }
        return true;
    }
}
