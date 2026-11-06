/*=============================================================================

 File   : MaterialShader.h
 Desc   : Material のスキーマと Pass の CPU データを定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/06
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _MATERIAL_SHADER_H_
#define _MATERIAL_SHADER_H_

#include <cstddef>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include "Asset/MaterialProperty.h"
#include "Asset/AssetType.h"

namespace Hestia
{
    class Material;
    template<class T> class AssetStorage;

    enum class RenderType
    {
        GBuffer,
        Forward
    };

    enum class RenderPhase
    {
        Opaque,
        Masked,
        Transparent
    };

    enum class FillMode
    {
        Solid,
        Wireframe
    };

    enum class CullMode
    {
        None,
        Front,
        Back
    };

    enum class BlendMode
    {
        None,
        Alpha,
        Additive
    };

    enum class CompareFunc
    {
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        Equal,
        NotEqual,
        Always
    };

    enum class PrimitiveTopologyType
    {
        Triangle,
        Line,
        Point
    };

    struct RasterizerState
    {
        FillMode m_fill = FillMode::Solid;
        CullMode m_cull = CullMode::Back;
        bool m_depthClip = true;
    };

    struct DepthStencilState
    {
        bool m_depthTest = true;
        bool m_depthWrite = true;
        CompareFunc m_depthFunc = CompareFunc::LessEqual;
    };

    struct ShaderPipelineDesc
    {
        std::vector<std::byte> m_vertexShader;
        std::vector<std::byte> m_pixelShader;
        std::vector<VertexStream> m_requiredVertexStreams; ///< VS が要求する Stream。重複なし
        RasterizerState m_rasterizer;
        BlendMode m_blend = BlendMode::None;
        DepthStencilState m_depthStencil;
        PrimitiveTopologyType m_topology = PrimitiveTopologyType::Triangle;
    };

    struct ShaderPassInfo
    {
        std::string m_name;
        RenderType m_renderType = RenderType::GBuffer;
        RenderPhase m_renderPhase = RenderPhase::Opaque;
        uint8_t m_shadingModelID = 0;
    };

    struct MaterialShaderPass
    {
        ShaderPipelineDesc m_pipeline;
        ShaderPassInfo m_info;
    };

    /// @brief Material の共通スキーマと Pass 定義を所有する CPU Asset。
    class MaterialShader
    {
    public:

        /// @brief メモリ上の定義から Shader を構築する
        /// @param name Shader の論理名
        /// @param properties Property と各 Pass の Reflection を統合済みの配置
        /// @param constantBufferSize 共通 CBuffer のバイト数。CBV 用の余白は含めない
        /// @param passes CPU 上の Pass 定義。スキーマだけを使う場合は空
        /// @return 構築済み Shader。名前、Property ID、配置が不正なら nullptr
        static std::unique_ptr<MaterialShader> Create(
            std::string name,
            std::vector<MaterialPropertyDesc> properties,
            uint32_t constantBufferSize,
            std::vector<MaterialShaderPass> passes);

        /// @brief 参照中の Material を切り離して Shader を破棄する。
        ~MaterialShader();

        MaterialShader(const MaterialShader&) = delete;
        MaterialShader& operator=(const MaterialShader&) = delete;

        /// @brief ID に対応する定義を借用する。未定義なら nullptr。
        const MaterialPropertyDesc* FindProperty(MaterialPropertyID id) const;

        /// @brief 名前に対応する定義を借用する。未定義なら nullptr。
        const MaterialPropertyDesc* FindProperty(std::string_view name) const;

        /// @brief 宣言順の Property 定義。Shader の寿命内で有効。
        std::span<const MaterialPropertyDesc> GetProperties() const { return m_properties; }

        /// @brief 宣言順の Pass 定義。Shader の寿命内で有効。
        std::span<const MaterialShaderPass> GetPasses() const { return m_passes; }

        /// @brief 共通 CBuffer のバイト数。
        uint32_t GetConstantBufferSize() const { return m_constantBufferSize; }

        /// @brief Shader の論理名。
        std::string_view GetName() const { return m_name; }

        /// @brief 登録済みの識別子。未登録なら無効。
        AssetHandle<MaterialShader> GetHandle() const { return m_handle; }

    private:
        MaterialShader() = default;

        friend class Material;
        friend class AssetStorage<MaterialShader>;

        AssetHandle<MaterialShader> m_handle;           // 自身のAssetHandle
        std::string m_name;                             // Shader の論理名
        std::vector<MaterialPropertyDesc> m_properties; // Property定義
        std::unordered_map<MaterialPropertyID, std::size_t> m_propertyIndices;  // PropertyID から m_properties のindexへの逆索引
        uint32_t m_constantBufferSize = 0;              // CBufferのバイト数
        std::vector<MaterialShaderPass> m_passes;       // Pass定義の配列
        std::vector<Material*> m_materials;             // このShaderを参照するMaterialの逆索引
    };
}

#endif // _MATERIAL_SHADER_H_
