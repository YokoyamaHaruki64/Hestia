/*=============================================================================

 File   : ScriptRef.h
 Desc   : Script の移動に追従する参照を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/13
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _SCRIPT_REF_H_
#define _SCRIPT_REF_H_

#include <cstddef>
#include <cstdlib>
#include <utility>

namespace HestiaGame
{
    namespace Detail
    {
        template<class T>
        class ScriptStorage;

        template<class T>
        struct ScriptReference
        {
            T* m_script;
            std::size_t m_refCount = 1;

            void AddRef() noexcept { ++m_refCount; }

            void Release() noexcept
            {
                if (--m_refCount == 0)
                    delete this;
            }
        };
    }

    /// @brief Storage 内の移動に追従し、Script の削除予約で無効になる参照。
    /// @note 生成した GameRuntime と同じスレッドで保持・操作する。
    template<class T>
    class ScriptRef
    {
    public:
        ScriptRef() noexcept = default;

        ScriptRef(const ScriptRef& other) noexcept : m_reference(other.m_reference)
        {
            if (m_reference != nullptr)
                m_reference->AddRef();
        }

        ScriptRef(ScriptRef&& other) noexcept
            : m_reference(std::exchange(other.m_reference, nullptr))
        {
        }

        ScriptRef& operator=(ScriptRef other) noexcept
        {
            std::swap(m_reference, other.m_reference);
            return *this;
        }

        ~ScriptRef()
        {
            if (m_reference != nullptr)
                m_reference->Release();
        }

        /// @brief Script を現在参照できるか返す。
        explicit operator bool() const noexcept { return Get() != nullptr; }

        /// @brief 現在の Script 本体を借用する。
        /// @pre 参照が有効である。無効な場合は処理を停止する。
        /// @return 次の Storage 構造変更まで利用できるポインタ。
        T* operator->() const noexcept
        {
            T* script = Get();
            if (script == nullptr)
                std::abort();
            return script;
        }

    private:
        friend class Detail::ScriptStorage<T>;

        explicit ScriptRef(Detail::ScriptReference<T>* reference) noexcept : m_reference(reference)
        {
            m_reference->AddRef();
        }

        T* Get() const noexcept { return m_reference != nullptr ? m_reference->m_script : nullptr; }

        Detail::ScriptReference<T>* m_reference = nullptr;
    };
}

#endif // _SCRIPT_REF_H_
