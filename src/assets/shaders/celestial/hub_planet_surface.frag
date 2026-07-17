#version 330 core

in vec2 vUv;

out vec4 outColor;

const float PI =
    3.14159265358979323846;

uniform sampler2D uAlbedoTexture;
uniform sampler2D uNormalTexture;

uniform bool uHasNormalTexture;

uniform vec2 uViewportOriginPx;
uniform vec2 uViewportSize;
uniform vec2 uPlanetCenterPx;

uniform float uPlanetRadiusPx;

/*
    Экранная система cinematic globe:

        X = вправо;
        Y = вверх;
        Z = к наблюдателю.

    uCameraToPlanetBody переводит её в body-fixed систему:

        X = texture longitude 0;
        Y = северный полюс;
        Z = восток, longitude +90°.
*/
uniform mat3 uCameraToPlanetBody;

uniform vec3 uLightDirection;

uniform float uAmbientStrength;
uniform float uDiffuseStrength;
uniform float uNormalStrength;

uniform float uHorizonDarkening;
uniform float uSurfaceContrast;
uniform float uSurfaceSaturation;

uniform vec3 uAerialPerspectiveColor;
uniform float uAerialPerspectiveStrength;
uniform float uAerialPerspectiveStart;
uniform float uAerialPerspectiveEnd;
uniform float uAerialPerspectiveBlurLod;

vec2 sphericalUvFromBodyNormal(
    vec3 bodyNormal
)
{
    bodyNormal =
        normalize(
            bodyNormal
        );

    float longitude =
        atan(
            bodyNormal.z,
            bodyNormal.x
        );

    float latitude =
        asin(
            clamp(
                bodyNormal.y,
                -1.0,
                1.0
            )
        );

    return
        vec2(
            fract(
                0.5 +
                longitude /
                    (
                        2.0 *
                        PI
                    )
            ),

            clamp(
                0.5 +
                latitude /
                    PI,
                0.001,
                0.999
            )
        );
}





vec2 wrappedEquirectangularUv(
    vec2 uv
)
{
    return
        vec2(
            fract(
                uv.x
            ),
            clamp(
                uv.y,
                0.001,
                0.999
            )
        );
}


void seamSafeEquirectangularGradients(
    vec2 uv,
    out vec2 gradientX,
    out vec2 gradientY
)
{
    gradientX =
        dFdx(
            uv
        );

    gradientY =
        dFdy(
            uv
        );

    /*
        Longitude is periodic. Across the 1 -> 0 meridian,
        the raw derivative is almost +/-1, although the real
        wrapped distance is only a few texels.

        Without this correction the sampler selects a very
        coarse mip level along a pole-to-pole line.
    */
    if (abs(gradientX.x) > 0.5)
    {
        gradientX.x -=
            sign(
                gradientX.x
            );
    }

    if (abs(gradientY.x) > 0.5)
    {
        gradientY.x -=
            sign(
                gradientY.x
            );
    }
}


vec4 sampleAlbedoWrapped(
    vec2 uv
)
{
    vec2 gradientX;
    vec2 gradientY;

    seamSafeEquirectangularGradients(
        uv,
        gradientX,
        gradientY
    );

    return
        textureGrad(
            uAlbedoTexture,
            wrappedEquirectangularUv(
                uv
            ),
            gradientX,
            gradientY
        );
}


vec4 sampleNormalWrapped(
    vec2 uv
)
{
    vec2 gradientX;
    vec2 gradientY;

    seamSafeEquirectangularGradients(
        uv,
        gradientX,
        gradientY
    );

    return
        textureGrad(
            uNormalTexture,
            wrappedEquirectangularUv(
                uv
            ),
            gradientX,
            gradientY
        );
}



vec3 fallbackNormalFromAlbedo(
    vec2 uv,
    vec3 centerAlbedo
)
{
    ivec2 textureSizePx =
        textureSize(
            uAlbedoTexture,
            0
        );

    vec2 texel =
        1.0 /
        vec2(
            max(
                textureSizePx.x,
                1
            ),
            max(
                textureSizePx.y,
                1
            )
        );

    const vec3 luminanceWeights =
        vec3(
            0.299,
            0.587,
            0.114
        );

    float centerHeight =
        dot(
            centerAlbedo,
            luminanceWeights
        );

    /*
        Две дополнительные выборки вместо четырёх.

        Для художественного псевдорельефа forward differences
        достаточно.
    */
    float rightHeight =
        dot(
            sampleAlbedoWrapped(
                vec2(
                    uv.x +
                        texel.x,
                    uv.y
                )
            ).rgb,
            luminanceWeights
        );

    float upHeight =
        dot(
            sampleAlbedoWrapped(
                vec2(
                    uv.x,
                    uv.y +
                        texel.y
                )
            ).rgb,
            luminanceWeights
        );

    return
        normalize(
            vec3(
                -(
                    rightHeight -
                    centerHeight
                ) *
                    uNormalStrength,

                -(
                    upHeight -
                    centerHeight
                ) *
                    uNormalStrength,

                1.0
            )
        );
}





vec3 tangentNormalAt(
    vec2 uv,
    vec3 centerAlbedo
)
{
    if (uHasNormalTexture)
    {
        vec3 encoded =
            sampleNormalWrapped(
                uv
            ).rgb;

        vec3 tangentNormal =
            encoded *
                2.0 -
            1.0;

        tangentNormal.xy *=
            uNormalStrength;

        return
            normalize(
                tangentNormal
            );
    }

    return
        fallbackNormalFromAlbedo(
            uv,
            centerAlbedo
        );
}






void main()
{
    /*
        uPlanetCenterPx использует координаты карты:
        начало слева сверху.

        gl_FragCoord:
        начало слева снизу.
    */
    vec2 planetCenterWindowPx =
        vec2(
            uViewportOriginPx.x +
                uPlanetCenterPx.x,

            uViewportOriginPx.y +
                (
                    uViewportSize.y -
                    uPlanetCenterPx.y
                )
        );

    vec2 local =
        (
            gl_FragCoord.xy -
            planetCenterWindowPx
        ) /
        max(
            1.0,
            uPlanetRadiusPx
        );

    float radiusSquared =
        dot(
            local,
            local
        );

    if (radiusSquared > 1.0)
        discard;

    float frontZ =
        sqrt(
            max(
                0.0,
                1.0 -
                    radiusSquared
            )
        );

    /*
        Геометрическая нормаль cinematic globe
        в экранной системе.
    */
    vec3 cameraSphereNormal =
        normalize(
            vec3(
                local.x,
                local.y,
                frontZ
            )
        );

    /*
        Реальная body-fixed нормаль родительской планеты.
        Именно по ней выбирается участок полной
        equirectangular texture.
    */
    vec3 bodyNormal =
        normalize(
            uCameraToPlanetBody *
            cameraSphereNormal
        );

    vec2 uv =
        sphericalUvFromBodyNormal(
            bodyNormal
        );

    /*
        Aerial perspective:
        к горизонту поверхность становится мягче и менее контрастной.
    */
    float horizonView =
        1.0 -
        clamp(
            frontZ,
            0.0,
            1.0
        );

    float aerialPerspectiveMask =
        smoothstep(
            uAerialPerspectiveStart,
            uAerialPerspectiveEnd,
            horizonView
        );


    /*
        Безопасное размытие без textureLod и без зависимости
        от наличия mipmap-уровней у albedo texture.
    */
    ivec2 albedoTextureSize =
        textureSize(
            uAlbedoTexture,
            0
        );

    vec2 albedoTexel =
        1.0 /
        vec2(
            max(
                albedoTextureSize.x,
                1
            ),
            max(
                albedoTextureSize.y,
                1
            )
        );

    vec3 sharpAlbedo =
        sampleAlbedoWrapped(
            uv
        ).rgb;

    /*
        Используем прежний uniform как радиус размытия
        в texel-ах, а не как mip LOD.
    */
    float blurRadiusTexels =
        uAerialPerspectiveBlurLod *
        pow(
            aerialPerspectiveMask,
            1.35
        );

    vec2 blurStep =
        albedoTexel *
        blurRadiusTexels;

    vec3 softenedAlbedo =
        (
            sharpAlbedo * 2.0 +

            sampleAlbedoWrapped(
                vec2(
                    uv.x +
                        blurStep.x,
                    uv.y
                )
            ).rgb +

            sampleAlbedoWrapped(
                vec2(
                    uv.x -
                        blurStep.x,
                    uv.y
                )
            ).rgb +

            sampleAlbedoWrapped(
                vec2(
                    uv.x,
                    uv.y +
                        blurStep.y
                )
            ).rgb +

            sampleAlbedoWrapped(
                vec2(
                    uv.x,
                    uv.y -
                        blurStep.y
                )
            ).rgb
        ) /
        6.0;

    vec3 albedo =
        mix(
            sharpAlbedo,
            softenedAlbedo,
            aerialPerspectiveMask
        );

    /*
        В текущей оптимизированной версии tangentNormalAt()
        обязательно принимает два аргумента.
    */
    vec3 tangentNormal =
        tangentNormalAt(
            uv,
            sharpAlbedo
        );

    /*
        Восточное направление в body-fixed системе,
        то есть направление роста longitude.
    */
    vec3 bodyTangent =
        vec3(
            -bodyNormal.z,
            0.0,
            bodyNormal.x
        );

    if (length(bodyTangent) < 0.001)
    {
        bodyTangent =
            vec3(
                0.0,
                0.0,
                1.0
            );
    }

    bodyTangent =
        normalize(
            bodyTangent
        );

    /*
        Северное касательное направление.
    */
    vec3 bodyBitangent =
        normalize(
            cross(
                bodyTangent,
                bodyNormal
            )
        );

    /*
        Для чистого вращения обратная матрица равна transpose.
    */
    mat3 planetBodyToCamera =
        transpose(
            uCameraToPlanetBody
        );

    vec3 cameraTangent =
        normalize(
            planetBodyToCamera *
            bodyTangent
        );

    vec3 cameraBitangent =
        normalize(
            planetBodyToCamera *
            bodyBitangent
        );

    mat3 tangentToCamera =
        mat3(
            cameraTangent,
            cameraBitangent,
            cameraSphereNormal
        );

    vec3 perturbedNormal =
        normalize(
            tangentToCamera *
            tangentNormal
        );

    vec3 lightDirection =
        normalize(
            uLightDirection
        );

    float diffuse =
        max(
            0.0,
            dot(
                perturbedNormal,
                lightDirection
            )
        );

    float lighting =
        uAmbientStrength +
        diffuse *
            uDiffuseStrength;

    /*
        Atmosphere рисуется отдельным pass.
        Поверхность около горизонта немного затемняется.
    */
    float horizonFactor =
        mix(
            uHorizonDarkening,
            1.0,
            pow(
                frontZ,
                0.42
            )
        );

    vec3 color =
        albedo *
        lighting *
        horizonFactor;

    float hazeStrength =
        aerialPerspectiveMask *
        uAerialPerspectiveStrength;

    float luminanceBeforeHaze =
        dot(
            color,
            vec3(
                0.2126,
                0.7152,
                0.0722
            )
        );

    /*
        Дальние области теряют насыщенность, контраст
        и частично уходят в атмосферный tint.
    */
    vec3 hazeTarget =
        mix(
            vec3(
                luminanceBeforeHaze
            ),
            uAerialPerspectiveColor,
            0.65
        );

    color =
        mix(
            color,
            hazeTarget,
            hazeStrength
        );

    float localSaturation =
        mix(
            uSurfaceSaturation,
            uSurfaceSaturation * 0.58,
            hazeStrength
        );

    float luminance =
        dot(
            color,
            vec3(
                0.2126,
                0.7152,
                0.0722
            )
        );

    color =
        mix(
            vec3(
                luminance
            ),
            color,
            clamp(
                localSaturation,
                0.0,
                1.5
            )
        );

    float localContrast =
        mix(
            uSurfaceContrast,
            1.0,
            clamp(
                hazeStrength * 0.85,
                0.0,
                1.0
            )
        );

    color =
        (
            color -
            vec3(
                0.5
            )
        ) *
            localContrast +
        vec3(
            0.5
        );

    outColor =
        vec4(
            clamp(
                color,
                vec3(
                    0.0
                ),
                vec3(
                    1.0
                )
            ),
            1.0
        );
}