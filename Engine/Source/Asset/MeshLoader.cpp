/*=============================================================================

 File   : MeshLoader.cpp
 Desc   : Assimpで階層付きモデルを読み込み、PrefabとMeshへ変換する。

------------------------------------------------------------------------------

 Date   : 2026/11/19
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Common/Include/StringUtils.h"
#include "Asset/AssetLoader.h"

#include <algorithm>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Asset/AssetSystem.h"
#include "Log/Logger.h"
#include "Serialize/SerializeFields.h"
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Hestia
{
    namespace
    {
        std::unique_ptr<Mesh> BuildMesh(const aiScene& scene, const aiNode& node,
            std::vector<uint32_t>& materialIndices, std::string_view& error)
        {
            const auto fail = [&error](std::string_view reason) -> std::unique_ptr<Mesh>
            {
                error = reason;
                return {};
            };

            // 全Meshのサイズと属性を先に集計し、Buffer上限内で必要なStreamだけを確保
            constexpr uint32_t unusedSlot = UINT32_MAX;
            std::vector<uint32_t> materialSlots(scene.mNumMaterials, unusedSlot);
            uint64_t vertexCount = 0;
            uint64_t indexCount = 0;

            bool hasNormalTangents = false;
            bool hasUVs = false;
            bool hasColors = false;
            for (unsigned int i = 0; i < node.mNumMeshes; ++i)
            {
                const aiMesh& source = *scene.mMeshes[node.mMeshes[i]];
                if (!source.HasPositions() || !source.HasFaces())
                    return fail("Mesh has no vertices or faces");
                if (source.mMaterialIndex >= materialSlots.size())
                    return fail("Mesh refers to an invalid material index");

                vertexCount += source.mNumVertices;
                indexCount += uint64_t{3} * source.mNumFaces;
                if (vertexCount > UINT32_MAX / sizeof(PositionVertex) ||
                    indexCount > UINT32_MAX / sizeof(uint32_t))
                    return fail("Mesh exceeds the GPU buffer size limit");

                materialSlots[source.mMaterialIndex] = 0;
                hasNormalTangents |= source.HasNormals() || source.HasTangentsAndBitangents();
                hasUVs |= source.HasTextureCoords(0);
                hasColors |= source.HasVertexColors(0);
            }
            if ((hasNormalTangents && vertexCount > UINT32_MAX / sizeof(NormalTangentVertex)) ||
                (hasUVs && vertexCount > UINT32_MAX / sizeof(PrimaryTexCoordVertex)) ||
                (hasColors && vertexCount > UINT32_MAX / sizeof(ColorVertex)))
                return fail("Mesh exceeds the GPU vertex stream size limit");

            for (uint32_t index = 0; index < materialSlots.size(); ++index)
            {
                auto& slot = materialSlots[index];
                if (slot != unusedSlot)
                {
                    slot = static_cast<uint32_t>(materialIndices.size());
                    materialIndices.push_back(index);
                }
            }

            auto mesh = std::make_unique<Mesh>();
            mesh->m_positions.reserve(static_cast<std::size_t>(vertexCount));
            if (hasNormalTangents)
                mesh->m_normalTangents.resize(static_cast<std::size_t>(vertexCount));
            if (hasUVs)
                mesh->m_primaryTexCoords.resize(static_cast<std::size_t>(vertexCount));
            if (hasColors)
                mesh->m_colors.resize(static_cast<std::size_t>(vertexCount), ColorVertex{{ 1.0f, 1.0f, 1.0f, 1.0f }});
            mesh->m_indices.reserve(static_cast<std::size_t>(indexCount));
            mesh->m_subMeshes.reserve(node.mNumMeshes);

            for (unsigned int i = 0; i < node.mNumMeshes; ++i)
            {
                const aiMesh& source = *scene.mMeshes[node.mMeshes[i]];
                const auto firstVertex = static_cast<uint32_t>(mesh->m_positions.size());
                const auto firstIndex = static_cast<uint32_t>(mesh->m_indices.size());

                // Mesh内の頂点番号を共有Index列へ移し、SubMeshに描画範囲を記録
                for (unsigned int faceIndex = 0; faceIndex < source.mNumFaces; ++faceIndex)
                {
                    const aiFace& face = source.mFaces[faceIndex];
                    if (face.mNumIndices != 3)
                        return fail("Only triangle faces are supported");
                    for (unsigned int corner = 0; corner < 3; ++corner)
                    {
                        const unsigned int index = face.mIndices[corner];
                        if (index >= source.mNumVertices)
                            return fail("Face refers to an invalid vertex index");
                        mesh->m_indices.push_back(firstVertex + index);
                    }
                }
                mesh->m_subMeshes.push_back({
                    firstIndex, static_cast<uint32_t>(mesh->m_indices.size()) - firstIndex,
                    materialSlots[source.mMaterialIndex]
                });

                // ファイルにある属性だけをコピーし、欠損頂点にはStreamの既定値を残す
                const bool hasNormals = source.HasNormals();
                const bool hasTangents = source.HasTangentsAndBitangents();
                const bool hasSourceUVs = source.HasTextureCoords(0);
                const bool hasSourceColors = source.HasVertexColors(0);
                for (unsigned int vertex = 0; vertex < source.mNumVertices; ++vertex)
                {
                    const auto dst = firstVertex + vertex;
                    const auto& position = source.mVertices[vertex];
                    mesh->m_positions.push_back({{ position.x, position.y, position.z }});
                    if (hasNormals)
                    {
                        const auto& normal = source.mNormals[vertex];
                        mesh->m_normalTangents[dst].m_normal = { normal.x, normal.y, normal.z };
                    }
                    if (hasTangents)
                    {
                        const auto& n = mesh->m_normalTangents[dst].m_normal;
                        const auto& t = source.mTangents[vertex];
                        const auto& b = source.mBitangents[vertex];
                        const float sign = ((aiVector3D(n[0], n[1], n[2]) ^ t) * b) < 0.0f ? -1.0f : 1.0f;
                        mesh->m_normalTangents[dst].m_tangent = { t.x, t.y, t.z, sign };
                    }
                    if (hasSourceUVs)
                    {
                        const auto& uv = source.mTextureCoords[0][vertex];
                        mesh->m_primaryTexCoords[dst].m_uv = { uv.x, uv.y };
                    }
                    if (hasSourceColors)
                    {
                        const auto& color = source.mColors[0][vertex];
                        mesh->m_colors[dst].m_color = { color.r, color.g, color.b, color.a };
                    }
                }
            }

            return mesh;
        }

        std::unique_ptr<Material> BuildMaterial(const aiMaterial& source, MaterialShader& shader)
        {
            auto material = std::make_unique<Material>();
            material->SetShader(&shader);

            // 取得できた値だけを既定のPBRパラメータに反映する
            aiColor4D color;
            if (aiGetMaterialColor(&source, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS ||
                aiGetMaterialColor(&source, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS)
                material->SetColor("_BaseColor", Color(color.r, color.g, color.b, color.a));

            ai_real value;
            if (aiGetMaterialFloat(&source, AI_MATKEY_METALLIC_FACTOR, &value) == AI_SUCCESS)
                material->SetFloat("_Metallic", value);
            if (aiGetMaterialFloat(&source, AI_MATKEY_ROUGHNESS_FACTOR, &value) == AI_SUCCESS)
                material->SetFloat("_Roughness", value);
            return material;
        }
    }

    AssetHandle<Prefab> AssetLoader<Prefab>::Load(const AssetInfo& info, const AssetDB& database)
    {
        const std::string path = database.SourcePath(info);
        const auto fail = [&info](std::string_view reason) -> AssetHandle<Prefab>
        {
            Logger::Error(std::format("Failed to import Model '{}': {}", info.m_path, reason));
            return {};
        };

        // Assimpのノード階層とローカル座標を保ったまま、描画側の座標規約に揃える
        unsigned int flags = aiProcess_ValidateDataStructure | aiProcess_Triangulate |
            aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder | aiProcess_FlipUVs;
        const auto extension = ToLowerAscii(std::string_view(path).substr(path.find_last_of('.')));
        const bool isObj = extension == ".obj";
        if (extension == ".fbx")
            flags |= aiProcess_GlobalScale;

        std::unique_ptr<const aiScene, decltype(&aiReleaseImport)> scene(
            aiImportFile(path.c_str(), flags), aiReleaseImport);
        if (!scene)
            return fail(aiGetErrorString());
        if ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene->mRootNode || !scene->HasMeshes())
            return fail("Scene is incomplete or has no meshes");

        // 同名Materialの有無を調べ、再インポート用のキーを決める
        constexpr std::string_view shaderPath = "Shaders/MatShaders/DefaultPBR.matsh";
        auto* shader = database.FindByPath(shaderPath)
            ? AssetSystem::Get(AssetSystem::Load<MaterialShader>(shaderPath)) : nullptr;
        if (!shader)
            Logger::Warning("DefaultPBR is unavailable for model: " + info.m_path);

        std::vector<std::string> materialNames(scene->mNumMaterials);
        std::unordered_map<std::string, uint32_t> nameCounts;
        for (uint32_t index = 0; index < scene->mNumMaterials; ++index)
        {
            aiString name;
            if (aiGetMaterialString(scene->mMaterials[index], AI_MATKEY_NAME, &name) == AI_SUCCESS)
            {
                materialNames[index] = name.C_Str();
                ++nameCounts[materialNames[index]];
            }
        }

        // MeshとMaterialで同じ連番を使い、使用するSubAssetだけを登録する
        auto prefab = std::make_unique<Prefab>();
        std::vector<std::pair<AssetRef, std::unique_ptr<Mesh>>> meshes;
        std::vector<std::pair<AssetRef, std::unique_ptr<Material>>> materials(scene->mNumMaterials);
        AssetInfo updated{ info.m_guid, info.m_type, info.m_path, info.m_nextLocalId, {}, {} };
        std::string error;
        const auto visit = [&](this const auto& visitNode, const aiNode& node,
            const std::string& parentPath, SerializedEntity& entity) -> bool
        {
            // OBJの合成ルート名はファイル名由来のため、改名で照合キーが変わらないよう固定する
            const bool isRoot = parentPath.empty();
            std::string name = isObj && isRoot ? "Root" : node.mName.C_Str();
            if (name.empty() && node.mNumMeshes != 0)
                name = scene->mMeshes[node.mMeshes[0]]->mName.C_Str();
            if (name.empty())
                name = "Node";

            const std::string nodePath = isRoot ? name : parentPath + "/" + name;
            if (isRoot)
                prefab->m_name = name;

            // 保存済みEntityと同じComponent構成とLocal TRSでノードを表す
            aiVector3D scale;
            aiVector3D position;
            aiQuaternion rotation;
            aiDecomposeMatrix(&node.mTransformation, &scale, &rotation, &position);
            using SerializeDetail::ToSerializedValue;

            entity.m_components = {
                { "Hestia::Name", { { "value", ToSerializedValue(name) } } },
                { "Hestia::Tag", {} },
                { "Hestia::Layer", {} },
                { "Hestia::Transform", {
                    { "position", ToSerializedValue(Vector3(position.x, position.y, position.z)) },
                    { "rotation", ToSerializedValue(Quaternion(rotation.x, rotation.y, rotation.z, rotation.w)) },
                    { "scale", ToSerializedValue(Vector3(scale.x, scale.y, scale.z)) },
                } },
                { "Hestia::TransformCache", {} },
                { "Hestia::TransformHierarchy", {} },
            };

            if (node.mNumMeshes != 0)
            {
                const auto old = info.m_subAssets.find(nodePath);
                const auto id = old != info.m_subAssets.end() ? old->second : updated.m_nextLocalId++;
                if (!updated.m_subAssets.emplace(nodePath, id).second)
                {
                    error = "Duplicate mesh node path: " + nodePath;
                    return false;
                }

                std::string_view meshError;
                std::vector<uint32_t> materialIndices;
                auto mesh = BuildMesh(*scene, node, materialIndices, meshError);
                if (!mesh)
                {
                    error = std::format("{}: {}", nodePath, meshError);
                    return false;
                }

                mesh->m_name = nodePath;
                std::ranges::replace(mesh->m_name, '/', '_');
                const AssetRef ref{ info.m_guid, id };
                auto& renderer = entity.m_components.emplace_back("Hestia::MeshRenderer");
                renderer.m_fields.emplace("mesh", ToSerializedValue(ref));
                meshes.emplace_back(ref, std::move(mesh));

                // Materialはモデル内で一度だけ作り、各RendererへSlot順に参照を並べる
                std::vector<AssetRef> references;
                for (const auto sourceIndex : materialIndices)
                {
                    auto& [materialRef, material] = materials[sourceIndex];
                    if (!materialRef.m_guid)
                    {
                        const auto& name = materialNames[sourceIndex];
                        const auto key = !name.empty() && nameCounts.at(name) == 1
                            ? "name:" + name : "index:" + std::to_string(sourceIndex);
                        const auto previous = info.m_materials.find(key);
                        const auto materialId = previous != info.m_materials.end()
                            ? previous->second : updated.m_nextLocalId++;
                        updated.m_materials.emplace(key, materialId);
                        materialRef = { info.m_guid, materialId };
                        if (shader)
                            material = BuildMaterial(*scene->mMaterials[sourceIndex], *shader);
                    }
                    if (shader)
                        references.push_back(materialRef);
                }
                renderer.m_fields.emplace("materials", ToSerializedValue(references));
            }

            // ノードの親子関係をSerializedEntityの所有関係へ移す
            entity.m_children.reserve(node.mNumChildren);
            for (unsigned int child = 0; child < node.mNumChildren; ++child)
            {
                if (!visitNode(*node.mChildren[child], nodePath, entity.m_children.emplace_back()))
                    return false;
            }
            return true;
        };

        if (!visit(*scene->mRootNode, "", prefab->m_root))
            return fail(error);

        // 全ノードを変換できた場合だけメタデータとCPU Assetを公開する
        if (!database.SaveModelMeta(info, updated))
            return {};

        for (auto& [ref, mesh] : meshes)
        {
            if (!AssetSystem::RegisterFromLoader(ref, std::move(mesh)).IsValid())
                return fail("CPU Mesh registration failed");
        }
        for (auto& [ref, material] : materials)
        {
            if (material && !AssetSystem::RegisterFromLoader(ref, std::move(material)).IsValid())
                return fail("CPU Material registration failed");
        }
        return AssetSystem::RegisterFromLoader(AssetRef{ info.m_guid, 0 }, std::move(prefab));
    }
}
