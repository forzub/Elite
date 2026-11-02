#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uDepth;   // resolved scene depth (Anime, no-MSAA targets)
uniform sampler2DMS uDepthMs; // multisample scene depth, sample 0 (Anime, MSAA)
uniform int uDepthIsMs;
// Main flight camera (Anime deep-space sky): view -> world rotation,
// tan(fovY / 2), aspect. uCameraValid = 0 on maps / without a flight view.
uniform mat3 uViewToWorld;
uniform float uTanHalfFovY;
uniform float uAspect;
uniform int uCameraValid;
uniform int uStyle;         // 0 Elite, 1 Modern, 2 Anime

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

// Anime silhouette ink on ships / stations / hub modules only. Meshes use
// perspective depth (near 1 m: 1 - depth ~ 1 / distance), the celestial
// pass log depth (1 - depth >~ 0.1): the two are not comparable, so a pixel
// is a "mesh" pixel when 1 - depth < 0.06 (farther than ~15 m). Planets,
// the Sun and space are the painted backdrop and never carry ink. A contour
// is mesh vs backdrop, or a relative distance jump between two mesh pixels;
// only the NEARER side is inked (the line sits on the object), and a thing
// only a couple of pixels wide stays a colour dot, never an ink blob.
float sceneDepth(vec2 uv)
{
    if (uDepthIsMs != 0)
        return texelFetch(uDepthMs, clamp(ivec2(uv * uResolution), ivec2(0), ivec2(uResolution) - 1), 0).r;
    return texture(uDepth, uv).r;
}

float animeInk(vec2 uv, vec2 texel, out vec3 nearColor)
{
    nearColor = texture(uScene, uv).rgb;
    float oc = 1.0 - sceneDepth(uv);
    // Mesh pixels between ~15 m and ~30 km (1 - depth ~ 1 / distance):
    // ships, stations, docks. The near-terrain depth pass (planet surface
    // seen from orbit, hundreds of km) is backdrop - no contour on hills.
    if (oc >= 0.06 || oc <= 3.3e-5)
        return 0.0;
    // Four neighbours (cross); distance ratio instead of a log per tap.
    vec4 on = vec4(1.0) - vec4(
        sceneDepth(uv + vec2(texel.x, 0.0)),
        sceneDepth(uv - vec2(texel.x, 0.0)),
        sceneDepth(uv + vec2(0.0, texel.y)),
        sceneDepth(uv - vec2(0.0, texel.y)));
    vec4 backdrop = vec4(greaterThanEqual(on, vec4(0.06))) + vec4(lessThanEqual(on, vec4(3.3e-5)));
    backdrop = min(backdrop, vec4(1.0));
    // Farther neighbour -> smaller (1 - depth): ratio > 1.1 .. 1.25 = contour.
    // Perspective depth is affine in screen space over a plane, so a surface
    // seen at a grazing angle (near terrain, a long hull plate) has a ~zero
    // second difference: only a real fold / silhouette passes this test.
    vec2 lap = abs(vec2(on.x + on.y, on.z + on.w) - 2.0 * oc) / oc;
    vec2 fold = smoothstep(vec2(0.08), vec2(0.18), lap);
    vec4 jump = smoothstep(vec4(1.10), vec4(1.25), vec4(oc) / max(on, vec4(1.0e-7))) * fold.xxyy;
    vec4 edge4 = max(jump, backdrop);
    float edge = max(max(edge4.x, edge4.y), max(edge4.z, edge4.w));
    float open = dot(backdrop, vec4(1.0));
    // A thing about two pixels wide (space on 3-4 sides) stays a colour dot.
    return edge * (1.0 - smoothstep(2.5, 3.5, open));
}

// Anime deep space: the painted world-fixed sky baked once by the Renderer
// (Renderer::bakeAnimeSkyCubemap) - one fetch per sky pixel.
uniform samplerCube uAnimeSky;

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
    bool anime = uStyle == 2 || uStyle == 3; // 3: DEV A/B, Anime without ink
    // Anime keeps flat colour masses crisp: no softening, half the bloom.
    vec3 color = anime ? center : mix(center, softened, uSoftening * smoothArea);

    vec3 bloom = texture(uBloom, uv).rgb;
    color += bloom * uBloomIntensity * (anime ? 0.5 : 1.0) * vec3(1.04, 0.98, 0.92);

    if (anime)
    {
        // Painted deep space under the stars / Milky Way / planet glow:
        // only where nothing was drawn (empty depth), added beneath what is
        // already there (bright stars and glow stay on top).
        float dc = sceneDepth(uv);
        if (dc >= 0.999999 && uCameraValid != 0)
        {
            vec2 ndc = gameUv * 2.0 - 1.0;
            vec3 viewDir = normalize(vec3(ndc.x * uTanHalfFovY * uAspect, ndc.y * uTanHalfFovY, -1.0));
            vec3 sky = texture(uAnimeSky, normalize(uViewToWorld * viewDir)).rgb;
            float lumaBg = luminance(color);
            color += sky * (1.0 - smoothstep(0.10, 0.45, lumaBg));
        }
        vec3 nearColor;
        float ink = uStyle == 2 ? animeInk(uv, uSceneTexelSize, nearColor) : 0.0;
        // Coloured ink: the object's own shadow tone, deep and slightly cool.
        vec3 inkColor = nearColor * vec3(0.22, 0.22, 0.30) + vec3(0.015, 0.015, 0.030);
        color = mix(color, inkColor, ink * 0.88);
    }

    float luma = luminance(color);

    vec2 hazeP = (gameUv - vec2(0.50, 0.55)) * vec2(0.90, 1.30);
    float broadHaze = exp(-dot(hazeP, hazeP) * 2.5);
    float shadowMask = 1.0 - smoothstep(0.10, 0.52, luma);

    color += vec3(0.012, 0.018, 0.028) * broadHaze * uHaze;
    // Anime grade: shadows lean deep blue-violet, highlights warm; a gentle
    // palette, never "saturation x 2".
    color += (anime ? vec3(0.002, 0.006, 0.020) : vec3(-0.003, 0.002, 0.010)) * shadowMask * 0.80;

    float highlightMask = smoothstep(0.45, 0.95, luma);
    color += (anime ? vec3(0.024, 0.012, -0.006) : vec3(0.010, 0.004, -0.004)) * highlightMask * 0.70;

    luma = luminance(color);
    color = mix(vec3(luma), color, anime ? max(uSaturation, 0.96) : uSaturation);
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
    // Anime: no film grain (flat cels).
    color += grainNoise * (anime ? 0.0 : uGrain) * grainMask;

    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
