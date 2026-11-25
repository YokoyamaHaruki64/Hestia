/*=============================================================================

 File   : ScriptStorage.h
 Desc   : 型ごとの Script 配列とコールバック実行を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/13
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _SCRIPT_STORAGE_H_
#define _SCRIPT_STORAGE_H_

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Engine/Include/API/GameEngineAPI.h"
#include "Common/Include/EntityHandle.h"
#include "GameLogger.h"
#include "ScriptRef.h"

namespace HestiaGame::Detail
{
    using AttachedScriptIndices =
        std::unordered_map<Hestia::EntityHandle, std::vector<std::size_t>, Hestia::EntityHandleHash>;

    enum class ScriptPhase
    {
        FixedUpdate,
        Update,
        LateUpdate
    };

    /// @brief STLはtemplateで渡せないため、型消去としてインターフェースを定義する
    /// @note GameRuntimeはIScriptStorageのポインタを保持し、ポリモーフィズムで型ごとのScriptStorageを操作する
    class IScriptStorage
    {
    public:
        virtual ~IScriptStorage() = default;
        /// @brief Scriptのコールバックを呼ぶ
        /// @param phase コールバックの種類
        /// @note 更新対象の Dense 配列を走査する
        virtual void Run(ScriptPhase phase) = 0;

        /// @brief Entity の Active 設定を受け取り、Script の有効状態の変化を通知する
        /// @param entity 対象
        /// @param active 状態
        /// @param lastAttachment Entityが最後にアタッチされたScriptのAttachment番号。これより大きいAttachment番号のScriptはまだアタッチされていないため、通知しない。
        virtual void NotifyActive(Hestia::EntityHandle entity, bool active, std::size_t lastAttachment) = 0;

        /// @brief Entityを削除する
        /// @param entity 対象
        /// @return 成功/失敗
        virtual bool Remove(Hestia::EntityHandle entity) = 0;

        /// @brief 全削除
        virtual void DestroyAll() = 0;

        /// @brief Script実体の生成/削除をバッチ処理
        /// @note 初期配置の完了時とフレーム末尾に呼ぶ
        virtual bool Flush(AttachedScriptIndices& attachments, std::size_t storageIndex) = 0;

        /// @brief 既定状態のScriptを生成して追加を予約する。
        virtual bool AddDefault(Hestia::EntityHandle entity, std::size_t attachment) = 0;
    };

    template<class T>
    struct ScriptState
    {
        ScriptState(Hestia::EntityHandle entity, T* script, std::size_t attachment)
            : m_entity(entity), m_reference(new ScriptReference<T>{ script }), m_attachment(attachment)
        {
        }

        // noexcept コピー
        ScriptState(ScriptState&& other) noexcept
            : m_entity(other.m_entity), m_reference(std::exchange(other.m_reference, nullptr)),
              m_attachment(other.m_attachment), m_enabled(other.m_enabled),
              m_entityActive(other.m_entityActive), m_removed(other.m_removed)
        {
        }

        // noexcept ムーブ
        ScriptState& operator=(ScriptState&& other) noexcept
        {
            std::swap(m_reference, other.m_reference);
            m_entity = other.m_entity;
            m_attachment = other.m_attachment;
            m_enabled = other.m_enabled;
            m_entityActive = other.m_entityActive;
            m_removed = other.m_removed;
            return *this;
        }

        ~ScriptState()
        {
            if (m_reference != nullptr)
                m_reference->Release();
        }

        Hestia::EntityHandle m_entity;
        ScriptReference<T>* m_reference;
        std::size_t m_attachment;
        bool m_enabled = true;
        bool m_entityActive = false;
        bool m_removed = false;
    };

    /// @brief ScriptStorageの具体型
    /// @tparam T Scriptの型
    /// @details 固定容量ページにScript本体とScriptStateの配列を保持する。
    /// EntityHandleのindexから位置を引くSparse配列と、追加・削除待ちの要求も保持する。
    template<class T>
    class ScriptStorage final : public IScriptStorage
    {
        static_assert(std::is_copy_constructible_v<T> || std::is_nothrow_move_constructible_v<T>,
            "Script must be copy constructible or nothrow move constructible to commit a pending addition");
        static_assert(std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>,
            "Script must be move or copy assignable for Swap Remove");

    public:
        explicit ScriptStorage(const Hestia::GameEngineAPI& engineAPI) : m_engineAPI(engineAPI) {}

        // @brief Scriptを追加する
        template<class... Args>
        ScriptRef<T> Add(Hestia::EntityHandle entity, std::size_t attachment, Args&&... args)
        {
            if (Find(entity) != nullptr)
                return {};

            // 追加通知の中で別の Script を追加しても、コールバック中の実体を移動しない
            auto& pending = m_pending.emplace_back(entity, attachment, std::forward<Args>(args)...);
            ScriptRef<T> reference(pending.m_state.m_reference);
            pending.m_state.m_entityActive = m_engineAPI.m_entity->IsActive(entity);
            if (pending.m_state.m_entityActive)
                NotifyEnabled(pending.m_state, true);

            // OnStart は追加時に一度だけ呼び、最初の更新まで待たない
            if constexpr (requires(T& script) { script.OnStart(entity); })
            {
                if (reference)
                    Invoke([&] { pending.m_script.OnStart(entity); });
            }

            return reference;
        }

        ScriptRef<T> Get(Hestia::EntityHandle entity) noexcept
        {
            auto* state = Find(entity);
            return state != nullptr ? ScriptRef<T>(state->m_reference) : ScriptRef<T>{};
        }

        bool SetEnabled(Hestia::EntityHandle entity, bool enabled)
        {
            auto* state = Find(entity);
            if (state == nullptr)
                return false;
            if (state->m_enabled == enabled)
                return true;

            state->m_enabled = enabled;
            if (state->m_entityActive)
                NotifyEnabled(*state, enabled);
            return true;
        }

        std::vector<Hestia::EntityHandle> FindEntities() const
        {
            std::vector<Hestia::EntityHandle> entities;
            for (const auto& page : m_pages)
            {
                for (const auto& state : page->m_states)
                {
                    if (!state.m_removed && m_engineAPI.m_entity->Exists(state.m_entity))
                        entities.push_back(state.m_entity);
                }
            }
            for (const auto& pending : m_pending)
            {
                const auto& state = pending.m_state;
                if (!state.m_removed && m_engineAPI.m_entity->Exists(state.m_entity))
                    entities.push_back(state.m_entity);
            }
            return entities;
        }

        void Run(ScriptPhase phase) override
        {
            /*
            * requires(T& script, Hestia::EntityHandle self){ script.FixedUpdate(self); } の返り値はbool
            * T型のscriptがselfを引数に取るメンバ関数を持つかどうかを判定することができる
            */


            switch (phase)
            {
            case ScriptPhase::FixedUpdate:
                if constexpr (requires(T & script, Hestia::EntityHandle self) { script.FixedUpdate(self); })
                    RunPhase([](T& script, Hestia::EntityHandle self) { script.FixedUpdate(self); });
                break;
            case ScriptPhase::Update:
                if constexpr (requires(T & script, Hestia::EntityHandle self) { script.Update(self); })
                    RunPhase([](T& script, Hestia::EntityHandle self) { script.Update(self); });
                break;
            case ScriptPhase::LateUpdate:
                if constexpr (requires(T & script, Hestia::EntityHandle self) { script.LateUpdate(self); })
                    RunPhase([](T& script, Hestia::EntityHandle self) { script.LateUpdate(self); });
                break;
            }
        }

        void NotifyActive(Hestia::EntityHandle entity, bool active, std::size_t lastAttachment) override
        {
            auto* state = Find(entity);
            if (state == nullptr || state->m_attachment > lastAttachment || state->m_entityActive == active)
                return;

            // コールバックから enabled を切り替える場合も、受け取った Active 設定を使う
            state->m_entityActive = active;
            if (state->m_enabled)
                NotifyEnabled(*state, active);
        }

        bool Remove(Hestia::EntityHandle entity) override
        {
            const auto index = FindDenseIndex(entity);
            if (index != INVALID_INDEX)
            {
                auto& state = GetPage(index).m_states[index % PAGE_CAPACITY];
                if (!state.m_removed)
                {
                    // Handle を削除待ち配列へ入れ、反映時点の DenseIndex を解決する
                    m_pendingRemovals.push_back(entity);
                    NotifyDestroy(state);
                    return true;
                }
            }

            for (auto& pending : m_pending)
            {
                if (pending.m_state.m_entity == entity && !pending.m_state.m_removed)
                {
                    NotifyDestroy(pending.m_state);
                    return true;
                }
            }
            return false;
        }

        void DestroyAll() override
        {
            for (const auto& page : m_pages)
                for (auto& state : page->m_states)
                    NotifyDestroy(state);
            for (auto& pending : m_pending)
                NotifyDestroy(pending.m_state);

            // 型のStorageを残し、旧Sceneの実体と位置情報だけを解放する
            m_pages.clear();
            m_denseCount = 0;
            m_sparse.clear();
            m_pending.clear();
            m_pendingRemovals.clear();
        }

        bool AddDefault(Hestia::EntityHandle entity, std::size_t attachment) override
        {
            if constexpr (std::is_default_constructible_v<T>)
                return static_cast<bool>(Add(entity, attachment));

            return false;
        }

        /// @brief Script実体の削除と追加を反映する
        bool Flush(AttachedScriptIndices& attachments, std::size_t storageIndex) override
        {
            // 再追加に備え、古いインスタンスを先に外す
            while (!m_pendingRemovals.empty())
            {
                const auto entity = m_pendingRemovals.back();
                SwapRemove(FindDenseIndex(entity));
                m_pendingRemovals.pop_back();

                auto entry = attachments.find(entity);
                std::erase(entry->second, storageIndex);
                if (entry->second.empty())
                    attachments.erase(entry);
            }

            while (!m_pending.empty())
            {
                auto& pending = m_pending.front();
                if (!pending.m_state.m_removed)
                {
                    const auto index = m_denseCount;
                    const auto entity = pending.m_state.m_entity;
                    std::vector<std::size_t>* attached = nullptr;
                    try
                    {
                        // 実体の反映後に逆引きの確保が失敗しないよう、先に空きを確保する
                        ReserveFor(entity);
                        attached = &attachments[entity];
                        attached->reserve(attached->size() + 1);
                        GetPage(index).m_scripts.emplace_back(
                            std::move_if_noexcept(pending.m_script));    // ムーブが例外を投げ得る場合はコピーを使う(move_if_noexceptの機能)
                    }
                    catch (...)
                    {
                        if (attached && attached->empty())
                            attachments.erase(entity);
                        Logger::Error("ScriptStorage failed to place a Script in dense storage");
                        return false;
                    }

                    // Stateの設定とSparseの更新はScriptの配置に成功してから行う
                    auto& page = GetPage(index);
                    auto& state = page.m_states.emplace_back(std::move(pending.m_state));
                    state.m_reference->m_script = &page.m_scripts.back();       // ScriptRefの参照先を更新
                    m_sparse[state.m_entity.m_index] = { state.m_entity.m_generation, index };
                    attached->push_back(storageIndex);
                    ++m_denseCount;
                }
                m_pending.pop_front();
            }
            return true;
        }

    private:
        void NotifyEnabled(ScriptState<T>& state, bool enabled)
        {
            T& script = *state.m_reference->m_script;
            const auto self = state.m_entity;
            if (enabled)
            {
                if constexpr (requires { script.OnEnable(self); })
                    Invoke([&] { script.OnEnable(self); });
            }
            else
            {
                if constexpr (requires { script.OnDisable(self); })
                    Invoke([&] { script.OnDisable(self); });
            }
        }

        // Script配列系統の無効index
        static constexpr std::size_t INVALID_INDEX = static_cast<std::size_t>(-1);
        static constexpr std::size_t PAGE_CAPACITY = 128;

        // 固定容量ページ
        struct Page
        {
            Page()
            {
                m_scripts.reserve(PAGE_CAPACITY);
                m_states.reserve(PAGE_CAPACITY);
            }

            std::vector<T> m_scripts;
            std::vector<ScriptState<T>> m_states;
        };

        Page& GetPage(std::size_t index) noexcept
        {
            return *m_pages[index / PAGE_CAPACITY];
        }

        // Sparse配列の要素
        struct SparseEntry
        {
            std::uint32_t m_generation = 0;
            std::size_t m_denseIndex = INVALID_INDEX;
        };

        // Scriptの追加待ちの要素
        struct PendingAdd
        {
            template<class... Args>
            PendingAdd(Hestia::EntityHandle entity, std::size_t attachment, Args&&... args)
                : m_script(std::forward<Args>(args)...),
                  m_state{ entity, &m_script, attachment }
            {
            }

            T m_script;
            ScriptState<T> m_state;
        };

        // 関数呼出の本体
        template<class Function>
        void Invoke(Function&& function)
        {
            try
            {
                function();
            }
            catch (...)
            {
                Logger::Error("Script callback threw an exception");
            }
        }

        // Scriptを走査し、条件を満たすもののコールバックを呼び出す
        // Tによって関数シグネチャが変わるため、関数をCallbackとしてTemplate化する
        // ここで想定するCallbackのシグネチャはvoid(T&, Hestia::EntityHandle)で、
        // Callback内の想定する実装はScriptの対応するメンバ関数を呼ぶことを想定している
        template<class Callback>
        void RunPhase(Callback callback)
        {
            const auto pageCount = (m_denseCount + PAGE_CAPACITY - 1) / PAGE_CAPACITY;
            for (std::size_t pageIndex = 0; pageIndex < pageCount; ++pageIndex)
            {
                auto& page = *m_pages[pageIndex];
                for (std::size_t index = 0; index < page.m_states.size(); ++index)
                {
                    auto& state = page.m_states[index];
                    const auto self = state.m_entity;

                    // ScriptStateの状態を確認し、コールバックを呼び出す
                    if (state.m_removed ||
                        !state.m_enabled ||
                        !m_engineAPI.m_entity->IsActive(self))  // API経由なのでアクセスコスト大。最適化の候補に
                        continue;

                    T& script = page.m_scripts[index];

                    Invoke([&] { callback(script, self); });
                }
            }
        }

        void NotifyDestroy(ScriptState<T>& state)
        {
            if (state.m_removed)
                return;

            // 削除フラグを立て、OnDestroyを呼ぶ
            T& script = *state.m_reference->m_script;
            state.m_removed = true;
            state.m_reference->m_script = nullptr;
            if constexpr (requires { script.OnDestroy(state.m_entity); })
                Invoke([&] { script.OnDestroy(state.m_entity); });
        }

        ScriptState<T>* Find(Hestia::EntityHandle entity) noexcept
        {
            const auto index = FindDenseIndex(entity);

            if (index != INVALID_INDEX)
            {
                auto& state = GetPage(index).m_states[index % PAGE_CAPACITY];
                if (!state.m_removed)
                    return &state;
            }

            for (auto& pending : m_pending)
            {
                if (pending.m_state.m_entity == entity && !pending.m_state.m_removed)
                    return &pending.m_state;
            }
            return nullptr;
        }

        std::size_t FindDenseIndex(Hestia::EntityHandle entity) const noexcept
        {
            if (entity.m_index >= m_sparse.size())
                return INVALID_INDEX;

            const auto& entry = m_sparse[entity.m_index];
            return entry.m_generation == entity.m_generation ? entry.m_denseIndex : INVALID_INDEX;
        }

        /// @brief 指定されたEntityのScriptを追加するためのリザーブを行う
        void ReserveFor(Hestia::EntityHandle entity)
        {
            if (entity.m_index >= m_sparse.size())
                m_sparse.resize(static_cast<std::size_t>(entity.m_index) + 1);

            // 空きのあるページを使い、全ページが満杯のときだけ増設する
            if (m_denseCount / PAGE_CAPACITY < m_pages.size())
                return;

            // ScriptStateとScriptの配列は、新しいページを作るときだけ確保する
            // 各 Dense 要素を一度ずつ予約できる容量を確保し、削除要求時の確保を避ける
            m_pendingRemovals.reserve(m_denseCount + PAGE_CAPACITY);
            m_pages.push_back(std::make_unique<Page>());

            // 既存ページは動かないため、ScriptRefの参照先は新規要素分だけ更新する
        }

        /// @brief 指定されたindexのScriptを削除する
        /// @details Swap&Popによる高速削除
        void SwapRemove(std::size_t index)
        {
            const auto last = m_denseCount - 1;
            auto& destination = GetPage(index);
            auto& source = GetPage(last);
            const auto slot = index % PAGE_CAPACITY;

            // 削除対象の位置に最後尾のScriptを移動する
            if (index != last)
            {
                if constexpr (std::is_move_assignable_v<T>)
                    destination.m_scripts[slot] = std::move(source.m_scripts.back());
                else
                    destination.m_scripts[slot] = std::as_const(source.m_scripts.back());
            }

            // 削除対象のStateが残っている間にSparseを解除する
            auto& state = destination.m_states[slot];
            m_sparse[state.m_entity.m_index] = {};

            if (index != last)
            {
                state = std::move(source.m_states.back());
                if (!state.m_removed)
                    state.m_reference->m_script = &destination.m_scripts[slot];
                m_sparse[state.m_entity.m_index].m_denseIndex = index;
            }

            source.m_scripts.pop_back();
            source.m_states.pop_back();
            --m_denseCount;
        }
        const Hestia::GameEngineAPI& m_engineAPI;
        std::vector<std::unique_ptr<Page>> m_pages;
        std::size_t m_denseCount = 0;
        std::vector<SparseEntry> m_sparse;
        std::list<PendingAdd> m_pending;
        std::vector<Hestia::EntityHandle> m_pendingRemovals;
    };
}

#endif // _SCRIPT_STORAGE_H_
