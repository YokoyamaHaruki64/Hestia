/*=============================================================================

 File   : GameRuntime.h
 Desc   : Game.dll 内部の Script 実行と Facade の寿命を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/13
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _GAME_RUNTIME_H_
#define _GAME_RUNTIME_H_

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ScriptStorage.h"

namespace HestiaGame::Detail
{
    /// @brief Engine から受け取る API と型ごとの ScriptStorage を管理する。
    class GameRuntime
    {
    public:
        /// @brief 型登録、初期配置の順に実行して Runtime を生成する。
        /// @note 初期化の関数は Game 内で呼ぶ。省略時は空の Runtime を生成する。
        static GameRuntime* Create(const Hestia::GameEngineAPI* api,
            bool (*registerScripts)() = nullptr, bool (*createInitialScripts)() = nullptr);
        void Finalize();
        void Run(ScriptPhase phase);
        void EndFrame();
        void OnEntityActiveChanged(Hestia::EntityHandle entity, bool active);

        const Hestia::GameEngineAPI& GetEngineAPI() const noexcept { return m_engineAPI; }

        /// @brief 初期化の型登録工程で具象 Script 型を登録する。
        /// @tparam T 登録する Script 型。
        /// @return 登録成功時は true。
        template<class T>
        bool RegisterScript()
        {
            if (!m_initializing || FindStorage<T>() != nullptr)
                return false;

            auto storage = std::make_unique<ScriptStorage<T>>(m_engineAPI);
            m_storages.reserve(m_storages.size() + 1);
            m_storageIndices.emplace(typeid(T), m_storages.size());
            m_storages.push_back(std::move(storage));
            return true;
        }

        template<class T, class... Args>
        ScriptRef<T> AddScript(Hestia::EntityHandle entity, Args&&... args)
        {
            if (!m_initialized)
                return {};

            auto* storage = FindStorage<T>();
            if (storage == nullptr || !m_engineAPI.m_entity->Exists(entity))
                return {};

            try
            {
                return storage->Add(entity, ++m_lastAttachment, std::forward<Args>(args)...);
            }
            catch (...)
            {
                Logger::Error("GameRuntime failed to construct a Script");
                return {};
            }
        }

        template<class T>
        ScriptRef<T> GetScript(Hestia::EntityHandle entity)
        {
            auto* storage = FindStorage<T>();
            return storage != nullptr ? storage->Get(entity) : ScriptRef<T>{};
        }

        template<class T>
        bool RemoveScript(Hestia::EntityHandle entity)
        {
            if (!m_initialized)
                return false;
            auto* storage = FindStorage<T>();
            if (storage == nullptr)
                return false;

            return storage->Remove(entity);
        }

        template<class T>
        bool SetScriptEnabled(Hestia::EntityHandle entity, bool enabled)
        {
            if (!m_initialized)
                return false;
            auto* storage = FindStorage<T>();
            if (storage == nullptr || !m_engineAPI.m_entity->Exists(entity))
                return false;

            return storage->SetEnabled(entity, enabled);
        }

        template<class T>
        std::vector<Hestia::EntityHandle> FindEntitiesWithScript() const
        {
            const auto* storage = FindStorage<T>();
            return storage != nullptr ? storage->FindEntities() : std::vector<Hestia::EntityHandle>{};
        }

    private:
        explicit GameRuntime(const Hestia::GameEngineAPI& api) : m_engineAPI(api) {}

        struct ActiveChange
        {
            Hestia::EntityHandle m_entity;
            bool m_active;
            std::size_t m_lastAttachment;
        };

        template<class T>
        ScriptStorage<T>* FindStorage() const noexcept
        {
            const auto it = m_storageIndices.find(typeid(T));
            return it == m_storageIndices.end() ? nullptr : static_cast<ScriptStorage<T>*>(m_storages[it->second].get());
        }


        const Hestia::GameEngineAPI& m_engineAPI;
        std::vector<std::unique_ptr<IScriptStorage>> m_storages;
        std::unordered_map<std::type_index, std::size_t> m_storageIndices;
        std::vector<ActiveChange> m_activeChanges;
        std::size_t m_lastAttachment = 0;
        bool m_initializing = false;
        bool m_initialized = false;
    };
}

#endif // _GAME_RUNTIME_H_
