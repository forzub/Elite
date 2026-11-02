#version 330 core

in vec3 vNormal;
in vec3 vColor;
in vec3 vWorldPos;
in vec3 vViewPos;

out vec4 FragColor;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

uniform vec3 hullColor;
uniform vec3 detailColor;
uniform vec3 glowColor;

uniform float fresnelPower;
uniform float rimIntensity;
uniform float edgeIntensity;
uniform float normalBlend;

uniform bool fogEnabled;
uniform float fogStart;
uniform float fogEnd;
uniform vec3 fogNearColor;
uniform vec3 fogFarColor;
uniform float fogIntensity;

uniform vec3 cameraPos;

uniform float distClose;
uniform float distMedium;
uniform float distFar;
uniform vec3 hullColorNear;
uniform vec3 hullColorMid;
uniform vec3 hullColorFar;

uniform int uSceneStyle; // 0 Elite Classic, 1 Modern, 2 Anime

// Surface effect "thrust" (semantic binding resolved on the CPU): executed
// thrust of the engine owning this part, and the world aft axis. The nozzle
// pad = faces looking aft; 0 thrust = ordinary surface.
uniform float uThrust01;
uniform vec3 uThrustAxis;
uniform vec2 uThrustPlane; // x = aft-most plane offset along the axis, y = pad depth (world)

// The pad: the aft end of the engine part (within the pad depth of its
// aft-most plane), strongest on the faces that look aft.
float nozzlePad(vec3 n, vec3 worldPos)
{
    vec3 axis = normalize(uThrustAxis);
    float behind = uThrustPlane.x - dot(axis, worldPos);
    float endZone = 1.0 - smoothstep(0.0, uThrustPlane.y, behind);
    // abs(): authored nozzle discs are not reliably wound (Cobra OBJ
    // engine caps face forward); the end zone already restricts to the aft end.
    float facingAft = smoothstep(0.2, 0.8, abs(dot(n, axis)));
    return endZone * (0.35 + 0.65 * facingAft);
}

void main()
{
    vec3 n = normalize(vNormal);
    vec3 l = normalize(lightDir);
    vec3 v = normalize(cameraPos - vWorldPos);

    float dist = length(vWorldPos - cameraPos);
    float midT = smoothstep(distClose, distMedium, dist);
    float farT = smoothstep(distMedium, distFar, dist);

    vec3 distanceHullColor =
        mix(
            mix(hullColorNear, hullColorMid, midT),
            hullColorFar,
            farT
        );

    // Мягкий свет без локального блика.
    // Это даёт градиент, а не пятно.
    float ndlRaw = dot(n, l);

    float wrap =
        clamp((ndlRaw + 0.55) / 1.55, 0.0, 1.0);

    wrap = smoothstep(0.0, 1.0, wrap);

    float skyT =
        clamp(n.y * 0.5 + 0.5, 0.0, 1.0);

    vec3 hemiAmbient =
        mix(
            ambientColor * 0.32,
            ambientColor * 0.68,
            skyT
        );

    // Очень широкий силуэтный градиент.
    // Без pow с большой степенью.
    float facing =
        clamp(dot(n, v), 0.0, 1.0);

    float rim =
        smoothstep(0.18, 1.0, 1.0 - facing) * 0.10;

    vec3 litColor =
        distanceHullColor *
        (
            hemiAmbient +
            lightColor * (wrap * 0.46)
        );

    litColor += hullColorFar * rim;

    // normalBlend оставляем, но сильно глушим,
    // чтобы не было цветных пятен от normal debug-эффекта.
    vec3 normalColor = n * 0.5 + 0.5;

    vec3 finalColor =
        mix(
            litColor,
            litColor * 0.90 + normalColor * hullColorFar * 0.08,
            normalBlend * 0.20
        );

    float thrust = clamp(uThrust01, 0.0, 1.0);
    float pad = thrust > 0.001 ? nozzlePad(n, vWorldPos) : 0.0;
    // Glow grows with thrust; at high thrust a white-hot core appears.
    float glowAmount = pad * pow(thrust, 0.8);
    vec3 hot = mix(vec3(1.0, 0.55, 0.22), vec3(1.0, 0.97, 0.9), smoothstep(0.55, 1.0, thrust));

    if (uSceneStyle == 0)
    {
        // Elite Classic: opaque black hull; the light edges draw the form.
        // A burning nozzle is a grey-to-white plate (monochrome).
        FragColor = vec4(vec3(0.012) + vec3(0.92) * glowAmount, 1.0);
        return;
    }
    if (uSceneStyle == 2)
    {
        // Anime: large colour masses under one dominant light. Base tone,
        // a cool blue-violet shadow tone (never black), a narrow warm
        // highlight; a hard, slightly anti-aliased terminator. With
        // distance the bands merge and the hull sinks into the painted
        // depth tone. The silhouette ink is the composite's depth contour.
        // Anime hull: a light steel blue-grey of its own (the Modern ramp
        // starts near black, which reads as a void in a cel frame), lightly
        // tinted by the distance ramp.
        vec3 albedo = mix(vec3(0.60, 0.64, 0.72), distanceHullColor * 1.15, 0.20);
        vec3 lightTint = lightColor / max(max(lightColor.r, max(lightColor.g, lightColor.b)), 1.0e-3);
        vec3 base = albedo * mix(vec3(1.0), lightTint, 0.35) * 0.92;
        vec3 shadowTone = albedo * vec3(0.40, 0.44, 0.60) + ambientColor * 0.04;
        vec3 highlight = mix(base, vec3(1.0, 0.94, 0.82), 0.38);
        float lit = smoothstep(-0.04, 0.04, ndlRaw);
        float hi = smoothstep(0.84, 0.88, ndlRaw) * smoothstep(0.30, 0.45, facing);
        float merge = farT * 0.75;                    // distance: bands merge
        lit = mix(lit, 0.62, merge);
        hi *= 1.0 - farT;
        vec3 toon = mix(mix(shadowTone, base, lit), highlight, hi);
        vec3 depthTone = vec3(0.17, 0.21, 0.31);      // painted distance tone
        toon = mix(toon, depthTone, farT * 0.45 + midT * 0.10);
        // Anime: the nozzle is a flat bright cel (two steps), no gradient.
        float cel = glowAmount > 0.66 ? 1.0 : (glowAmount > 0.2 ? 0.6 : 0.0);
        toon = mix(toon, hot, cel);
        FragColor = vec4(pow(max(toon, vec3(0.0)), vec3(1.0 / 1.08)), 1.0);
        return;
    }

    if (fogEnabled)
    {
        float fogFactor =
            clamp(
                (dist - fogStart) / max(fogEnd - fogStart, 0.001),
                0.0,
                1.0
            );

        fogFactor *= fogIntensity;

        vec3 finalFogColor =
            mix(fogNearColor, fogFarColor, fogFactor);

        float luma =
            dot(finalColor, vec3(0.299, 0.587, 0.114));

        vec3 softColor =
            mix(finalColor, vec3(luma), fogFactor * 0.18);

        finalColor =
            mix(softColor, finalFogColor, fogFactor);
    }

    // Modern: emissive nozzle, added after fog (it is a light source).
    finalColor += hot * glowAmount * 2.2;

    finalColor =
        pow(max(finalColor, vec3(0.0)), vec3(1.0 / 1.08));

    FragColor = vec4(finalColor, 1.0);
}
