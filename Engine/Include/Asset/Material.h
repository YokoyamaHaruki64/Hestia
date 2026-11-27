/*=============================================================================

 File   : Material.h
 Desc   : Shader を参照する Material Asset と型付き Property 値を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/06
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _MATERIAL_H_
#define _MATERIAL_H_

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "Asset/MaterialProperty.h"
#include "Log/Logger.h"

namespace Hestia
{
    class MaterialShader;
    template<class T> class AssetLoader;
    template<class T> struct Serializer;
    template<class T> class AssetStorage;

    /// @brief Shader の参照と、型付き Property 値および転送用キャッシュを保持する。
    class Material
    {
    public:
        Material() = default;

        /// @brief Shader の逆索引から自身を解除する。
        ~Material();

        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        /// @brief 名前を UTF-8 バイト列としてハッシュし、Shader 非依存の ID を生成する。
        static MaterialPropertyID MakePropertyID(std::string_view name);

        /// @brief Shader を付け替え、同名・同型の値を引き継ぐ。nullptr で参照を解除する。
        /// @param shader 非所有の Shader。Shader の破棄時には参照が自動的に解除される。
        void SetShader(MaterialShader* shader);

        /// @brief 現在の Shader を借用する。未設定または解除済みなら nullptr。
        const MaterialShader* GetShader() const { return m_shader; }

        /// @brief bool値を指定したPropertyIDに設定する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @param value 設定するbool値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetBool(MaterialPropertyID id, bool value) { return SetProperty(id, value); }

        /// @brief bool値を指定したProperty名に設定する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @param value 設定するbool値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetBool(std::string_view name, bool value) { return SetBool(MakePropertyID(name), value); }

        /// @brief bool値を指定したPropertyIDから取得する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @return 取得したbool値。PropertyIDがShaderに存在しない場合はfalseを返す
        bool GetBool(MaterialPropertyID id) const
        {
            bool ret;
            if (GetProperty(id, ret)) return ret;
            else
            {
                Logger::Error("Material GetBool failed: PropertyID not found in Shader");
                return false;
            }
                
        }

        /// @brief bool値を指定したProperty名から取得する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @return 取得したbool値。PropertyIDがShaderに存在しない場合はfalseを返す
        bool GetBool(std::string_view name) const { return GetBool(MakePropertyID(name)); }

        /// @brief Int値を指定したPropertyIDに設定する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @param value 設定するInt値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetInt(MaterialPropertyID id, int32_t value) { return SetProperty(id, value); }

        /// @brief Int値を指定したProperty名に設定する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @param value 設定するInt値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetInt(std::string_view name, int32_t value) { return SetInt(MakePropertyID(name), value); }

        /// @brief Int値を指定したPropertyIDから取得する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @return 取得したInt値。PropertyIDがShaderに存在しない場合は既定値を返す
        int32_t GetInt(MaterialPropertyID id) const
        {
            int32_t ret = int32_t{};
            if (GetProperty(id, ret)) return ret;
            else
            {
                Logger::Error("Material GetInt failed: PropertyID not found in Shader");
                return int32_t{};
            }
        }

        /// @brief Int値を指定したProperty名から取得する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @return 取得したInt値。PropertyIDがShaderに存在しない場合は既定値を返す
        int32_t GetInt(std::string_view name) const { return GetInt(MakePropertyID(name)); }

        /// @brief Float値を指定したPropertyIDに設定する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @param value 設定するFloat値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetFloat(MaterialPropertyID id, float value) { return SetProperty(id, value); }

        /// @brief Float値を指定したProperty名に設定する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @param value 設定するFloat値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetFloat(std::string_view name, float value) { return SetFloat(MakePropertyID(name), value); }

        /// @brief Float値を指定したPropertyIDから取得する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @return 取得したFloat値。PropertyIDがShaderに存在しない場合は既定値を返す
        float GetFloat(MaterialPropertyID id) const
        {
            float ret = 0.0f;
            if (GetProperty(id, ret)) return ret;
            else
            {
                Logger::Error("Material GetFloat failed: PropertyID not found in Shader");
                return 0.0f;
            }
        }

        /// @brief Float値を指定したProperty名から取得する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @return 取得したFloat値。PropertyIDがShaderに存在しない場合は既定値を返す
        float GetFloat(std::string_view name) const { return GetFloat(MakePropertyID(name)); }

        /// @brief Float2値を指定したPropertyIDに設定する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @param value 設定するFloat2値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetFloat2(MaterialPropertyID id, const Vector2& value) { return SetProperty(id, value); }

        /// @brief Float2値を指定したProperty名に設定する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @param value 設定するFloat2値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetFloat2(std::string_view name, const Vector2& value) { return SetFloat2(MakePropertyID(name), value); }

        /// @brief Float2値を指定したPropertyIDから取得する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @return 取得したFloat2値。PropertyIDがShaderに存在しない場合は既定値を返す
        Vector2 GetFloat2(MaterialPropertyID id) const
        {
            Vector2 ret = Vector2{};
            if (GetProperty(id, ret)) return ret;
            else
            {
                Logger::Error("Material GetFloat2 failed: PropertyID not found in Shader");
                return Vector2{};
            }
        }

        /// @brief Float2値を指定したProperty名から取得する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @return 取得したFloat2値。PropertyIDがShaderに存在しない場合は既定値を返す
        Vector2 GetFloat2(std::string_view name) const { return GetFloat2(MakePropertyID(name)); }

        /// @brief Float3値を指定したPropertyIDに設定する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @param value 設定するFloat3値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetFloat3(MaterialPropertyID id, const Vector3& value) { return SetProperty(id, value); }

        /// @brief Float3値を指定したProperty名に設定する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @param value 設定するFloat3値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetFloat3(std::string_view name, const Vector3& value) { return SetFloat3(MakePropertyID(name), value); }

        /// @brief Float3値を指定したPropertyIDから取得する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @return 取得したFloat3値。PropertyIDがShaderに存在しない場合は既定値を返す
        Vector3 GetFloat3(MaterialPropertyID id) const
        {
            Vector3 ret = Vector3{};
            if (GetProperty(id, ret)) return ret;
            else
            {
                Logger::Error("Material GetFloat3 failed: PropertyID not found in Shader");
                return Vector3{};
            }
        }

        /// @brief Float3値を指定したProperty名から取得する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @return 取得したFloat3値。PropertyIDがShaderに存在しない場合は既定値を返す
        Vector3 GetFloat3(std::string_view name) const { return GetFloat3(MakePropertyID(name)); }

        /// @brief Float4値を指定したPropertyIDに設定する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @param value 設定するFloat4値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetFloat4(MaterialPropertyID id, const Vector4& value) { return SetProperty(id, value); }

        /// @brief Float4値を指定したProperty名に設定する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @param value 設定するFloat4値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetFloat4(std::string_view name, const Vector4& value) { return SetFloat4(MakePropertyID(name), value); }

        /// @brief Float4値を指定したPropertyIDから取得する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @return 取得したFloat4値。PropertyIDがShaderに存在しない場合は既定値を返す
        Vector4 GetFloat4(MaterialPropertyID id) const
        {
            Vector4 ret = Vector4{};
            if (GetProperty(id, ret)) return ret;
            else
            {
                Logger::Error("Material GetFloat4 failed: PropertyID not found in Shader");
                return Vector4{};
            }
        }

        /// @brief Float4値を指定したProperty名から取得する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @return 取得したFloat4値。PropertyIDがShaderに存在しない場合は既定値を返す
        Vector4 GetFloat4(std::string_view name) const { return GetFloat4(MakePropertyID(name)); }

        /// @brief Color値を指定したPropertyIDに設定する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @param value 設定するColor値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetColor(MaterialPropertyID id, const Color& value) { return SetProperty(id, value); }

        /// @brief Color値を指定したProperty名に設定する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @param value 設定するColor値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetColor(std::string_view name, const Color& value) { return SetColor(MakePropertyID(name), value); }

        /// @brief Color値を指定したPropertyIDから取得する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @return 取得したColor値。PropertyIDがShaderに存在しない場合は既定値を返す
        Color GetColor(MaterialPropertyID id) const
        {
            Color ret = Color{};
            if (GetProperty(id, ret)) return ret;
            else
            {
                Logger::Error("Material GetColor failed: PropertyID not found in Shader");
                return Color{};
            }
        }

        /// @brief Color値を指定したProperty名から取得する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @return 取得したColor値。PropertyIDがShaderに存在しない場合は既定値を返す
        Color GetColor(std::string_view name) const
        { return GetColor(MakePropertyID(name)); }

        /// @brief Matrix3x3値を指定したPropertyIDに設定する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @param value 設定するMatrix3x3値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetMatrix3x3(MaterialPropertyID id, const Matrix3x3& value) { return SetProperty(id, value); }

        /// @brief Matrix3x3値を指定したProperty名に設定する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @param value 設定するMatrix3x3値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetMatrix3x3(std::string_view name, const Matrix3x3& value) { return SetMatrix3x3(MakePropertyID(name), value); }

        /// @brief Matrix3x3値を指定したPropertyIDから取得する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @return 取得したMatrix3x3値。PropertyIDがShaderに存在しない場合は既定値を返す
        Matrix3x3 GetMatrix3x3(MaterialPropertyID id) const
        {
            Matrix3x3 ret = Matrix3x3{};
            if (GetProperty(id, ret)) return ret;
            else
            {
                Logger::Error("Material GetMatrix3x3 failed: PropertyID not found in Shader");
                return Matrix3x3{};
            }
        }

        /// @brief Matrix3x3値を指定したProperty名から取得する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @return 取得したMatrix3x3値。PropertyIDがShaderに存在しない場合は既定値を返す
        Matrix3x3 GetMatrix3x3(std::string_view name) const
        { return GetMatrix3x3(MakePropertyID(name)); }

        /// @brief Matrix4x4値を指定したPropertyIDに設定する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @param value 設定するMatrix4x4値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetMatrix4x4(MaterialPropertyID id, const Matrix4x4& value) { return SetProperty(id, value); }

        /// @brief Matrix4x4値を指定したProperty名に設定する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @param value 設定するMatrix4x4値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetMatrix4x4(std::string_view name, const Matrix4x4& value) { return SetMatrix4x4(MakePropertyID(name), value); }

        /// @brief Matrix4x4値を指定したPropertyIDから取得する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @return 取得したMatrix4x4値。PropertyIDがShaderに存在しない場合は既定値を返す
        Matrix4x4 GetMatrix4x4(MaterialPropertyID id) const
        {
            Matrix4x4 ret = Matrix4x4{};
            if (GetProperty(id, ret)) return ret;
            else
            {
                Logger::Error("Material GetMatrix4x4 failed: PropertyID not found in Shader");
                return Matrix4x4{};
            }
        }

        /// @brief Matrix4x4値を指定したProperty名から取得する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @return 取得したMatrix4x4値。PropertyIDがShaderに存在しない場合は既定値を返す
        Matrix4x4 GetMatrix4x4(std::string_view name) const
        { return GetMatrix4x4(MakePropertyID(name)); }

        /// @brief Texture値を指定したPropertyIDに設定する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @param value 設定するTexture値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetTexture(MaterialPropertyID id, AssetHandle<Texture> value) { return SetProperty(id, value); }

        /// @brief Texture値を指定したProperty名に設定する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @param value 設定するTexture値
        /// @return 設定成功: true / PropertyIDがShaderに存在しない: false
        bool SetTexture(std::string_view name, AssetHandle<Texture> value) { return SetTexture(MakePropertyID(name), value); }

        /// @brief Texture値を指定したPropertyIDから取得する
        /// @param id PropertyID MakePropertyIDで生成可能
        /// @return 取得したTexture値。PropertyIDがShaderに存在しない場合は既定値を返す
        AssetHandle<Texture> GetTexture(MaterialPropertyID id) const
        {
            AssetHandle<Texture> ret = AssetHandle<Texture>{};
            if (GetProperty(id, ret)) return ret;
            else
            {
                Logger::Error("Material GetTexture failed: PropertyID not found in Shader");
                return AssetHandle<Texture>{};
            }
        }

        /// @brief Texture値を指定したProperty名から取得する
        /// @param name Property名 内部でMakePropertyIDを使用。頻繁に使う場合IDキャッシュ推奨
        /// @return 取得したTexture値。PropertyIDがShaderに存在しない場合は既定値を返す
        AssetHandle<Texture> GetTexture(std::string_view name) const
        { return GetTexture(MakePropertyID(name)); }

        /// @brief GPU 転送用 CBuffer の View。Shader の変更または破棄まで有効。
        std::span<const std::byte> GetConstantBuffer() const { return m_constantBuffer; }

        /// @brief 値の更新または Shader の差し替えごとに進む世代。
        uint64_t GetValueRevision() const { return m_valueRevision; }

        /// @brief 登録済みの識別子。未登録なら無効。
        AssetHandle<Material> GetHandle() const { return m_handle; }

    private:
        friend class MaterialShader;
        friend class AssetStorage<Material>;
        friend class AssetLoader<Material>;
        friend struct Serializer<Material>;

        bool SetProperty(MaterialPropertyID id, const MaterialPropertyValue& value);
        const MaterialPropertyValue* FindPropertyValue(MaterialPropertyID id) const;

        template<class T>
        bool GetProperty(MaterialPropertyID id, T& out) const
        {
            const T* value = std::get_if<T>(FindPropertyValue(id));
            if (value == nullptr)
                return false;

            out = *value;
            return true;
        }

        // Shader のレイアウト検証に実際のパックサイズを使う
        static std::size_t GetPackedConstantSize(const MaterialPropertyDesc& property);

        // Shader 側が逆索引を破棄する際に借用参照と依存データを解除
        void OnShaderDestroyed();

        AssetHandle<Material> m_handle;                 // 自分のHandle
        MaterialShader* m_shader = nullptr;             // 非所有の Shader 参照
        std::vector<MaterialPropertyValue> m_values;    // Property 値の配列。Shader の定義順に対応
        std::vector<std::byte> m_constantBuffer;        // GPU 転送用 CBuffer のキャッシュ
        uint64_t m_valueRevision = 0;                   // 値の更新または Shader の差し替えごとに進む世代
    };
}

#endif // _MATERIAL_H_
