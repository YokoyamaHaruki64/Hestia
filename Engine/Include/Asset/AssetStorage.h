/*=============================================================================

 File   : AssetStorage.h
 Desc   : 型別 CPU Asset を所有し、名前と Handle で検索する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ASSET_STORAGE_H_
#define _ASSET_STORAGE_H_

#include <cstdint>
#include <memory>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Asset/AssetHandle.h"
#include "Common/Include/StringUtils.h"

namespace Hestia
{
    /// @brief Asset 型ごとの実体と名前索引を所有する。
    template<class T>
    class AssetStorage
    {
    public:
        /// @brief 名前から Handle を検索する。
        /// @param name ASCII 英字の大小を区別しない Asset 名。
        /// @return 登録済み Handle。未登録なら無効 Handle。
        AssetHandle<T> Find(std::string_view name) const
        {
            if (name.empty())
                return {};

            const auto found = m_nameToHandle.find(ToLowerAscii(name));
            if (found == m_nameToHandle.end())
                return {};

            return found->second;
        }

        /// @brief Asset の所有権を受け取り、Handle と名前を登録する。
        /// @param name ASCII 英字の大小を区別しない一意な Asset 名。
        /// @param asset 登録する Asset。呼び出し時点で所有権が移る。
        /// @return 登録成功時の Handle。失敗時は無効 Handle。
        AssetHandle<T> Register(std::string_view name, std::unique_ptr<T> asset)
        {
            if (name.empty())
                return {};

            return RegisterSlot(m_nameToHandle, ToLowerAscii(name), std::move(asset), {});
        }

        /// @brief 永続参照からロード済みHandleを検索する
        AssetHandle<T> Find(const AssetRef& ref) const
        {
            const auto found = m_refToHandle.find(ref);
            return found == m_refToHandle.end() ? AssetHandle<T>{} : found->second;
        }

        /// @brief Loaderが解決した永続参照でCPU Assetを登録する
        AssetHandle<T> Register(const AssetRef& ref, std::unique_ptr<T> asset)
        {
            return RegisterSlot(m_refToHandle, ref, std::move(asset), ref);
        }

        /// @brief 保存時に使う永続参照を取得する。名前登録のAssetは空
        AssetRef GetReference(AssetHandle<T> handle) const
        {
            return Get(handle) != nullptr ? m_slots[handle.m_index].m_ref : AssetRef{};
        }

        /// @brief 名前登録したAssetの索引名を保存時に取得する
        /// @param handle 名前を検索するHandle
        /// @return 登録名のView。ファイル由来または未登録のHandleは空
        std::string_view GetRegisteredName(AssetHandle<T> handle) const
        {
            for (const auto& [name, entry] : m_nameToHandle)
            {
                if (entry.m_index == handle.m_index && entry.m_generation == handle.m_generation)
                    return name;
            }
            return {};
        }

        /// @brief Index と Generation が一致する Asset を借用する。
        /// @param handle 照合する Handle。
        /// @return Asset への非所有ポインタ。無効または不一致なら nullptr。
        T* Get(AssetHandle<T> handle)
        {
            if (!handle.IsValid())
                return nullptr;

            const std::size_t index = static_cast<std::size_t>(handle.m_index);
            if (index >= m_slots.size())
                return nullptr;

            Slot& slot = m_slots[index];
            if (slot.m_generation != handle.m_generation)
                return nullptr;

            return slot.m_asset.get();
        }

        /// @brief Index と Generation が一致する Asset を読み取り専用で借用する。
        /// @param handle 照合する Handle。
        /// @return Asset への非所有ポインタ。無効または不一致なら nullptr。
        const T* Get(AssetHandle<T> handle) const
        {
            if (!handle.IsValid())
                return nullptr;

            const std::size_t index = static_cast<std::size_t>(handle.m_index);
            if (index >= m_slots.size())
                return nullptr;

            const Slot& slot = m_slots[index];
            if (slot.m_generation != handle.m_generation)
                return nullptr;

            return slot.m_asset.get();
        }

        /// @brief 正規化名が登録済みか確認する。
        /// @param name ASCII 英字の大小を区別しない Asset 名。
        /// @return 登録済みなら true。
        bool Contains(std::string_view name) const
        {
            return Find(name).IsValid();
        }

    private:
        struct Slot
        {
            std::unique_ptr<T> m_asset;
            uint32_t m_generation = 1;
            AssetRef m_ref;
        };

        template<class Map, class Key>
        AssetHandle<T> RegisterSlot(Map& index, Key key, std::unique_ptr<T> asset, const AssetRef& ref)
        {
            if (!asset || m_slots.size() >= static_cast<std::size_t>(UINT32_MAX))
                return {};

            const AssetHandle<T> handle{ static_cast<uint32_t>(m_slots.size()), 1 };
            if (!index.emplace(std::move(key), handle).second)
                return {};

            asset->m_handle = handle;
            m_slots.push_back({ std::move(asset), handle.m_generation, ref });
            return handle;
        }

        std::vector<Slot> m_slots;
        std::unordered_map<std::string, AssetHandle<T>> m_nameToHandle;
        std::unordered_map<AssetRef, AssetHandle<T>, AssetRef::Hash> m_refToHandle;
    };
}

#endif // _ASSET_STORAGE_H_
