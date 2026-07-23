#pragma once

#include <cmath>
#include <cstdint>

namespace game::navigation
{

/*
    Возвращает индекс непосредственного родителя.

    Работает с GalaxyGridIndex и CubicGridIndex:
    оба типа содержат поля x, y, z.
*/
template<typename Index>
Index cubicParentIndex(
    const Index& childIndex,
    int subdivision
)
{
    const double divisor =
        static_cast<double>(subdivision);

    Index parentIndex;

    parentIndex.x =
        static_cast<std::int64_t>(
            std::llround(
                static_cast<double>(childIndex.x) /
                divisor
            )
        );

    parentIndex.y =
        static_cast<std::int64_t>(
            std::llround(
                static_cast<double>(childIndex.y) /
                divisor
            )
        );

    parentIndex.z =
        static_cast<std::int64_t>(
            std::llround(
                static_cast<double>(childIndex.z) /
                divisor
            )
        );

    return parentIndex;
}


/*
    Проверяет, принадлежит ли index набору дочерних кубов parent.

    Для subdivision = 25 допустимый диапазон:
        parent * 25 - 12
        ...
        parent * 25 + 12
*/
template<typename Index>
bool cubicIndexBelongsToParent(
    const Index& index,
    const Index& parentIndex,
    int subdivision
)
{
    const std::int64_t division =
        static_cast<std::int64_t>(
            subdivision
        );

    const std::int64_t half =
        division / 2;

    const std::int64_t centerX =
        parentIndex.x * division;

    const std::int64_t centerY =
        parentIndex.y * division;

    const std::int64_t centerZ =
        parentIndex.z * division;

    return
        index.x >= centerX - half &&
        index.x <= centerX + half &&

        index.y >= centerY - half &&
        index.y <= centerY + half &&

        index.z >= centerZ - half &&
        index.z <= centerZ + half;
}

} // namespace game::navigation