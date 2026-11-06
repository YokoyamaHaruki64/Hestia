/*=============================================================================

 File   : EngineAPI.cpp
 Desc   : Engine API テーブルと Engine の生成・呼び出し境界を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/06
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Engine.h"

namespace Hestia
{
    namespace
    {
        EngineHandle CreateEngine(const ApplicationAPI* applicationAPI)
        {
            if (applicationAPI == nullptr)
                return nullptr;

            Engine* engine = new (std::nothrow) Engine();
            if (engine == nullptr || !engine->Initialize(*applicationAPI))
            {
                delete engine;
                return nullptr;
            }

            return engine;
        }

        void DestroyEngine(EngineHandle engineHandle)
        {
            if (engineHandle == nullptr)
                return;

            Engine* engine = static_cast<Engine*>(engineHandle);
            engine->Finalize();
            delete engine;
        }

        void ProcessEngineMessage(EngineHandle engineHandle, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            if (engineHandle != nullptr)
                static_cast<Engine*>(engineHandle)->ProcessMessage(hwnd, message, wParam, lParam);
        }

        void FixedUpdateEngine(EngineHandle engineHandle, float deltaTime)
        {
            if (engineHandle != nullptr)
                static_cast<Engine*>(engineHandle)->FixedUpdate(deltaTime);
        }

        void ExecuteEngineFrame(EngineHandle engineHandle, float deltaTime)
        {
            if (engineHandle != nullptr)
                static_cast<Engine*>(engineHandle)->FrameExecute(deltaTime);
        }

        constexpr EngineAPI ENGINE_API_TABLE
        {
            &CreateEngine,
            &DestroyEngine,
            &ProcessEngineMessage,
            &FixedUpdateEngine,
            &ExecuteEngineFrame
        };
    }

}

extern "C" HESTIA_ENGINE_API const Hestia::EngineAPI* GetEngineAPI()
{
    return &Hestia::ENGINE_API_TABLE;
}
