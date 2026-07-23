#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace game::navigation
{

/*
    Унифицированное состояние камеры кубической навигации.

    target:
        физическая точка, на которую смотрит камера.

    scale:
        масштаб просмотра.

        Для Galaxy это camera.distance.
        Для System это orthographic half-height, который сейчас
        тоже хранится в camera.distance.
*/
struct CubicNavigationCameraPose
{
    glm::dvec3 target {0.0};
    double scale = 1.0;
};


/*
    Общий плавный перелёт камеры.

    Компонент ничего не знает:
    - о Galaxy/System;
    - о единицах ly/AU;
    - о перспективной или ортографической проекции;
    - о GLFW и рендере.

    Он интерполирует только target и scale.
*/
class CubicNavigationCameraFlight
{
public:
    bool active() const
    {
        return m_active;
    }

    void reset()
    {
        m_active = false;

        m_start = {};
        m_destination = {};

        m_startTimeSeconds = 0.0;
        m_durationSeconds = 0.01;
    }

    /*
        Запускает движение от currentPose к destinationPose.

        currentPose передаётся не const, потому что при практически
        нулевом перемещении destination применяется немедленно.
    */
    void begin(
        CubicNavigationCameraPose& currentPose,
        const CubicNavigationCameraPose& destinationPose,
        double nowSeconds,
        double durationSeconds
    )
    {
        const double targetDistance =
            glm::length(
                destinationPose.target -
                currentPose.target
            );

        const double scaleDistance =
            std::abs(
                destinationPose.scale -
                currentPose.scale
            );

        /*
            Камера уже находится в нужной точке.
            Не запускаем формальную анимацию на месте.
        */
        if (targetDistance < 0.0001 &&
            scaleDistance < 0.0001)
        {
            currentPose = destinationPose;

            m_active = false;
            return;
        }

        m_start =
            currentPose;

        m_destination =
            destinationPose;

        /*
            Логарифмическая интерполяция требует
            строго положительных значений.
        */
        m_start.scale =
            std::max(
                m_start.scale,
                0.0000001
            );

        m_destination.scale =
            std::max(
                m_destination.scale,
                0.0000001
            );

        m_startTimeSeconds =
            nowSeconds;

        m_durationSeconds =
            std::max(
                0.01,
                durationSeconds
            );

        m_active =
            true;
    }

    /*
        Обновляет текущую позицию камеры.

        Возвращает false, если активного перелёта не было.
        Возвращает true, если inOutPose был обновлён.
    */
    bool update(
        double nowSeconds,
        CubicNavigationCameraPose& inOutPose
    )
    {
        if (!m_active)
            return false;

        const double elapsedSeconds =
            std::max(
                0.0,
                nowSeconds -
                    m_startTimeSeconds
            );

        const double linearProgress =
            std::clamp(
                elapsedSeconds /
                    m_durationSeconds,
                0.0,
                1.0
            );

        const double progress =
            smootherStep(
                linearProgress
            );

        inOutPose.target =
            glm::mix(
                m_start.target,
                m_destination.target,
                progress
            );

        /*
            Масштаб воспринимается как отношение величин,
            поэтому интерполируем его логарифм.
        */
        inOutPose.scale =
            std::exp(
                std::log(m_start.scale) +
                (
                    std::log(m_destination.scale) -
                    std::log(m_start.scale)
                ) *
                progress
            );

        if (linearProgress >= 1.0)
        {
            inOutPose =
                m_destination;

            m_active =
                false;
        }

        return true;
    }

    /*
        Останавливает перелёт.

        snapToDestination == false:
            камера остаётся в текущей точке.

        snapToDestination == true:
            камера сразу устанавливается в конечную точку.
    */
    bool cancel(
        bool snapToDestination,
        CubicNavigationCameraPose& inOutPose
    )
    {
        if (!m_active)
            return false;

        if (snapToDestination)
        {
            inOutPose =
                m_destination;
        }

        m_active =
            false;

        return true;
    }

private:
    static double smootherStep(
        double value
    )
    {
        return
            value *
            value *
            value *
            (
                value *
                (
                    value * 6.0 -
                    15.0
                ) +
                10.0
            );
    }

private:
    bool m_active = false;

    CubicNavigationCameraPose
        m_start;

    CubicNavigationCameraPose
        m_destination;

    double m_startTimeSeconds = 0.0;
    double m_durationSeconds = 0.01;
};

} // namespace game::navigation