#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUv;

/*
    Преобразует локальную сферу:

        X = направление нулевой долготы
        Y = северный полюс
        Z = восточное направление

    в систему DetailCamera:

        X = вправо по экрану
        Y = вверх по экрану
        Z = к наблюдателю
*/
uniform mat3 uBodyToCamera;

uniform vec2 uViewportSize;
uniform vec2 uPlanetCenterPx;
uniform float uPlanetRadiusPx;

uniform float uLongitudeUvOffset;
uniform bool uFlipV;

out vec2 vUv;
out vec3 vCameraNormal;
out float vNormalizedLatitude;

void main()
{
    vec3 cameraPosition =
        uBodyToCamera *
        aPosition;

    /*
        SystemMapRenderer хранит экранные координаты
        от левого верхнего угла.
    */
    vec2 screenPx =
        uPlanetCenterPx +
        vec2(
            cameraPosition.x,
            -cameraPosition.y
        ) *
        uPlanetRadiusPx;

    /*
        Переводим координаты от левого верхнего угла
        в OpenGL NDC.
    */
    vec2 ndc;

    ndc.x =
        screenPx.x /
        max(
            1.0,
            uViewportSize.x
        ) *
        2.0 -
        1.0;

    ndc.y =
        1.0 -
        screenPx.y /
        max(
            1.0,
            uViewportSize.y
        ) *
        2.0;

    gl_Position =
        vec4(
            ndc,
            0.0,
            1.0
        );

    float textureV =
        uFlipV
            ? 1.0 - aUv.y
            : aUv.y;

    vUv =
        vec2(
            aUv.x +
                uLongitudeUvOffset,
            textureV
        );

    /*
        Матрица является чистым вращением,
        поэтому длина остаётся равной единице.
    */
    vCameraNormal =
        cameraPosition;

    /*
        aUv.y линейно соответствует latitude:

            0.0 = южный полюс
            0.5 = экватор
            1.0 = северный полюс
    */
    vNormalizedLatitude =
        abs(
            aUv.y *
                2.0 -
            1.0
        );
}