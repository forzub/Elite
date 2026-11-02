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

    // Near: the object's own material colour (vColor = MeshTriangle
    // baseColor, model data); mid/far: the style's distance fade.
    vec3 distanceHullColor =
        mix(
            mix(vColor, mix(vColor, hullColorMid, 0.5), midT),
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
        // Anime value hierarchy on the object's OWN material colour (vColor),
        // never lightened by distance: light mass / base mass / shadow mass
        // (+ the composite's near-black silhouette and the ranked edge
        // lines). Hard, AA-limited band borders; no PBR specular.
        vec3 albedo = vColor;
        float lumA = dot(albedo, vec3(0.299, 0.587, 0.114));
        // Distance simplifies instead of washing out: a little less chroma,
        // and the base band merges into light (two masses remain).
        albedo = mix(albedo, vec3(lumA), farT * 0.25);
        vec3 lightTint = lightColor / max(max(lightColor.r, max(lightColor.g, lightColor.b)), 1.0e-3);
        // Light materials are compressed in the light mass (no white clay):
        // three clearly separated values for any albedo.
        vec3 lightMass = albedo * mix(vec3(1.0), lightTint, 0.25) * mix(1.05, 0.80, smoothstep(0.50, 0.85, lumA));
        vec3 baseMass = albedo * mix(0.66, 0.55, smoothstep(0.50, 0.85, lumA));
        vec3 shadowMass = albedo * vec3(0.25, 0.27, 0.33);
        float aa = max(fwidth(ndlRaw), 0.002);
        float toLight = smoothstep(0.42 - aa, 0.42 + aa, ndlRaw);
        float toBase = smoothstep(-0.06 - aa, -0.06 + aa, ndlRaw);
        toLight = mix(toLight, toBase, farT);
        vec3 toon = mix(mix(shadowMass, baseMass, toBase), lightMass, toLight);
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
