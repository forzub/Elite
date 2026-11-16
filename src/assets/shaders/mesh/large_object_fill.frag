#version 330 core

in vec3 vNormal;
in vec3 vColor; // material base colour of this face (model data)
in vec3 vWorldPos;
in vec3 vViewPos;
in vec3 vObjectPos;
in vec3 vObjectNormal;
in float vInstSeed;
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

// ===== FACADE WINDOWS (hub megastructure, 2026-11-16) =====
// A facade material (id eq_hub_facade*) travels as its base colour + 4 (ModelAssetGpuLibrary): its vertical faces get a
// procedural grid of windows in the module's own frame (floors 4 m, pitch 3 m) - no geometry per window. Most buildings
// are dim (5..15 % of the windows lit, author: «светящихся окон надо сделать меньше. полностью засвеченные только
// некоторые здания»), about one in ten is lit up (60..80 %). Far away the grid averages to its mean glow (no shimmer).
float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
vec3 facadeWindows(vec3 nObj)
{
    if (abs(nObj.y) > 0.6)
        return vec3(0.0);                                   // roofs / floors: no windows
    float u = abs(nObj.x) > abs(nObj.z) ? vObjectPos.z : vObjectPos.x;
    vec2 g = vec2(u / 3.0, vObjectPos.y / 4.0);
    vec2 cell = floor(g);
    vec2 f = fract(g);
    float building = hash12(vec2(vInstSeed * 913.0, 17.0));
    float litFrac = building < 0.10 ? mix(0.60, 0.80, hash12(vec2(vInstSeed * 71.0, 3.0)))
                                    : mix(0.05, 0.15, hash12(vec2(vInstSeed * 37.0, 9.0)));
    float floorBias = hash12(vec2(cell.y, vInstSeed * 53.0)) < 0.18 ? 2.2 : 1.0;   // some floors busier
    float h = hash12(cell + vec2(vInstSeed * 131.0, vInstSeed * 977.0));
    float lit = step(h, litFrac * floorBias);
    vec3 warm = vec3(1.0, 0.72, 0.40), cool = vec3(0.55, 0.75, 1.0);
    vec3 col = mix(warm, cool, step(0.78, hash12(cell * 1.7 + 5.0)));
    float pane = step(0.2, f.x) * step(f.x, 0.8) * step(0.3, f.y) * step(f.y, 0.75);
    vec2 fw = fwidth(g);
    float far = smoothstep(0.25, 0.9, max(fw.x, fw.y));      // a window under a pixel: its average instead
    vec3 nearGlow = col * lit * pane;
    vec3 farGlow = mix(warm, cool, 0.22) * litFrac * 0.27 * 1.2;
    return mix(nearGlow, farGlow, far);
}

void main()
{
    // Emissive faces (window lights, beacons, floodlights): ModelAssetGpuLibrary passes their glow colour + 2 as the
    // face colour. Unlit and bright in every style (Classic: points of light on the black structure).
    float colourMax = max(vColor.r, max(vColor.g, vColor.b));
    if (colourMax >= 3.5)
    {
        // a facade: its own colour, lit like any hull (below), plus the window grid
        vec3 base = vColor - vec3(4.0);
        vec3 win = facadeWindows(normalize(vObjectNormal));
        vec3 nn = normalize(vNormal);
        vec3 l2 = normalize(lightDir);
        float wrap2 = clamp((dot(nn, l2) + 0.42) / 1.42, 0.0, 1.0);
        vec3 hull = base * (ambientColor * 0.45 + lightColor * wrap2 * 0.6);
        if (uSceneStyle == 0)
            hull = vec3(0.012);                                  // Classic: black structure, the lights only
        else if (uSceneStyle == 2)
            hull = base * mix(0.30, 0.75, step(0.0, dot(nn, l2)));   // Anime: two flat masses
        FragColor = vec4(pow(max(hull, vec3(0.0)), vec3(1.0 / 1.08)) + win * (uSceneStyle == 1 ? 1.5 : 1.1), 1.0);
        return;
    }
    if (colourMax >= 1.5)
    {
        vec3 glow = max(vColor - vec3(2.0), vec3(0.0));
        FragColor = vec4(glow * (uSceneStyle == 1 ? 1.6 : uSceneStyle == 2 ? 1.15 : 0.95), 1.0);
        return;
    }
    vec3 n = normalize(vNormal);
    vec3 l = normalize(lightDir);
    vec3 v = normalize(cameraPos - vWorldPos);
    vec3 h = normalize(l + v);

    float dist = length(vWorldPos - cameraPos);

    // ===== ЦВЕТ КОРПУСА ПО ДИСТАНЦИИ =====
    float midT = smoothstep(distClose, distMedium, dist);
    float farT = smoothstep(distMedium, distFar, dist);
    float horizonT = smoothstep(distFar, distHorizon, dist);

    // Near: the object's own material colour; mid/far: the style's
    // distance fade (hullColorMid/Far), not a replacement colour.
    vec3 hullColor = mix(mix(vColor, mix(vColor, hullColorMid, 0.5), midT), hullColorFar, farT);

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
        float nl = dot(n, l);
        float farMerge = max(farT, horizonT);
        // Anime value hierarchy on the object's OWN material colour (vColor),
        // never lightened by distance: light mass / base mass / shadow mass
        // (+ the composite's near-black silhouette and the ranked edge
        // lines). Hard, AA-limited band borders; no PBR specular.
        vec3 albedo = vColor;
        float lumA = dot(albedo, vec3(0.299, 0.587, 0.114));
        // Distance simplifies instead of washing out: a little less chroma,
        // and the base band merges into light (two masses remain).
        albedo = mix(albedo, vec3(lumA), farMerge * 0.25);
        vec3 lightTint = lightColor / max(max(lightColor.r, max(lightColor.g, lightColor.b)), 1.0e-3);
        // Light materials are compressed in the light mass (no white clay):
        // three clearly separated values for any albedo.
        vec3 lightMass = albedo * mix(vec3(1.0), lightTint, 0.25) * mix(1.05, 0.80, smoothstep(0.50, 0.85, lumA));
        vec3 baseMass = albedo * mix(0.66, 0.55, smoothstep(0.50, 0.85, lumA));
        vec3 shadowMass = albedo * vec3(0.25, 0.27, 0.33);
        float aa = max(fwidth(nl), 0.002);
        float toLight = smoothstep(0.42 - aa, 0.42 + aa, nl);
        float toBase = smoothstep(-0.06 - aa, -0.06 + aa, nl);
        toLight = mix(toLight, toBase, farMerge);
        vec3 toon = mix(mix(shadowMass, baseMass, toBase), lightMass, toLight);
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
