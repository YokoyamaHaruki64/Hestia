/*=============================================================================

 File   : Component.h
 Desc   : すべてのEntityが保持する基本Componentを定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

#include <cstdint>
#include <string>

namespace Hestia
{
    struct Name
    {
        std::string m_value;
    };

    struct Tag
    {
        uint32_t m_value = 0;
    };

    struct Layer
    {
        uint32_t m_value = 0;
    };
}

#endif // _COMPONENT_H_
