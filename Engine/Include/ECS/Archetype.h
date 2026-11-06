/*=============================================================================

 File   : Archetype.h
 Desc   : Entity構成からChunkのSoA配置を作り、Chunk群を所有する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ARCHETYPE_H_
#define _ARCHETYPE_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "ECS/ComponentRegistry.h"

namespace Hestia
{
    class Chunk;

    /// @brief ArchetypeのComponent型集合
    /// @note Component型IDはRegistryで決定的に生成されるため、Archetypeの署名として使用
    struct ArchetypeSignature
    {
        std::vector<ComponentTypeId> m_types;

        bool operator==(const ArchetypeSignature&) const noexcept = default;
    };

    /// @brief Archetype内の各Component列の配置情報
    struct ComponentLayout
    {
        ComponentTypeId m_type = 0;     // Component型ID
        size_t m_offset = 0;            // Chunk内の先頭からのbyte offset
        const ComponentInfo* m_info = nullptr;  // Component情報
    };

    struct ChunkLayout
    {
        size_t m_byteSize = 0;  // Chunk全体のbyte容量
        size_t m_capacity = 0;  // Chunk内の最大row数
        size_t m_alignment = 0; // Chunkの最大alignment
        std::vector<ComponentLayout> m_components;  // Archetype内の各Component列の配置情報
    };

    /// @brief 共通Layoutと、それを使用するChunk群を所有する。
    /// @note Layout内のComponentInfoはRegistryを参照するため、RegistryはArchetypeより長く存続する
    class Archetype
    {
    public:
        /// @brief Component構成からChunk配置を作る。
        /// @note 入力不正時は無効状態で構築し、IsValidで確認する
        Archetype(const ComponentRegistry& registry,
            ArchetypeSignature signature, size_t chunkByteSize);
        ~Archetype() = default;

        Archetype(const Archetype&) = delete;
        Archetype& operator=(const Archetype&) = delete;

        /// @brief Chunk配置の作成に成功したか返す。
        bool IsValid() const noexcept { return m_isValid; }

        /// @brief Component型の正規化済み集合を取得する。
        const ArchetypeSignature& GetSignature() const noexcept { return m_signature; }

        /// @brief Chunk共通の配置情報を取得する。
        const ChunkLayout& GetLayout() const noexcept { return m_layout; }

        /// @brief 所有するChunk数を取得する。
        size_t GetChunkCount() const noexcept { return m_chunks.size(); }

        /// @brief indexに対応するChunkを取得する。
        Chunk& GetChunk(size_t index) noexcept { return *m_chunks[index]; }
        const Chunk& GetChunk(size_t index) const noexcept { return *m_chunks[index]; }

        /// @brief 空き行を持つChunkを返し、必要なら新規Chunkを確保する。
        /// @pre IsValidがtrue
        Chunk& GetOrCreateWritableChunk();

        /// @brief Component IDに対応する列番号を返す。
        size_t FindColumn(ComponentTypeId id) const noexcept;

    private:
        friend class ECSWorld;

        void RemoveLastRow(Chunk& chunk) noexcept;

        const ComponentRegistry* m_registry = nullptr;
        ArchetypeSignature m_signature;
        ChunkLayout m_layout;
        std::vector<std::unique_ptr<Chunk>> m_chunks;
        bool m_isValid = false;

        Chunk* m_activeChunk = nullptr;         // 現在の書き込み先
        std::vector<Chunk*> m_writableChunks;   // 書き込み先以外で空き行を持つChunk
    };
}

#endif // _ARCHETYPE_H_
