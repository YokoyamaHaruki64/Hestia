/*=============================================================================

 File   : TransformProcess.cpp
 Desc   : Transformの階層更新と、特異な親を許容するWorld操作を実装する

------------------------------------------------------------------------------

 Date   : 2026/11/05
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Transform/TransformProcess.h"

#include <algorithm>
#include <cmath>

#include "ECS/ECSSystem.h"
#include "Log/Logger.h"

namespace Hestia
{
    namespace
    {
        Vector3 Basis(const Matrix4x4_SIMD& matrix, size_t row)
        {
            return { matrix[row][0], matrix[row][1], matrix[row][2] };
        }

        Matrix4x4_SIMD RotationMatrix(const Quaternion& rotation)
        {
            return Quaternion_SIMD::Load(rotation).ToMatrix();
        }

        bool IsFinite(const Vector3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        bool IsFinite(const Quaternion& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) &&
                std::isfinite(value.z) && std::isfinite(value.w);
        }

        Vector3 SignedLengths(const Matrix4x4_SIMD& matrix, const Matrix4x4_SIMD& rotation)
        {
            // 基底の長さを拡縮とし、基準回転との内積から符号を決める
            float values[3];
            for (size_t row = 0; row < 3; ++row)
            {
                const Vector3 basis = Basis(matrix, row);
                values[row] = basis.Length() *
                    (Vector3::Dot(basis, Basis(rotation, row)) < 0.0f ? -1.0f : 1.0f);
            }
            return { values[0], values[1], values[2] };
        }
    }

    ECSWorld* TransformProcess::Resolve(EntityHandle handle) noexcept
    {
        // 初期化済みの間だけWorldを借用する
        ECSSystem* system = ECSSystem::s_instance;
        if (system == nullptr || !system->m_isInitialized)
            return nullptr;

        // 操作のたびにHandleを引き直し、破棄予約済みのEntityを除く
        Entity* entity = system->m_world.GetEntity(handle);
        return entity != nullptr && !entity->IsDestroy() ? &system->m_world : nullptr;
    }

    void TransformProcess::MarkWorldDirty(ECSWorld& world, Transform& transform,
        const TransformHierarchy& hierarchy)
    {
        // dirtyな枝は伝播済みで、再計算も親が先なので再通知は不要
        if (transform.m_worldDirty)
            return;

        // 自身のCacheを無効化し、生存中の子へ通知を伝える
        transform.m_worldDirty = true;
        for (EntityHandle child : hierarchy.m_children)
        {
            const Entity* entity = world.GetEntity(child);
            if (entity == nullptr || entity->IsDestroy())
                continue;

            MarkWorldDirty(world, *world.GetComponent<Transform>(child),
                *world.GetComponent<TransformHierarchy>(child));
        }
    }

    void TransformProcess::UpdateWorld(ECSWorld& world, Transform& transform,
        TransformCache& cache, const TransformHierarchy& hierarchy)
    {
        // Cacheが有効なら、親を検索せずに済む
        if (!transform.m_worldDirty)
            return;

        // Chunkの走査順に関係なく、合成に使う親のWorldを先に確定する
        const Matrix4x4_SIMD* parentWorld = nullptr;
        if (hierarchy.m_parent.IsValid())
            parentWorld = &EnsureWorld(world, hierarchy.m_parent);

        // 親だけが変わった場合はLocal行列を再利用する
        if (transform.m_localDirty)
        {
            const Vector3& scale = transform.m_localScale;
            const Vector3& position = transform.m_localPosition;
            transform.m_localMatrix = Matrix4x4_SIMD::Scaling(scale.x, scale.y, scale.z);
            transform.m_localMatrix *= RotationMatrix(transform.m_localRotation);
            transform.m_localMatrix.Translate(position.x, position.y, position.z);
            transform.m_localDirty = false;
        }

        // Localを親のWorldと合成し、書き込みが済んでからCacheを有効にする
        cache.m_worldMatrix = transform.m_localMatrix;
        if (parentWorld != nullptr)
            cache.m_worldMatrix *= *parentWorld;
        transform.m_worldDirty = false;
    }

    const Matrix4x4_SIMD& TransformProcess::EnsureWorld(ECSWorld& world, EntityHandle handle)
    {
        // 現在の列をHandleから解決し、PreRenderと同じ更新経路に渡す
        auto& transform = *world.GetComponent<Transform>(handle);
        auto& cache = *world.GetComponent<TransformCache>(handle);
        UpdateWorld(world, transform, cache, *world.GetComponent<TransformHierarchy>(handle));
        return cache.m_worldMatrix;
    }

    void TransformProcess::Run(size_t count, const Entity* entities, Transform* transforms,
        TransformCache* caches, const TransformHierarchy* hierarchies)
    {
        // activeにかかわらず列を走査し、破棄予約済みの行だけを除く
        ECSWorld& world = ECSSystem::s_instance->m_world;
        for (size_t index = 0; index < count; ++index)
        {
            if (entities[index].IsDestroy())
                continue;

            UpdateWorld(world, transforms[index],
                caches[index], hierarchies[index]);
        }
    }

    bool TransformProcess::WriteLocal(ECSWorld& world, Transform& transform,
        const TransformHierarchy& hierarchy, const Vector3& position,
        const Quaternion& rotation, const Vector3& scale)
    {
        // 途中まで書き換わらないよう、TRS一式を先に検証する
        if (!IsFinite(position) || !IsFinite(rotation) || !IsFinite(scale))
        {
            Logger::Error("Transform received a non-finite TRS");
            return false;
        }

        // 回転を正規化してLocalの正本を確定する
        transform.m_localPosition = position;
        transform.m_localRotation = rotation.Normalized();
        transform.m_localScale = scale;

        // 行列の再計算は遅延させ、子孫のWorldまで無効化する
        transform.m_localDirty = true;
        MarkWorldDirty(world, transform, hierarchy);
        return true;
    }

    bool TransformProcess::SetLocalTRS(Transform& transform, const TransformHierarchy& hierarchy,
        const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    {
        // 列から操作する場合も、子孫への伝播に使うWorldが必要
        if (ECSSystem::s_instance == nullptr || !ECSSystem::s_instance->m_isInitialized)
        {
            Logger::Error("TransformProcess is unavailable before ECS initialization");
            return false;
        }

        return WriteLocal(ECSSystem::s_instance->m_world, transform, hierarchy,
            position, rotation, scale);
    }

    Quaternion TransformProcess::WorldRotation(ECSWorld& world, EntityHandle handle)
    {
        // 拡縮の影響を受けないよう、Local回転を子から親の順に合成する
        Quaternion rotation = Quaternion::Identity();
        while (handle.IsValid())
        {
            rotation = (rotation * world.GetComponent<Transform>(handle)->m_localRotation).Normalized();
            handle = world.GetComponent<TransformHierarchy>(handle)->m_parent;
        }
        return rotation;
    }

    Vector3 TransformProcess::ScaleFactors(ECSWorld& world, EntityHandle parent,
        const Quaternion& localRotation, const Quaternion& worldRotation)
    {
        // Local拡縮を掛ける前の基底に親の変換を適用し、各軸の倍率を求める
        Matrix4x4_SIMD basis = RotationMatrix(localRotation);
        if (parent.IsValid())
            basis *= EnsureWorld(world, parent);
        return SignedLengths(basis, RotationMatrix(worldRotation));
    }

    Vector3 TransformProcess::WorldScale(ECSWorld& world, EntityHandle handle)
    {
        const auto& transform = *world.GetComponent<Transform>(handle);
        const Vector3 factors = ScaleFactors(world,
            world.GetComponent<TransformHierarchy>(handle)->m_parent,
            transform.m_localRotation, WorldRotation(world, handle));
        return { transform.m_localScale.x * factors.x,
            transform.m_localScale.y * factors.y, transform.m_localScale.z * factors.z };
    }

    Vector3 TransformProcess::LocalPosition(const Matrix4x4_SIMD& parentWorld,
        const Vector3& oldLocal, const Vector3& worldPosition)
    {
        // 現在のLocalで得られるWorld位置を引き、補正に必要な差分を求める
        double linear[3][3];
        double gram[3][3]{};
        double eigenvectors[3][3]{};
        double residual[3] = { worldPosition.x, worldPosition.y, worldPosition.z };
        const double old[3] = { oldLocal.x, oldLocal.y, oldLocal.z };
        for (size_t column = 0; column < 3; ++column)
        {
            residual[column] -= parentWorld[3][column];
            for (size_t row = 0; row < 3; ++row)
            {
                linear[row][column] = parentWorld[row][column];
                residual[column] -= old[row] * linear[row][column];
            }
        }

        // 親の線形部から、最小二乗の正規方程式に使う対称行列を作る
        for (size_t row = 0; row < 3; ++row)
        {
            eigenvectors[row][row] = 1.0;
            for (size_t column = 0; column < 3; ++column)
                for (size_t axis = 0; axis < 3; ++axis)
                    gram[row][column] += linear[row][axis] * linear[column][axis];
        }

        // Jacobi法で対角化し、親の変換で潰れる方向を判定できるようにする
        constexpr size_t MAX_JACOBI_STEPS = 32;
        constexpr double RELATIVE_RANK_TOLERANCE_SQUARED = 1.0e-12;
        for (size_t step = 0; step < MAX_JACOBI_STEPS; ++step)
        {
            // 最大の非対角成分を選び、十分小さければ対角化を終える
            size_t p = 0;
            size_t q = 1;
            for (size_t row = 0; row < 3; ++row)
                for (size_t column = row + 1; column < 3; ++column)
                    if (std::abs(gram[row][column]) > std::abs(gram[p][q]))
                    {
                        p = row;
                        q = column;
                    }

            const double largest = std::max({ std::abs(gram[0][0]),
                std::abs(gram[1][1]), std::abs(gram[2][2]) });
            if (std::abs(gram[p][q]) <= largest * 1.0e-15)
                break;

            // 選んだ平面を回転し、補正の復元に使う固有ベクトルも更新する
            const double tau = (gram[q][q] - gram[p][p]) / (2.0 * gram[p][q]);
            const double tangent = std::copysign(1.0, tau) /
                (std::abs(tau) + std::sqrt(1.0 + tau * tau));
            const double cosine = 1.0 / std::sqrt(1.0 + tangent * tangent);
            const double sine = tangent * cosine;
            gram[p][p] -= tangent * gram[p][q];
            gram[q][q] += tangent * gram[p][q];
            gram[p][q] = gram[q][p] = 0.0;
            for (size_t row = 0; row < 3; ++row)
            {
                if (row != p && row != q)
                {
                    const double a = gram[row][p];
                    const double b = gram[row][q];
                    gram[row][p] = gram[p][row] = cosine * a - sine * b;
                    gram[row][q] = gram[q][row] = sine * a + cosine * b;
                }
                const double a = eigenvectors[row][p];
                const double b = eigenvectors[row][q];
                eigenvectors[row][p] = cosine * a - sine * b;
                eigenvectors[row][q] = sine * a + cosine * b;
            }
        }

        // 最大固有値を基準に、逆算できない方向を判定する閾値を決める
        const double cutoff = std::max({ gram[0][0], gram[1][1], gram[2][2] }) *
            RELATIVE_RANK_TOLERANCE_SQUARED;

        // Worldの差分を正規方程式の右辺へ写し、元のLocalを補正の基準にする
        double right[3]{};
        double result[3] = { old[0], old[1], old[2] };
        for (size_t row = 0; row < 3; ++row)
            for (size_t column = 0; column < 3; ++column)
                right[row] += residual[column] * linear[row][column];

        // 逆算できる方向だけを補正し、潰れた方向には元のLocal成分を残す
        for (size_t axis = 0; axis < 3; ++axis)
        {
            if (gram[axis][axis] <= cutoff)
                continue;

            double amount = 0.0;
            for (size_t row = 0; row < 3; ++row)
                amount += right[row] * eigenvectors[row][axis];
            amount /= gram[axis][axis];
            for (size_t row = 0; row < 3; ++row)
                result[row] += amount * eigenvectors[row][axis];
        }
        return { static_cast<float>(result[0]), static_cast<float>(result[1]),
            static_cast<float>(result[2]) };
    }

    bool TransformProcess::WriteWorld(ECSWorld& world, EntityHandle parent, Transform& transform,
        const TransformHierarchy& hierarchy, const Vector3& position,
        const Quaternion& rotation, const Vector3& scale)
    {
        // 不正な入力では座標変換やLocalへの書き込みに進まない
        if (!IsFinite(position) || !IsFinite(rotation) || !IsFinite(scale))
        {
            Logger::Error("Transform received a non-finite World TRS");
            return false;
        }

        // 親の回転を除き、拡縮の逆算にも使うLocal回転を先に求める
        const Quaternion worldRotation = rotation.Normalized();
        const Quaternion parentRotation = parent.IsValid() ? WorldRotation(world, parent) :
            Quaternion::Identity();
        const Quaternion localRotation = (worldRotation * parentRotation.Inversed()).Normalized();

        // 親のWorldから、維持できる範囲でLocal位置を求める
        const Matrix4x4_SIMD parentWorld = parent.IsValid() ? EnsureWorld(world, parent) :
            Matrix4x4_SIMD::Identity();
        const Vector3 positionLocal = LocalPosition(parentWorld, transform.m_localPosition, position);

        // 求めた回転の基底で拡縮を逆算し、潰れた軸は元のLocal値を残す
        const Vector3 factors = ScaleFactors(world, parent, localRotation, worldRotation);
        const Vector3 scaleLocal = {
            factors.x == 0.0f ? transform.m_localScale.x : scale.x / factors.x,
            factors.y == 0.0f ? transform.m_localScale.y : scale.y / factors.y,
            factors.z == 0.0f ? transform.m_localScale.z : scale.z / factors.z };

        // 変換後のTRSをまとめて確定し、子孫のCacheも無効化する
        return WriteLocal(world, transform, hierarchy, positionLocal, localRotation, scaleLocal);
    }

    bool TransformProcess::DecomposeWorld(const Matrix4x4_SIMD& matrix,
        const Quaternion& fallback, Vector3& position, Quaternion& rotation, Vector3& scale)
    {
        // 非有限値や射影成分を含む行列はTRSへの近似対象から外す
        for (size_t index = 0; index < 16; ++index)
            if (!std::isfinite(matrix.At(index)))
                return false;

        constexpr float AFFINE_TOLERANCE = 1.0e-6f;
        if (std::abs(matrix[0][3]) > AFFINE_TOLERANCE ||
            std::abs(matrix[1][3]) > AFFINE_TOLERANCE ||
            std::abs(matrix[2][3]) > AFFINE_TOLERANCE ||
            std::abs(matrix[3][3] - 1.0f) > AFFINE_TOLERANCE)
            return false;

        // X基底を回転の基準にし、軸が潰れていれば現在の回転から補う
        const Matrix4x4_SIMD previous = RotationMatrix(fallback);
        Vector3 x = Basis(matrix, 0);
        if (x.LengthSqr() == 0.0f)
            x = Basis(previous, 0);
        x = x.Normalized();

        // Xへの射影を除いてYを直交化し、せん断を回転へ持ち込まない
        Vector3 y = Basis(matrix, 1);
        y -= x * Vector3::Dot(x, y);
        if (y.LengthSqr() < AFFINE_TOLERANCE * AFFINE_TOLERANCE)
        {
            y = Basis(previous, 1);
            y -= x * Vector3::Dot(x, y);
        }
        if (y.LengthSqr() < AFFINE_TOLERANCE * AFFINE_TOLERANCE)
        {
            // 現在の回転からもYが得られなければ、Xと平行になりにくい軸を使う
            y = std::abs(x.y) < 0.9f ? Vector3::Up() : Vector3::Right();
            y -= x * Vector3::Dot(x, y);
        }

        // 外積で回転基底を確定し、元の基底との比較から拡縮の符号を求める
        y = y.Normalized();
        const Vector3 z = Vector3::Cross(x, y).Normalized();
        y = Vector3::Cross(z, x);
        const Matrix4x4_SIMD orthogonal(
            x.x, x.y, x.z, 0.0f, y.x, y.y, y.z, 0.0f,
            z.x, z.y, z.z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        rotation = Quaternion_SIMD::FromRotationMatrix(orthogonal).Store().Normalized();
        position = Basis(matrix, 3);
        scale = SignedLengths(matrix, orthogonal);
        return true;
    }
}
