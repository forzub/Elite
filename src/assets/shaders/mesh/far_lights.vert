#version 330 core

// FAR LIGHTS (src/render/geometry/FarLightsRenderer): a light source of a model asset as a point with a physical
// brightness - power / d^2 - visible far beyond the LOD that drew its geometry (author 2026-11-23: lights are seen from
// 30 km). Depth-tested against the scene; pulled towards the camera by the depth buffer's resolution at its distance so
// a light on a far facade is not lost in its own wall's depth quantum.
layout(location = 0) in vec3 aPos;        // asset space
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aPowerArea;  // power (m2 x brightness), glowing area (m2)
layout(location = 3) in float aKind;

uniform mat4 uModelView;   // asset -> camera-relative view space
uniform mat4 uProj;
uniform float uFocalPx;    // projection focal length in pixels
uniform float uNear;
uniform float uTime;
uniform float uGain;       // the brightness scale (ELITE_FAR_LIGHTS_GAIN)

out vec3 vColor;
out float vAlpha;

void main()
{
    vec4 v = uModelView * vec4(aPos, 1.0);
    float d = max(1.0, length(v.xyz));
    // the apparent brightness: a lit window cell (~50 m2) is a faint point at ~10 km, a beacon at ~30 km
    float flux = uGain * aPowerArea.x / (d * d);
    float b = flux / 2.0e-5;                              // 1 = full point brightness
    // its own geometry is big enough to show it: the point steps aside (2..5 px of glowing size)
    float srcPx = sqrt(max(aPowerArea.y, 0.01)) / d * uFocalPx;
    float stepAside = 1.0 - smoothstep(2.0, 5.0, srcPx);
    // beacons blink (1 s period, 35 % on), seeded by their place
    float blink = 1.0;
    if (aKind > 0.5 && aKind < 1.5)
        blink = fract(uTime + fract(dot(aPos, vec3(0.0131, 0.0173, 0.0197))) * 7.0) < 0.35 ? 1.0 : 0.15;
    float a = clamp(sqrt(b), 0.0, 1.0) * stepAside * blink;
    if (b < 0.01 || a < 0.02)
    {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);   // off screen
        gl_PointSize = 0.0;
        vAlpha = 0.0;
        vColor = vec3(0.0);
        return;
    }
    // the depth quantum (24 bit) at d, twice, + 1 m: towards the camera
    float pull = 2.0 * d * d / (uNear * 16777216.0) + 1.0;
    v.xyz -= normalize(v.xyz) * min(pull, 0.5 * d);
    gl_Position = uProj * v;
    gl_PointSize = clamp(1.5 + 2.5 * sqrt(b), 1.5, 5.0);
    vColor = aColor;
    vAlpha = a;
}
