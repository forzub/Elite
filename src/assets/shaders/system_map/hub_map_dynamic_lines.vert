#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;
layout(location = 2) in float aCoordinateSpace;

uniform vec2 uViewportSizePx;
uniform vec2 uScreenOriginPx;
uniform float uPixelsPerMeter;

/*
    x = cos(yaw)
    y = sin(yaw)
    z = cos(pitch)
    w = sin(pitch)
*/
uniform vec4 uCameraTrig;

out vec4 vColor;

vec2 hubPositionToScreenPx(
    vec3 hubPosition
)
{
    float cosYaw =
        uCameraTrig.x;

    float sinYaw =
        uCameraTrig.y;

    float cosPitch =
        uCameraTrig.z;

    float sinPitch =
        uCameraTrig.w;

    float rotatedX =
        hubPosition.x *
            cosYaw -
        hubPosition.z *
            sinYaw;

    float rotatedZ =
        hubPosition.x *
            sinYaw +
        hubPosition.z *
            cosYaw;

    float projectedY =
        hubPosition.y *
            cosPitch -
        rotatedZ *
            sinPitch;

    return
        uScreenOriginPx +
        vec2(
            rotatedX,
            -projectedY
        ) *
        uPixelsPerMeter;
}

// the depth along the view (author 2026-11-21: the city blocks are solid - edges and lines behind them hide):
// camera-space z grows toward the viewer; NDC z = -z / 200 km (nearer = smaller)
float hubPositionToNdcDepth(
    vec3 hubPosition
)
{
    float rotatedZ = hubPosition.x * uCameraTrig.y + hubPosition.z * uCameraTrig.x;
    float towardCamera = hubPosition.y * uCameraTrig.w + rotatedZ * uCameraTrig.z;
    return clamp(-towardCamera / 200000.0, -0.999, 0.999);
}

vec2 screenPxToNdc(
    vec2 screenPx
)
{
    return
        vec2(
            screenPx.x /
                max(
                    1.0,
                    uViewportSizePx.x
                ) *
                2.0 -
                1.0,

            1.0 -
            screenPx.y /
                max(
                    1.0,
                    uViewportSizePx.y
                ) *
                2.0
        );
}

void main()
{
    vec2 screenPx =
        aCoordinateSpace < 0.5
            ? hubPositionToScreenPx(
                aPosition
            )
            : aPosition.xy;

    gl_Position =
        vec4(
            screenPxToNdc(
                screenPx
            ),
            aCoordinateSpace < 0.5
                ? hubPositionToNdcDepth(
                    aPosition
                )
                : -0.999,
            1.0
        );

    vColor =
        aColor;
}
