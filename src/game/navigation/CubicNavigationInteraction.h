#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

namespace game::navigation
{

/*
    Результат обработки пользовательского действия.

    Этот enum не знает ничего о Galaxy, System,
    OpenGL, единицах измерения или конкретной камере.
*/
enum class CubicNavigationLevelAction
{
    None,

    /*
        Перейти к дочернему уровню.
        Текущий выбранный куб становится материнским.
    */
    Refine,

    /*
        Вернуться к родительскому уровню.
    */
    Coarsen,

    /*
        Внутренних уровней этой карты больше нет.
        Конкретная карта сама решает, существует ли
        следующий режим просмотра.
    */
    EnterChildMap
};


/*
    Общие параметры поведения кубической навигации.
*/
struct CubicNavigationInteractionPolicy
{
    /*
        При приближении колесом уровень уточняется,
        когда текущий куб занимает более 72% меньшей
        стороны viewport.
    */
    float refineViewportFraction = 0.72f;

    /*
        При отдалении уровень укрупняется,
        когда текущий куб занимает менее 18%.
    */
    float coarsenViewportFraction = 0.18f;

    /*
        После двойного клика новый материнский куб
        должен занимать около 86% меньшей стороны окна.
    */
    float parentFitViewportFraction = 0.86f;

    /*
        Поправка на экранный bounding box повёрнутого куба.
        Это значение уже используется в текущем рендере.
    */
    float projectedCubeExtentFactor = 1.35f;
};




/*
    Общий детектор двойного клика по кубу.

    Index может быть:
    - GalaxyGridIndex;
    - CubicGridIndex.

    Требование только одно:
    Index должен поддерживать operator==.
*/
template<typename Index>
class CubicNavigationClickTracker
{
public:
    bool registerClick(
        double nowSeconds,
        double screenX,
        double screenY,
        int level,
        const Index& index,
        double maxIntervalSeconds,
        double maxDistancePx
    )
    {
        const double deltaX =
            screenX - m_lastScreenX;

        const double deltaY =
            screenY - m_lastScreenY;

        const double distanceSquared =
            deltaX * deltaX +
            deltaY * deltaY;

        const double intervalSeconds =
            nowSeconds -
            m_lastTimeSeconds;

        const bool doubleClick =
            m_valid &&
            intervalSeconds >= 0.0 &&
            intervalSeconds <= maxIntervalSeconds &&
            distanceSquared <=
                maxDistancePx *
                maxDistancePx &&
            level == m_lastLevel &&
            index == m_lastIndex;

        if (doubleClick)
        {
            /*
                Третий клик не должен считаться ещё одним
                двойным кликом той же последовательности.
            */
            reset();
            return true;
        }

        m_valid = true;

        m_lastTimeSeconds =
            nowSeconds;

        m_lastScreenX =
            screenX;

        m_lastScreenY =
            screenY;

        m_lastLevel =
            level;

        m_lastIndex =
            index;

        return false;
    }

    void reset()
    {
        m_valid = false;
        m_lastTimeSeconds = 0.0;
        m_lastScreenX = 0.0;
        m_lastScreenY = 0.0;
        m_lastLevel = -1;
        m_lastIndex = {};
    }

private:
    bool m_valid = false;

    double m_lastTimeSeconds = 0.0;
    double m_lastScreenX = 0.0;
    double m_lastScreenY = 0.0;

    int m_lastLevel = -1;

    Index m_lastIndex {};
};


/*
    Общая смена уровня.

    Grid должен предоставлять:
    - refineAroundAnchor();
    - coarsenAroundAnchor();
    - anchorCell();
    - selectCell();
    - clearHoveredCell().

    reanchor вызывается после изменения уровня, но до selectCell().
    Galaxy использует его для setAnchorFromPositionLy().
    System — для setAnchorFromPosition().
*/
template<typename Grid, typename Reanchor>
bool applyCubicNavigationLevelAction(
    CubicNavigationLevelAction action,
    Grid& grid,
    Reanchor&& reanchor
)
{
    bool levelChanged = false;

    if (action ==
        CubicNavigationLevelAction::Refine)
    {
        levelChanged =
            grid.refineAroundAnchor();
    }
    else if (
        action ==
        CubicNavigationLevelAction::Coarsen)
    {
        levelChanged =
            grid.coarsenAroundAnchor();
    }

    if (!levelChanged)
        return false;

    std::forward<Reanchor>(
        reanchor
    )();

    grid.selectCell(
        grid.anchorCell()
    );

    grid.clearHoveredCell();

    return true;
}






/*
    Смена уровня относительно пространственной точки обзора.

    position не является выбранным игровым объектом. Это навигационный
    курсор карты: точка пространства, находящаяся в центре экрана.

    Якорь синхронизируется дважды:
    - до смены уровня, чтобы родителем стал куб под курсором;
    - после смены уровня, чтобы выбрать дочерний/родительский куб,
      содержащий ту же самую точку.

    setAnchor должен иметь сигнатуру:

        setAnchor(grid, position)

    Такой адаптер позволяет использовать общий алгоритм и с System
    (setAnchorFromPosition), и с Galaxy
    (setAnchorFromPositionLy).
*/
template<
    typename Grid,
    typename Position,
    typename SetAnchor
>
bool applyCubicNavigationLevelActionAtPosition(
    CubicNavigationLevelAction action,
    Grid& grid,
    const Position& position,
    SetAnchor&& setAnchor,
    bool selectAnchorAfterChange = false
)
{
    if (action != CubicNavigationLevelAction::Refine &&
        action != CubicNavigationLevelAction::Coarsen)
    {
        return false;
    }

    /*
        Старый anchor мог остаться возле ранее выбранной звезды,
        планеты или куба. Перед изменением уровня переносим его
        в точку текущего обзора.
    */
    setAnchor(
        grid,
        position
    );

    bool levelChanged = false;

    if (action == CubicNavigationLevelAction::Refine)
    {
        levelChanged =
            grid.refineAroundAnchor();
    }
    else
    {
        levelChanged =
            grid.coarsenAroundAnchor();
    }

    if (!levelChanged)
        return false;

    /*
        После изменения размера ячейки вычисляем индекс заново.
        Координата position при этом не меняется.
    */
    setAnchor(
        grid,
        position
    );

    if (selectAnchorAfterChange)
    {
        grid.selectCell(
            grid.anchorCell()
        );
    }

    grid.clearHoveredCell();

    return true;
}


inline float cubicNavigationViewportReferencePx(
    int viewportWidth,
    int viewportHeight
)
{
    return static_cast<float>(
        std::max(
            1,
            std::min(
                viewportWidth,
                viewportHeight
            )
        )
    );
}


/*
    Экранный диаметр перспективного куба.

    Важно: размер вычисляется аналитически, а не через восемь
    спроецированных углов. Когда куб становится больше viewport,
    все его углы могут оказаться за экраном, хотя сам куб занимает
    весь кадр. Проверка visible для углов в таком случае ошибочно
    давала нулевой размер и блокировала смену уровня.
*/
inline float cubicNavigationPerspectiveProjectedDiameterPx(
    float edgeWorld,
    float centerDepthWorld,
    float verticalFovRad,
    int viewportHeight,
    const CubicNavigationInteractionPolicy& policy = {}
)
{
    if (edgeWorld <= 0.0f ||
        centerDepthWorld <= 0.000001f ||
        viewportHeight <= 0)
    {
        return 0.0f;
    }

    const float tanHalfFov =
        std::max(
            std::tan(verticalFovRad * 0.5f),
            0.000001f
        );

    return
        edgeWorld *
        policy.projectedCubeExtentFactor *
        static_cast<float>(viewportHeight) /
        (
            2.0f *
            centerDepthWorld *
            tanHalfFov
        );
}


/*
    Экранный диаметр куба для ортографической камеры.
    orthographicHalfHeight соответствует половине видимой
    высоты мира.
*/
inline float cubicNavigationOrthographicProjectedDiameterPx(
    float edgeWorld,
    float orthographicHalfHeight,
    int viewportHeight,
    const CubicNavigationInteractionPolicy& policy = {}
)
{
    if (edgeWorld <= 0.0f ||
        orthographicHalfHeight <= 0.000001f ||
        viewportHeight <= 0)
    {
        return 0.0f;
    }

    return
        edgeWorld *
        policy.projectedCubeExtentFactor *
        static_cast<float>(viewportHeight) /
        (
            2.0f *
            orthographicHalfHeight
        );
}


/*
    Колесо меняет уровень без camera flight.

    На последнем уровне конкретная карта может разрешить
    переход в дочерний режим. Для Galaxy это System.
    Для System сейчас дочерняя карта не открывается.
*/
inline CubicNavigationLevelAction
cubicNavigationWheelAction(
    float wheelDelta,
    float currentCellDiameterPx,
    int viewportWidth,
    int viewportHeight,
    bool canRefine,
    bool canCoarsen,
    bool canEnterChildMap,
    const CubicNavigationInteractionPolicy& policy = {}
)
{
    if (wheelDelta == 0.0f)
        return CubicNavigationLevelAction::None;

    const float viewportReferencePx =
        cubicNavigationViewportReferencePx(
            viewportWidth,
            viewportHeight
        );

    if (wheelDelta > 0.0f &&
        currentCellDiameterPx >=
            viewportReferencePx *
            policy.refineViewportFraction)
    {
        if (canRefine)
            return CubicNavigationLevelAction::Refine;

        if (canEnterChildMap)
            return CubicNavigationLevelAction::EnterChildMap;

        return CubicNavigationLevelAction::None;
    }

    if (wheelDelta < 0.0f &&
        currentCellDiameterPx <=
            viewportReferencePx *
            policy.coarsenViewportFraction)
    {
        return canCoarsen
            ? CubicNavigationLevelAction::Coarsen
            : CubicNavigationLevelAction::None;
    }

    return CubicNavigationLevelAction::None;
}


/*
    Двойной клик является явной командой пользователя.

    Если внутренний уровень существует — уточняем сетку.
    Если уровни закончились — карта может открыть
    дочерний режим, например Galaxy -> System.
*/
inline CubicNavigationLevelAction
cubicNavigationDoubleClickAction(
    bool canRefine,
    bool canEnterChildMap
)
{
    if (canRefine)
        return CubicNavigationLevelAction::Refine;

    if (canEnterChildMap)
        return CubicNavigationLevelAction::EnterChildMap;

    return CubicNavigationLevelAction::None;
}


/*
    Расстояние перспективной камеры, при котором куб
    занимает parentFitViewportFraction меньшей стороны окна.

    edgeWorld — длина грани куба в единицах рендера.
    verticalFovRad — вертикальный FOV камеры в радианах.
*/
inline float cubicNavigationPerspectiveFitDistance(
    float edgeWorld,
    float verticalFovRad,
    int viewportWidth,
    int viewportHeight,
    const CubicNavigationInteractionPolicy& policy = {}
)
{
    const float safeEdge =
        std::max(edgeWorld, 0.000001f);

    const float viewportReferencePx =
        cubicNavigationViewportReferencePx(
            viewportWidth,
            viewportHeight
        );

    const float safeViewportHeight =
        static_cast<float>(
            std::max(viewportHeight, 1)
        );

    const float safeFillFraction =
        std::clamp(
            policy.parentFitViewportFraction,
            0.10f,
            0.98f
        );

    const float tanHalfFov =
        std::max(
            std::tan(verticalFovRad * 0.5f),
            0.000001f
        );

    return
        safeEdge *
        policy.projectedCubeExtentFactor *
        safeViewportHeight /
        (
            2.0f *
            viewportReferencePx *
            safeFillFraction *
            tanHalfFov
        );
}


/*
    Аналогичный расчёт для ортографической камеры System.

    Результат — orthographic half-height.
    Пока Galaxy его не использует, но System сможет
    подключиться к той же политике без копирования формул.
*/
inline float cubicNavigationOrthographicFitHalfHeight(
    float edgeWorld,
    int viewportWidth,
    int viewportHeight,
    const CubicNavigationInteractionPolicy& policy = {}
)
{
    const float safeEdge =
        std::max(edgeWorld, 0.000001f);

    const float viewportReferencePx =
        cubicNavigationViewportReferencePx(
            viewportWidth,
            viewportHeight
        );

    const float safeViewportHeight =
        static_cast<float>(
            std::max(viewportHeight, 1)
        );

    const float safeFillFraction =
        std::clamp(
            policy.parentFitViewportFraction,
            0.10f,
            0.98f
        );

    return
        safeEdge *
        policy.projectedCubeExtentFactor *
        safeViewportHeight /
        (
            2.0f *
            viewportReferencePx *
            safeFillFraction
        );
}

} // namespace game::navigation
