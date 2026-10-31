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
        // Anime: three flat light bands, cool shadow, dark ink at the
        // silhouette; colours stay the hull's own (no extra palette).
        float band = ndlRaw > 0.30 ? 1.0 : (ndlRaw > -0.10 ? 0.66 : 0.40);
        vec3 shade = band < 0.5 ? vec3(0.86, 0.90, 1.0) : vec3(1.0);
        vec3 toon = distanceHullColor * (ambientColor * 0.35 + lightColor * 0.55) * band * shade * 1.25;
        if (facing < 0.16)
            toon = vec3(0.05, 0.05, 0.07);
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
