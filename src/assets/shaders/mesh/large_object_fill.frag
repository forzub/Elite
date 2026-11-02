#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;
in vec3 vViewPos;
in vec3 vObjectPos;
out vec4 FragColor;

// ===== UNIFORMS =====
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

uniform float fresnelPower;
uniform float rimIntensity;
uniform float normalBlend;

uniform float distClose;
uniform float distMedium;
uniform float distFar;
uniform float distHorizon;
uniform vec3 hullColorNear;
uniform vec3 hullColorMid;
uniform vec3 hullColorFar;

uniform bool gridEnabled;
uniform float gridCellSize;
uniform float gridLineWidth;
uniform vec3 gridLineColor;
uniform float gridLineAlpha;
uniform float gridGlow;
uniform vec3 gridCellSizeXYZ;
uniform vec3 gridOffset;

uniform bool fogEnabled;
uniform float fogStart;
uniform float fogEnd;
uniform vec3 fogNearColor;
uniform vec3 fogFarColor;
uniform float fogIntensity;

uniform vec3 cameraPos;
uniform int uSceneStyle; // 0 Elite Classic, 1 Modern, 2 Anime

void main()
{
    vec3 n = normalize(vNormal);
    vec3 l = normalize(lightDir);
    vec3 v = normalize(cameraPos - vWorldPos);
    vec3 h = normalize(l + v);

    float dist = length(vWorldPos - cameraPos);

    // ===== ЦВЕТ КОРПУСА ПО ДИСТАНЦИИ =====
    float midT = smoothstep(distClose, distMedium, dist);
    float farT = smoothstep(distMedium, distFar, dist);
    float horizonT = smoothstep(distFar, distHorizon, dist);

    vec3 hullColor = mix(mix(hullColorNear, hullColorMid, midT), hullColorFar, farT);

    // Далеко объект светлеет мягко, без белой пересветки.
    hullColor = mix(hullColor, hullColorFar * 1.16, horizonT);

    // ===== ОБЪЁМ =====
    // Старый вариант давал почти чистый Lambert: грань без прямого света становилась
    // почти чёрной. Для техногенного wire/fill рендера нужен не физически честный,
    // а читаемый объём: wrap light + hemisphere ambient + rim.
    float ndl = max(dot(n, l), 0.0);
    float wrap = clamp((dot(n, l) + 0.42) / 1.42, 0.0, 1.0);
    float facing = max(dot(n, v), 0.0);

    float skyT = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 lowerAmbient = ambientColor * 0.20;
    vec3 upperAmbient = ambientColor * 0.58;
    vec3 hemiAmbient = mix(lowerAmbient, upperAmbient, skyT);

    float rim = pow(1.0 - facing, max(fresnelPower, 0.001)) * rimIntensity;
    float spec = pow(max(dot(n, h), 0.0), 28.0) * ndl * 0.10;

    vec3 litColor = hullColor * (hemiAmbient + lightColor * (wrap * 0.48 + ndl * 0.24));
    litColor += hullColorFar * rim * 0.30;
    litColor += lightColor * spec;

    // Лёгкая нормальная подкраска возвращает гранёность/объём, но не превращает
    // модель в радужную visualizer-карту нормалей.
    vec3 normalColor = n * 0.5 + 0.5;
    vec3 finalColor = mix(litColor, litColor * 0.72 + normalColor * hullColorFar * 0.34, normalBlend * 0.45);

    if (uSceneStyle == 0)
    {
        // Elite Classic: opaque black structure; edges draw it.
        FragColor = vec4(vec3(0.012), 1.0);
        return;
    }
    if (uSceneStyle == 2)
    {
        // Anime: the same colour-mass language as ships (base / cool
        // shadow / small warm highlight). Stations are big: the horizon
        // term (distance) merges the bands and lowers the contrast so a far
        // station reads as a painted background element.
        float nl = dot(n, l);
        vec3 albedo = hullColor * 1.10;
        vec3 lightTint = lightColor / max(max(lightColor.r, max(lightColor.g, lightColor.b)), 1.0e-3);
        vec3 base = albedo * mix(vec3(1.0), lightTint, 0.35) * 0.92;
        vec3 shadowTone = albedo * vec3(0.40, 0.44, 0.60) + ambientColor * 0.04;
        vec3 highlight = mix(base, vec3(1.0, 0.94, 0.82), 0.30);
        float lit = smoothstep(-0.04, 0.04, nl);
        float hi = smoothstep(0.86, 0.90, nl) * smoothstep(0.30, 0.45, facing) * (1.0 - horizonT);
        lit = mix(lit, 0.62, horizonT * 0.80);
        vec3 toon = mix(mix(shadowTone, base, lit), highlight, hi);
        toon = mix(toon, vec3(0.17, 0.21, 0.31), horizonT * 0.50);
        FragColor = vec4(pow(max(toon, vec3(0.0)), vec3(1.0 / 1.08)), 1.0);
        return;
    }

    // ===== СЕТКА =====
    if (gridEnabled) {
        vec3 localPos = vObjectPos + gridOffset;

        vec3 tangent = normalize(cross(n, vec3(0.0, 1.0, 0.0)));
        if (length(tangent) < 0.001 || abs(dot(tangent, n)) > 0.99) {
            tangent = normalize(cross(n, vec3(1.0, 0.0, 0.0)));
        }
        vec3 bitangent = normalize(cross(n, tangent));

        vec2 planarPos = vec2(dot(localPos, tangent), dot(localPos, bitangent));
        vec2 gridCoords = planarPos / max(gridCellSizeXYZ.xy, vec2(0.001));
        vec2 fracCoords = fract(gridCoords);
        vec2 distToLine = min(fracCoords, 1.0 - fracCoords) * gridCellSizeXYZ.xy;

        float minDist = min(distToLine.x, distToLine.y);
        float worldToPixel = max(dist / 500.0, 0.001);
        float lineWidth = gridLineWidth * worldToPixel;

        float gridFactor = 1.0 - smoothstep(0.0, lineWidth, minDist);
        float glow = exp(-minDist * minDist / max(lineWidth * lineWidth * 100.0, 0.001)) * gridGlow;
        gridFactor = max(gridFactor, glow);
        gridFactor *= abs(dot(n, v));

        finalColor = mix(finalColor, gridLineColor, gridFactor * gridLineAlpha);
    }

    // ===== ПЕРЦЕПТИВНАЯ ДЫМКА =====
    if (fogEnabled) {
        float fogFactor = clamp((dist - fogStart) / max(fogEnd - fogStart, 0.001), 0.0, 1.0);
        vec3 fogColor = mix(fogNearColor, fogFarColor, fogFactor);

        float luma = dot(finalColor, vec3(0.299, 0.587, 0.114));
        vec3 lowContrastColor = mix(finalColor, vec3(luma), fogFactor * 0.22);
        finalColor = mix(lowContrastColor, fogColor, fogFactor * fogIntensity);
    }

    finalColor = pow(max(finalColor, vec3(0.0)), vec3(1.0 / 1.08));
    FragColor = vec4(finalColor, 1.0);
}
