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
uniform float uLongitudeOffset;

uniform vec3 uLightDirection;

uniform float uAmbientStrength;
uniform float uDiffuseStrength;
uniform float uNormalStrength;

uniform float uHorizonDarkening;
uniform float uSurfaceContrast;
uniform float uSurfaceSaturation;

vec2 sphericalUv(
    vec3 sphereNormal
)
{
    float longitude =
        atan(
            sphereNormal.x,
            sphereNormal.z
        );

    float latitude =
        asin(
            clamp(
                sphereNormal.y,
                -1.0,
                1.0
            )
        );

    return
        vec2(
            fract(
                0.5 +
                longitude /
                    (2.0 * PI) +
                uLongitudeOffset
            ),
            clamp(
                0.5 -
                latitude /
                    PI,
                0.001,
                0.999
            )
        );
}

vec3 fallbackNormalFromAlbedo(
    vec2 uv
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
            max(textureSizePx.x, 1),
            max(textureSizePx.y, 1)
        );

    float leftHeight =
        dot(
            texture(
                uAlbedoTexture,
                vec2(
                    fract(uv.x - texel.x),
                    uv.y
                )
            ).rgb,
            vec3(
                0.299,
                0.587,
                0.114
            )
        );

    float rightHeight =
        dot(
            texture(
                uAlbedoTexture,
                vec2(
                    fract(uv.x + texel.x),
                    uv.y
                )
            ).rgb,
            vec3(
                0.299,
                0.587,
                0.114
            )
        );

    float downHeight =
        dot(
            texture(
                uAlbedoTexture,
                vec2(
                    uv.x,
                    clamp(
                        uv.y - texel.y,
                        0.001,
                        0.999
                    )
                )
            ).rgb,
            vec3(
                0.299,
                0.587,
                0.114
            )
        );

    float upHeight =
        dot(
            texture(
                uAlbedoTexture,
                vec2(
                    uv.x,
                    clamp(
                        uv.y + texel.y,
                        0.001,
                        0.999
                    )
                )
            ).rgb,
            vec3(
                0.299,
                0.587,
                0.114
            )
        );

    return
        normalize(
            vec3(
                -(rightHeight - leftHeight) *
                    uNormalStrength,
                -(upHeight - downHeight) *
                    uNormalStrength,
                1.0
            )
        );
}

vec3 tangentNormalAt(
    vec2 uv
)
{
    if (uHasNormalTexture)
    {
        vec3 encoded =
            texture(
                uNormalTexture,
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
            uv
        );
}

void main()
{
    /*
        uPlanetCenterPx приходит из старого map renderer:
        Y направлен вниз.

        gl_FragCoord:
        Y направлен вверх.
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

    vec3 sphereNormal =
        normalize(
            vec3(
                local.x,
                local.y,
                frontZ
            )
        );

    vec2 uv =
        sphericalUv(
            sphereNormal
        );

    vec3 albedo =
        texture(
            uAlbedoTexture,
            uv
        ).rgb;

    vec3 tangentNormal =
        tangentNormalAt(
            uv
        );

    /*
        Tangent идёт по долготе.
    */
    vec3 tangent =
        normalize(
            vec3(
                sphereNormal.z,
                0.0,
                -sphereNormal.x
            )
        );

    /*
        Возле полюсов предыдущий tangent может вырождаться.
    */
    if (length(tangent) < 0.001)
    {
        tangent =
            vec3(
                1.0,
                0.0,
                0.0
            );
    }

    vec3 bitangent =
        normalize(
            cross(
                sphereNormal,
                tangent
            )
        );

    mat3 tangentToWorld =
        mat3(
            tangent,
            bitangent,
            sphereNormal
        );

    vec3 perturbedNormal =
        normalize(
            tangentToWorld *
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
        У горизонта поверхность должна быть темнее,
        потому что атмосфера будет лежать поверх отдельным pass.
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
            vec3(luminance),
            color,
            clamp(
                uSurfaceSaturation,
                0.0,
                1.5
            )
        );

    color =
        (
            color -
            vec3(0.5)
        ) *
            uSurfaceContrast +
        vec3(0.5);

    outColor =
        vec4(
            clamp(
                color,
                vec3(0.0),
                vec3(1.0)
            ),
            1.0
        );
}