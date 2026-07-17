#version 330 core

layout(location = 0) in vec3 aLocalPosition;

/*
    MeshGPU::drawEdgesInstanced() binds the instance matrix
    to locations 4, 5, 6 and 7.
*/
layout(location = 4) in mat4 aLocalToHub;

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

uniform vec4 uColor;

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
    vec3 hubPosition =
        (
            aLocalToHub *
            vec4(
                aLocalPosition,
                1.0
            )
        ).xyz;

    vec2 screenPx =
        hubPositionToScreenPx(
            hubPosition
        );

    gl_Position =
        vec4(
            screenPxToNdc(
                screenPx
            ),
            0.0,
            1.0
        );

    vColor =
        uColor;
}
