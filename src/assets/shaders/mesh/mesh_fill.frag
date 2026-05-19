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

    finalColor =
        pow(max(finalColor, vec3(0.0)), vec3(1.0 / 1.08));

    FragColor = vec4(finalColor, 1.0);
}
