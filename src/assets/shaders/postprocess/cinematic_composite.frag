#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom;

uniform vec2 uSceneTexelSize;
uniform vec2 uResolution;
uniform vec4 uGameRect;

uniform float uBloomIntensity;
uniform float uSoftening;
uniform float uSaturation;
uniform float uContrast;
uniform float uVignette;
uniform float uGrain;
uniform float uHaze;
uniform float uTime;

float luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main()
{
    vec2 uv = clamp(vUv, vec2(0.0), vec2(1.0));

    if (uv.x < uGameRect.x || uv.y < uGameRect.y ||
        uv.x > uGameRect.z || uv.y > uGameRect.w)
    {
        FragColor = texture(uScene, uv);
        return;
    }

    vec2 gameUv = (uv - uGameRect.xy) / max(
        uGameRect.zw - uGameRect.xy,
        vec2(0.0001)
    );

    vec2 minSampleUv = uGameRect.xy + uSceneTexelSize;
    vec2 maxSampleUv = uGameRect.zw - uSceneTexelSize;

    vec3 center = texture(uScene, uv).rgb;

    vec2 d = uSceneTexelSize;
    vec3 tapA = texture(uScene, clamp(uv + vec2( d.x,  d.y), minSampleUv, maxSampleUv)).rgb;
    vec3 tapB = texture(uScene, clamp(uv + vec2(-d.x,  d.y), minSampleUv, maxSampleUv)).rgb;
    vec3 tapC = texture(uScene, clamp(uv + vec2( d.x, -d.y), minSampleUv, maxSampleUv)).rgb;
    vec3 tapD = texture(uScene, clamp(uv + vec2(-d.x, -d.y), minSampleUv, maxSampleUv)).rgb;

    vec3 softened = center * 0.40 + (tapA + tapB + tapC + tapD) * 0.15;

    float edgeDelta = 0.0;
    edgeDelta = max(edgeDelta, abs(luminance(center) - luminance(tapA)));
    edgeDelta = max(edgeDelta, abs(luminance(center) - luminance(tapB)));
    edgeDelta = max(edgeDelta, abs(luminance(center) - luminance(tapC)));
    edgeDelta = max(edgeDelta, abs(luminance(center) - luminance(tapD)));

    float smoothArea = 1.0 - smoothstep(0.025, 0.15, edgeDelta);
    vec3 color = mix(center, softened, uSoftening * smoothArea);

    vec3 bloom = texture(uBloom, uv).rgb;
    color += bloom * uBloomIntensity * vec3(1.04, 0.98, 0.92);

    float luma = luminance(color);

    vec2 hazeP = (gameUv - vec2(0.50, 0.55)) * vec2(0.90, 1.30);
    float broadHaze = exp(-dot(hazeP, hazeP) * 2.5);
    float shadowMask = 1.0 - smoothstep(0.10, 0.52, luma);

    color += vec3(0.012, 0.018, 0.028) * broadHaze * uHaze;
    color += vec3(-0.003, 0.002, 0.010) * shadowMask * 0.80;

    float highlightMask = smoothstep(0.45, 0.95, luma);
    color += vec3(0.010, 0.004, -0.004) * highlightMask * 0.70;

    luma = luminance(color);
    color = mix(vec3(luma), color, uSaturation);
    color = (color - 0.5) * uContrast + 0.5;

    vec3 clampedColor = clamp(color, 0.0, 1.0);
    vec3 sCurve = clampedColor * clampedColor * (3.0 - 2.0 * clampedColor);
    color = mix(color, sCurve, 0.12);

    vec3 overbright = max(color - 1.0, vec3(0.0));
    color = min(color, vec3(1.0)) + overbright / (1.0 + overbright);

    vec2 p = gameUv * 2.0 - 1.0;
    float vignetteDistance = dot(p * vec2(0.78, 1.0), p * vec2(0.78, 1.0));
    float vignetteMask = smoothstep(0.35, 1.25, vignetteDistance);
    color *= 1.0 - uVignette * vignetteMask;

    float grainNoise = hash12(
        floor(uv * uResolution) + vec2(fract(uTime * 23.71) * 97.0)
    ) - 0.5;

    float finalLuma = luminance(color);
    float grainMask = 0.35 + 0.65 * (1.0 - smoothstep(0.20, 0.80, finalLuma));
    color += grainNoise * uGrain * grainMask;

    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}