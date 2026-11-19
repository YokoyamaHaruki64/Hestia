/*=============================================================================

 File   : TransformFacade.cpp
 Desc   : Game向けTransform操作をEngine.dll内のECSへ接続する

------------------------------------------------------------------------------

 Date   : 2026/11/05
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Game/Transform.h"

namespace HestiaGame
{
    Transform::Transform(Hestia::EntityHandle entity) noexcept
        : m_entity(entity)
    {
        // Engine 内の利用箇所からは、同じ操作テーブルへ直接接続する
        static const auto api = Hestia::TransformAPIImpl::CreateAPI();
        m_api = &api;
    }
}
