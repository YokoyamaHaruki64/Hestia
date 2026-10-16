/*=============================================================================

 File   : GPUResourceStorage.h
 Desc   : AssetHandle に対応する Graphics 側 GPU Resource を保持する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _GPU_RESOURCE_STORAGE_H_
#define _GPU_RESOURCE_STORAGE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "Asset/AssetHandle.h"

namespace Hestia
{
    /// @brief AssetHandle の Index と Generation に対応する GPU Resource を保持
    /// @tparam TAsset Handle が対応する CPU Asset 型
    /// @tparam TGpuResource 保持する Graphics 側 Resource 型
    /// @note 無効 Handle、重複 Index、世代不一致は各操作で実行時に確認する
    template<class TAsset, class TGpuResource>
    class GPUResourceStorage
    {
    public:
        /// @brief AssetHandle に対応する GPU Resource を登録
        /// @param handle CPU Asset と共有する Handle
        /// @param resource 登録する GPU Resource
        /// @return 登録成功: true / 無効 Handle・重複 Handle: false
        bool Register(AssetHandle<TAsset> handle, TGpuResource resource)
        {
            const size_t index = static_cast<size_t>(handle.m_index);
            if (!CanRegister(handle))
                return false;
            if (m_slots.size() <= index)
                m_slots.resize(index + 1);

            Slot& slot = m_slots[index];
            slot.m_resource = std::make_unique<TGpuResource>(std::move(resource));
            slot.m_generation = handle.m_generation;
            return true;
        }

        /// @brief Index が未使用で GPU Resource を登録できるか確認
        /// @return Handle が有効で Index が空き: true / それ以外: false
        bool CanRegister(AssetHandle<TAsset> handle) const
        {
            if (!handle.IsValid())
                return false;

            const size_t index = static_cast<size_t>(handle.m_index);
            return index < m_slots.max_size() &&
                (index >= m_slots.size() || !m_slots[index].m_resource);
        }

        /// @brief Index と Generation が一致する GPU Resource を取得
        /// @return 登録済み Resource: 非 null / 無効または不一致: null
        TGpuResource* Get(AssetHandle<TAsset> handle)
        {
            if (!handle.IsValid())
                return nullptr;

            const size_t index = static_cast<size_t>(handle.m_index);
            if (index >= m_slots.size())
                return nullptr;

            Slot& slot = m_slots[index];
            if (slot.m_generation != handle.m_generation)
                return nullptr;

            return slot.m_resource.get();
        }

        /// @brief Index と Generation が一致する GPU Resource を取得
        /// @return 登録済み Resource: 非 null / 無効または不一致: null
        const TGpuResource* Get(AssetHandle<TAsset> handle) const
        {
            if (!handle.IsValid())
                return nullptr;

            const size_t index = static_cast<size_t>(handle.m_index);
            if (index >= m_slots.size())
                return nullptr;

            const Slot& slot = m_slots[index];
            if (slot.m_generation != handle.m_generation)
                return nullptr;

            return slot.m_resource.get();
        }

        /// @brief 保持する GPU Resource をすべて解放
        void Clear()
        {
            m_slots.clear();
        }

    private:
        struct Slot
        {
            uint32_t m_generation = 0;
            std::unique_ptr<TGpuResource> m_resource;
        };

        std::vector<Slot> m_slots;
    };
}

#endif // _GPU_RESOURCE_STORAGE_H_
