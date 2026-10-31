#pragma once

// GLSL of the runtime celestial renderer (GLSL 330 core). One surface
// shading function serves the far globe and the local terrain, so the two
// representations of a body look the same where they meet; uStyle selects
// the Elite / Modern / Anime path.

namespace render::celestial_flight::shaders
{
// Logarithmic depth: the camera sits at the origin of a frame that spans
// metres to astronomical units; every pass writes gl_FragDepth.
inline const char* LogDepthVs = R"(
uniform float uLogC;
out float vFlogz;
void applyLogDepth()
{
    vFlogz = 1.0 + gl_Position.w;
    gl_Position.z = (log2(max(1e-6, vFlogz)) * uLogC - 1.0) * gl_Position.w;
}
)";

inline const char* LogDepthFs = R"(
uniform float uLogC;
in float vFlogz;
void writeLogDepth() { gl_FragDepth = log2(max(1e-6, vFlogz)) * uLogC * 0.5; }
)";

// Style constants and the shared surface shading.
inline const char* SurfaceShading = R"(
uniform int uStyle;          // 0 Elite, 1 Modern, 2 Anime
uniform vec3 uLineColour;    // Elite phosphor
uniform vec3 uSunDir;        // world, towards the light
uniform vec3 uSunColour;
uniform vec3 uHazeColour;    // atmosphere tint (day sky at the horizon)
uniform float uHazeDensity;  // 0 = airless
uniform float uContourKm;    // Elite contour spacing
uniform float uDetailStrength; // procedural micro relief (normal tilt)

float hash13(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

// Value noise with analytic gradient (body-anchored micro relief: it never
// swims with the camera).
vec4 noised(vec3 x)
{
    // Lattice wrapped every 256 cells: callers keep the input small by
    // passing anchor positions modulo the period.
    vec3 p = mod(floor(x), 256.0);
    vec3 w = fract(x);
    vec3 u = w * w * (3.0 - 2.0 * w);
    vec3 du = 6.0 * w * (1.0 - w);
    float a = hash13(p), b = hash13(mod(p + vec3(1, 0, 0), 256.0)), c = hash13(mod(p + vec3(0, 1, 0), 256.0)),
          d = hash13(mod(p + vec3(1, 1, 0), 256.0));
    float e = hash13(mod(p + vec3(0, 0, 1), 256.0)), f = hash13(mod(p + vec3(1, 0, 1), 256.0)),
          g = hash13(mod(p + vec3(0, 1, 1), 256.0)), h = hash13(mod(p + vec3(1, 1, 1), 256.0));
    float k0 = a, k1 = b - a, k2 = c - a, k3 = e - a, k4 = a - b - c + d, k5 = a - c - e + g, k6 = a - b - e + f,
          k7 = -a + b + c - d + e - f - g + h;
    float v = k0 + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x +
              k7 * u.x * u.y * u.z;
    vec3 g3 = du * vec3(k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z, k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x,
                        k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y);
    return vec4(v, g3);
}

// Band-limited micro normal perturbation: octaves finer than a pixel fade.
vec3 microNormal(vec3 n, vec3 p, float footprint)
{
    vec3 grad = vec3(0.0);
    float amp = 1.0;
    float freq = 1.0;
    for (int i = 0; i < 5; ++i)
    {
        float fade = clamp(1.5 - footprint * freq * 2.0, 0.0, 1.0);
        if (fade <= 0.0)
            break;
        grad += noised(p * freq).yzw * amp * fade * freq;
        amp *= 0.5;
        freq *= 2.03;
    }
    grad -= n * dot(grad, n);
    return normalize(n - grad * uDetailStrength);
}

float lineMask(float value, float widthPx)
{
    float w = max(fwidth(value), 1e-6);
    return 1.0 - clamp(abs(value) / (w * widthPx), 0.0, 1.0);
}

// Silhouette band: nv = dot(normal, view) grows like a square root away
// from the limb, so the band is measured from the outermost pixel.
float rimMask(float nv, float widthPx)
{
    float w = max(fwidth(nv), 1e-5);
    return 1.0 - clamp((nv - w * 0.5) / (w * widthPx), 0.0, 1.0);
}

float contourMask(float value, float widthPx)
{
    float f = abs(fract(value + 0.5) - 0.5);
    float w = max(fwidth(value), 1e-6);
    return 1.0 - clamp(f / (w * widthPx), 0.0, 1.0);
}

vec3 posterize(vec3 c, float levels)
{
    float l = max(max(c.r, c.g), c.b);
    if (l <= 1e-4)
        return c;
    vec3 hue = c / l;
    float q = floor(l * levels + 0.5) / levels;
    // Saturated, flat colour.
    float grey = dot(hue, vec3(0.333));
    hue = clamp(mix(vec3(grey), hue, 1.35), 0.0, 1.5);
    return hue * max(q, 0.5 / levels);
}

// Shades one surface point. land / water: albedo of the two sides of the
// coastline; coastKm < 0 water. nRelief: relief normal, nSphere: radial.
// viewDir: from the point to the camera. dist: metres (haze).
vec3 shadeSurface(vec3 land, vec3 water, float coastKm, float heightKm, float emission, float ice,
                  vec3 nRelief, vec3 nSphere, vec3 viewDir, float dist, float lattice, float isGas,
                  float cloudShadow, out float alpha)
{
    alpha = 1.0;
    float isWater = coastKm < 0.0 ? 1.0 : 0.0;
    vec3 L = normalize(uSunDir);
    float ns = dot(nSphere, L);
    float day = smoothstep(-0.12, 0.10, ns);

    if (uStyle == 0)
    {
        // Elite: black opaque body, phosphor lines.
        float coast = isGas > 0.5 ? 0.0 : lineMask(coastKm, 1.3);
        float contour = isWater > 0.5 ? 0.0 : contourMask(heightKm / max(uContourKm, 1e-3), 0.9) * 0.55;
        if (isGas > 0.5)
            contour = contourMask(dot(land, vec3(0.3, 0.5, 0.2)) * 9.0, 1.0) * 0.6;
        float grid = lattice >= 0.0 ? lattice * 0.45 : 0.0; // precomputed lattice line mask
        float nv = dot(nSphere, viewDir);
        float rim = rimMask(nv, 1.2);
        // The outline stays bright on the night side (a whole circle, as in
        // the vector original); surface lines dim with the light.
        float fade = 0.30 + 0.70 * day;
        float lines = max(max(coast, contour), grid) * fade;
        float far = uHazeDensity > 0.0 ? exp(-dist * uHazeDensity * 0.3) : 1.0;
        return uLineColour * max(lines * mix(0.35, 1.0, far), rim * 0.9);
    }

    vec3 albedo = mix(land, water, isWater);
    if (uStyle == 2)
    {
        // Anime: posterised palette, banded light, hue-shifted shadows,
        // ink lines on the rim and coast.
        vec3 base = posterize(albedo * 1.15, 5.0);
        float l = dot(normalize(mix(nSphere, nRelief, 0.55)), L);
        vec3 lit = vec3(1.0, 0.98, 0.92);
        vec3 mid = vec3(0.78, 0.66, 0.70);
        vec3 shade = vec3(0.20, 0.22, 0.42);
        vec3 light = l > 0.42 ? lit : (l > 0.04 ? mid : shade);
        vec3 col = base * light * uSunColour;
        if (isWater > 0.5)
        {
            vec3 h = normalize(L + viewDir);
            if (pow(max(dot(nSphere, h), 0.0), 220.0) > 0.5 && l > 0.1)
                col = vec3(1.0);
        }
        col += vec3(1.0, 0.85, 0.35) * step(0.35, emission) * (1.0 - day) * 0.9;
        float nv = dot(nSphere, viewDir);
        if (uHazeDensity > 0.0 && nv < 0.28 && l > 0.0)
            col = mix(col, uHazeColour * 1.2, 0.45);
        float ink = max(rimMask(nv, 2.2), isGas > 0.5 ? 0.0 : lineMask(coastKm, 1.1) * 0.8);
        col = mix(col, vec3(0.04, 0.04, 0.10), ink);
        if (uHazeDensity > 0.0)
        {
            float haze = 1.0 - exp(-dist * uHazeDensity);
            haze = floor(haze * 4.0) / 4.0;
            col = mix(col, uHazeColour * (0.4 + 0.6 * day), haze);
        }
        return col;
    }

    // Modern.
    float nl = max(dot(nRelief, L), 0.0) * day;
    nl = mix(nl, max(ns, 0.0), 0.15 * isGas);
    vec3 col = albedo * uSunColour * (nl * (1.0 - cloudShadow * 0.6) + 0.015);
    if (isWater > 0.5)
    {
        vec3 h = normalize(L + viewDir);
        float spec = pow(max(dot(nRelief, h), 0.0), 90.0) * 0.6 + pow(max(dot(nRelief, h), 0.0), 12.0) * 0.06;
        col += uSunColour * spec * day * (1.0 - cloudShadow);
    }
    col += vec3(1.0, 0.72, 0.38) * emission * emission * (1.0 - day) * 0.6;
    if (uHazeDensity > 0.0)
    {
        float haze = 1.0 - exp(-dist * uHazeDensity);
        float nv = max(dot(nSphere, viewDir), 0.0);
        haze = max(haze, pow(1.0 - nv, 5.0) * 0.6 * min(1.0, uHazeDensity * 3.0e5));
        col = mix(col, uHazeColour * (0.05 + 0.95 * day) * uSunColour, clamp(haze, 0.0, 1.0));
    }
    return col;
}
)";

// ---- far globe ------------------------------------------------------------
inline const char* GlobeVs = R"(
layout(location = 0) in vec3 aDir;
uniform mat4 uViewProj;
uniform mat3 uBodyToWorld;
uniform vec3 uCentre;
uniform float uRadius;
uniform float uDisplace;
uniform samplerCube uRelief;
out vec3 vDirBody;
out vec3 vWorld;
void applyLogDepth();
void main()
{
    vec3 d = normalize(aDir);
    float h = uDisplace > 0.0 ? textureLod(uRelief, d, 0.0).r * 1000.0 * uDisplace : 0.0;
    vec3 p = uBodyToWorld * (d * (uRadius + h)) + uCentre;
    vDirBody = d;
    vWorld = p;
    gl_Position = uViewProj * vec4(p, 1.0);
    applyLogDepth();
}
)";

inline const char* GlobeFs = R"(
in vec3 vDirBody;
in vec3 vWorld;
uniform mat3 uBodyToWorld;
uniform samplerCube uAlbedo;
uniform samplerCube uNormal;
uniform samplerCube uRelief;
uniform samplerCube uCloudCover;
uniform float uHasClouds;
uniform float uIsGas;
uniform float uPixelAngle;
uniform float uRadius;
uniform vec3 uFlat;          // colour while the texture bakes
uniform float uHasTexture;
out vec4 fragColour;
void writeLogDepth();
vec3 shadeSurface(vec3 land, vec3 water, float coastKm, float heightKm, float emission, float ice,
                  vec3 nRelief, vec3 nSphere, vec3 viewDir, float dist, float lattice, float isGas,
                  float cloudShadow, out float alpha);
vec3 microNormal(vec3 n, vec3 p, float footprint);
uniform float uMicroWavelength;
void main()
{
    vec3 d = normalize(vDirBody);
    vec3 ns = normalize(uBodyToWorld * d);
    float dist = length(vWorld);
    vec3 v = -vWorld / dist;
    vec3 land = uFlat, water = uFlat;
    float coastKm = 1.0, heightKm = 0.0, emission = 0.0;
    vec3 nr = ns;
    if (uHasTexture > 0.5)
    {
        vec4 a = texture(uAlbedo, d);
        vec4 n = texture(uNormal, d);
        vec2 r = texture(uRelief, d).rg;
        vec3 c = a.rgb * a.rgb;
        land = c;
        water = c;
        coastKm = r.g;
        heightKm = r.r;
        emission = n.a;
        nr = normalize(uBodyToWorld * (n.xyz * 2.0 - 1.0));
    }
    // Micro relief below the texel (screen-footprint band limited).
    if (uMicroWavelength > 0.0 && uIsGas < 0.5)
        nr = microNormal(nr, d * (uRadius / uMicroWavelength), dist * uPixelAngle / uMicroWavelength);
    float shadow = 0.0;
    if (uHasClouds > 0.5)
        shadow = texture(uCloudCover, normalize(d + 0.004 * (transpose(uBodyToWorld) * uSunDir))).r;
    float alpha;
    vec3 col = shadeSurface(land, water, coastKm, heightKm, emission, 0.0, nr, ns, v, dist, -1.0, uIsGas,
                            shadow, alpha);
    fragColour = vec4(col, 1.0);
    writeLogDepth();
}
)";

// ---- local terrain (camera-centred clipmap) --------------------------------
inline const char* TerrainVs = R"(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aMorph;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec2 aLattice;
layout(location = 4) in vec4 aLand;
layout(location = 5) in vec4 aWater;
layout(location = 6) in float aCoast;
layout(location = 7) in float aHeight;
uniform mat4 uViewProj;
uniform mat3 uBodyToWorld;
uniform vec3 uAnchor;       // level anchor relative to the camera (world)
uniform vec2 uCamLattice;   // camera offset from the level centre (cells)
uniform float uHalfCells;
uniform vec2 uLatticeBase;  // centre index mod 64 (Elite grid)
out vec3 vWorld;
out vec3 vNormal;
out vec3 vBodyPos;
out vec4 vLand;
out vec4 vWater;
out float vCoast;
out float vHeight;
out vec2 vLattice;
void applyLogDepth();
void main()
{
    vec2 off = abs(aLattice - uCamLattice) / uHalfCells;
    float m = clamp((max(off.x, off.y) - 0.40) / 0.35, 0.0, 1.0);
    vec3 pb = mix(aPos, aMorph, m);
    vec3 p = uBodyToWorld * pb + uAnchor;
    vWorld = p;
    vBodyPos = pb;
    vNormal = uBodyToWorld * aNormal;
    vLand = aLand;
    vWater = aWater;
    vCoast = aCoast;
    vHeight = aHeight;
    vLattice = (aLattice + uLatticeBase) / 8.0;
    gl_Position = uViewProj * vec4(p, 1.0);
    applyLogDepth();
}
)";

inline const char* TerrainFs = R"(
in vec3 vWorld;
in vec3 vNormal;
in vec3 vBodyPos;
in vec4 vLand;
in vec4 vWater;
in float vCoast;
in float vHeight;
in vec2 vLattice;
uniform mat3 uBodyToWorld;
uniform vec3 uBodyCentre;   // body centre relative to the camera (world)
uniform float uPixelAngle;
uniform float uMicroScale;  // 1 / micro wavelength (m)
uniform vec3 uAnchorBody;   // anchor in the body frame, mod the micro period
out vec4 fragColour;
void writeLogDepth();
vec3 shadeSurface(vec3 land, vec3 water, float coastKm, float heightKm, float emission, float ice,
                  vec3 nRelief, vec3 nSphere, vec3 viewDir, float dist, float lattice, float isGas,
                  float cloudShadow, out float alpha);
vec3 microNormal(vec3 n, vec3 p, float footprint);
void main()
{
    float dist = length(vWorld);
    vec3 v = -vWorld / dist;
    vec3 ns = normalize(vWorld - uBodyCentre);
    vec3 nr = normalize(vNormal);
    float footprint = dist * uPixelAngle * uMicroScale;
    nr = microNormal(nr, (vBodyPos + uAnchorBody) * uMicroScale, footprint);
    vec3 land = vLand.rgb * vLand.rgb;
    vec3 water = vWater.rgb * vWater.rgb;
    float alpha;
    // Elite lattice wireframe: the nearer of the two grid families.
    vec2 g = abs(fract(vLattice + 0.5) - 0.5) / max(fwidth(vLattice), vec2(1e-6));
    float lat = 1.0 - clamp(min(g.x, g.y) / 0.8, 0.0, 1.0);
    vec3 col = shadeSurface(land, water, vCoast / 1000.0, vHeight / 1000.0, vWater.a, vLand.a, nr, ns, v, dist,
                            lat, 0.0, 0.0, alpha);
    fragColour = vec4(col, 1.0);
    writeLogDepth();
}
)";

// Depth only (the near pass: ships and hubs are occluded by the ground).
inline const char* TerrainDepthVs = R"(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aMorph;
layout(location = 3) in vec2 aLattice;
uniform mat4 uViewProj;
uniform mat3 uBodyToWorld;
uniform vec3 uAnchor;
uniform vec2 uCamLattice;
uniform float uHalfCells;
void main()
{
    vec2 off = abs(aLattice - uCamLattice) / uHalfCells;
    float m = clamp((max(off.x, off.y) - 0.40) / 0.35, 0.0, 1.0);
    gl_Position = uViewProj * vec4(uBodyToWorld * mix(aPos, aMorph, m) + uAnchor, 1.0);
}
)";
inline const char* TerrainDepthFs = R"(
out vec4 fragColour;
void main() { fragColour = vec4(0.0); }
)";

// ---- cloud shell ----------------------------------------------------------
inline const char* ShellVs = R"(
layout(location = 0) in vec3 aDir;
uniform mat4 uViewProj;
uniform mat3 uBodyToWorld;
uniform vec3 uCentre;
uniform float uShellRadius;
out vec3 vDirBody;
out vec3 vWorld;
void applyLogDepth();
void main()
{
    vec3 d = normalize(aDir);
    vec3 p = uBodyToWorld * (d * uShellRadius) + uCentre;
    vDirBody = d;
    vWorld = p;
    gl_Position = uViewProj * vec4(p, 1.0);
    applyLogDepth();
}
)";

inline const char* CloudFs = R"(
in vec3 vDirBody;
in vec3 vWorld;
uniform mat3 uBodyToWorld;
uniform samplerCube uCover;
uniform samplerCube uCoverPrev;
uniform float uBlend;
uniform float uOpacity;
uniform vec3 uCentre;
out vec4 fragColour;
void writeLogDepth();
float lineMask(float value, float widthPx);
void main()
{
    vec3 d = normalize(vDirBody);
    float c = mix(texture(uCoverPrev, d).r, texture(uCover, d).r, uBlend);
    vec3 n = normalize(vWorld - uCentre);
    vec3 L = normalize(uSunDir);
    float day = smoothstep(-0.15, 0.12, dot(n, L));
    vec3 v = normalize(-vWorld);
    if (uStyle == 0)
    {
        float edge = lineMask(c - 0.55, 1.2);
        if (edge < 0.05)
            discard;
        fragColour = vec4(uLineColour * (0.25 + 0.5 * day), edge * 0.7);
    }
    else if (uStyle == 2)
    {
        float a = step(0.52, c) * uOpacity;
        if (a < 0.01)
            discard;
        float l = dot(n, L);
        vec3 col = l > 0.25 ? vec3(1.0) : (l > -0.05 ? vec3(0.80, 0.80, 0.92) : vec3(0.18, 0.20, 0.36));
        float rim = 1.0 - clamp(abs(c - 0.52) / max(fwidth(c), 1e-5) / 2.0, 0.0, 1.0);
        col = mix(col, vec3(0.45, 0.50, 0.70), rim * 0.6);
        fragColour = vec4(col * uSunColour, a);
    }
    else
    {
        float a = smoothstep(0.30, 0.78, c) * uOpacity;
        if (a < 0.003)
            discard;
        float l = max(dot(n, L), 0.0);
        float thick = smoothstep(0.5, 1.0, c);
        vec3 col = uSunColour * (0.08 + 0.92 * day) * mix(1.0, 0.72, thick * (1.0 - l));
        // Forward scattering at the limb.
        col += uSunColour * pow(max(dot(-v, L), 0.0), 8.0) * 0.25 * day;
        fragColour = vec4(col * (0.04 + 0.96 * day), a * (0.35 + 0.65 * day + 0.2));
    }
    writeLogDepth();
}
)";

// ---- atmosphere shell (single scattering, game grade) ----------------------
inline const char* AtmosphereFs = R"(
in vec3 vDirBody;
in vec3 vWorld;
uniform vec3 uCentre;       // body centre relative to the camera
uniform float uPlanetRadius;
uniform float uShellRadius;
uniform float uScaleHeight;
uniform vec3 uScatter;      // Rayleigh-like colour weights
uniform float uDensity;     // optical scale
uniform vec3 uSunDir;
uniform vec3 uSunColour;
uniform int uStyle;
uniform vec3 uLineColour;
uniform int uSteps;
out vec4 fragColour;
void writeLogDepth();

vec2 raySphere(vec3 o, vec3 d, float r)
{
    float b = dot(o, d);
    float c = dot(o, o) - r * r;
    float h = b * b - c;
    if (h < 0.0)
        return vec2(-1.0, -2.0);
    h = sqrt(h);
    return vec2(-b - h, -b + h);
}

void main()
{
    // Work in planet radii around the body centre: the camera is at -uCentre.
    float R = uPlanetRadius;
    vec3 o = -uCentre / R;
    vec3 dir = normalize(vWorld);
    float top = uShellRadius / R;
    vec2 tShell = raySphere(o, dir, top);
    if (tShell.y < 0.0)
        discard;
    float t0 = max(tShell.x, 0.0);
    float t1 = tShell.y;
    vec2 tGround = raySphere(o, dir, 1.0);
    bool hitsGround = tGround.x > 0.0;
    if (hitsGround)
        t1 = min(t1, tGround.x);
    vec3 L = normalize(uSunDir);
    float H = uScaleHeight / R;

    if (uStyle == 0)
    {
        // Elite: a single outline of the atmosphere limb.
        vec3 pc = o + dir * clamp(-dot(o, dir), t0, tShell.y);
        float r = length(pc);
        float edge = 1.0 - clamp(abs(r - top) / max(fwidth(r), 1e-6) / 1.2, 0.0, 1.0);
        if (edge < 0.05)
            discard;
        fragColour = vec4(uLineColour * 0.6, edge * 0.8);
        writeLogDepth();
        return;
    }

    int steps = max(uSteps, 2);
    float ds = (t1 - t0) / float(steps);
    vec3 sum = vec3(0.0);
    float depthView = 0.0;
    for (int i = 0; i < 32; ++i)
    {
        if (i >= steps)
            break;
        vec3 p = o + dir * (t0 + ds * (float(i) + 0.5));
        float r = length(p);
        float rho = exp(-(r - 1.0) / H);
        depthView += rho * ds;
        // Sun visibility: the planet's shadow (soft terminator).
        float mu = dot(p / r, L);
        float sunVis = smoothstep(-0.18, 0.08, mu + (r - 1.0) * 3.0);
        float sunPath = rho * H * (1.0 + 6.0 * (1.0 - clamp(mu, 0.0, 1.0)));
        vec3 att = exp(-uScatter * uDensity * (sunPath + depthView));
        sum += rho * sunVis * att * ds;
    }
    float cosTheta = dot(dir, L);
    float phaseR = 0.75 * (1.0 + cosTheta * cosTheta);
    float g = 0.76;
    float phaseM = (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5) * 0.08;
    vec3 colour = uSunColour * sum * uDensity * (uScatter * phaseR + vec3(phaseM));
    float alpha = clamp(1.0 - exp(-depthView * uDensity * 0.6), 0.0, 1.0);
    if (uStyle == 2)
    {
        // Anime: flat stepped sky bands and a clean limb halo.
        float lum = dot(colour, vec3(0.3, 0.5, 0.2));
        float band = floor(clamp(lum * 3.0, 0.0, 3.0) + 0.5) / 3.0;
        colour = normalize(uScatter + 0.15) * band * 1.4 * uSunColour;
        alpha = floor(alpha * 4.0 + 0.5) / 4.0;
    }
    vec3 mapped = 1.0 - exp(-colour * 1.6);
    fragColour = vec4(mapped, max(alpha * 0.25, max(max(mapped.r, mapped.g), mapped.b)));
    writeLogDepth();
}
)";

// ---- rings ----------------------------------------------------------------
inline const char* RingVs = R"(
layout(location = 0) in vec2 aCorner;
uniform mat4 uViewProj;
uniform vec3 uCentre;
uniform vec3 uAxisU;
uniform vec3 uAxisV;
uniform float uOuter;
out vec3 vWorld;
out vec2 vPlane;
void applyLogDepth();
void main()
{
    vPlane = aCorner * uOuter;
    vec3 p = uCentre + uAxisU * vPlane.x + uAxisV * vPlane.y;
    vWorld = p;
    gl_Position = uViewProj * vec4(p, 1.0);
    applyLogDepth();
}
)";

inline const char* RingFs = R"(
in vec3 vWorld;
in vec2 vPlane;
uniform sampler2D uProfile;  // x = (r - inner) / (outer - inner): rgb tint, a opacity
uniform float uInner;
uniform float uOuter;
uniform vec3 uCentre;
uniform float uPlanetRadius;
uniform vec3 uNormal;
out vec4 fragColour;
void writeLogDepth();
float lineMask(float value, float widthPx);
void main()
{
    float r = length(vPlane);
    if (r < uInner || r > uOuter)
        discard;
    float x = (r - uInner) / (uOuter - uInner);
    vec4 prof = texture(uProfile, vec2(x, 0.5));
    vec3 L = normalize(uSunDir);
    // Planet shadow: the ray from the ring point towards the sun.
    vec3 p = vWorld - uCentre;
    float b = dot(p, L);
    float c = dot(p, p) - uPlanetRadius * uPlanetRadius;
    float shadow = (b < 0.0 && b * b - c > 0.0) ? 1.0 : 0.0;
    if (uStyle == 0)
    {
        float edges = max(lineMask(r - uInner, 1.2), lineMask(r - uOuter, 1.2));
        float bands = clamp(abs(dFdx(prof.a)) + abs(dFdy(prof.a)), 0.0, 1.0) * 6.0;
        float fine = lineMask(fract(x * 24.0 + 0.5) - 0.5, 0.7) * 0.35 * step(0.05, prof.a);
        float m = clamp(max(max(edges, bands), fine), 0.0, 1.0);
        if (m < 0.04)
            discard;
        fragColour = vec4(uLineColour * (1.0 - 0.6 * shadow), m * 0.85);
        writeLogDepth();
        return;
    }
    float lit = (0.25 + 0.75 * abs(dot(uNormal, L))) * (1.0 - 0.85 * shadow);
    float a = prof.a;
    vec3 col = prof.rgb * uSunColour * lit;
    if (uStyle == 2)
    {
        a = floor(a * 3.0 + 0.6) / 3.0;
        col = floor(col * 4.0 + 0.5) / 4.0;
        float edge = max(lineMask(r - uInner, 1.5), lineMask(r - uOuter, 1.5));
        col = mix(col, vec3(0.06, 0.06, 0.14), edge);
        a = max(a, edge);
    }
    if (a < 0.01)
        discard;
    fragColour = vec4(col, a);
    writeLogDepth();
}
)";

// ---- stars (disc) ---------------------------------------------------------
inline const char* StarFs = R"(
in vec3 vDirBody;
in vec3 vWorld;
uniform vec3 uCentre;
uniform vec3 uColour;
out vec4 fragColour;
void writeLogDepth();
float lineMask(float value, float widthPx);
void main()
{
    vec3 n = normalize(vWorld - uCentre);
    vec3 v = normalize(-vWorld);
    float mu = max(dot(n, v), 0.0);
    if (uStyle == 0)
    {
        float rim = rimMask(mu, 1.5);
        float ring = lineMask(mu - 0.6, 1.0) * 0.5;
        fragColour = vec4(uLineColour * max(rim, ring), 1.0);
    }
    else if (uStyle == 2)
    {
        vec3 col = mu > 0.35 ? vec3(1.0, 1.0, 0.95) : uColour * 1.3;
        fragColour = vec4(col, 1.0);
    }
    else
    {
        float limb = 1.0 - 0.6 * (1.0 - mu) - 0.15 * (1.0 - mu * mu);
        fragColour = vec4(uColour * limb * 3.0, 1.0);
    }
    writeLogDepth();
}
)";

// ---- glow / point sprites (screen space) -----------------------------------
inline const char* SpriteVs = R"(
layout(location = 0) in vec3 aPos;    // relative to the camera (world)
layout(location = 1) in vec4 aColour; // rgb, a = intensity
layout(location = 2) in float aSize;  // pixels
layout(location = 3) in float aKind;  // 0 point, 1 star glow
uniform mat4 uViewProj;
out vec4 vColour;
out float vKind;
void applyLogDepth();
void main()
{
    gl_Position = uViewProj * vec4(aPos, 1.0);
    applyLogDepth();
    gl_PointSize = aSize;
    vColour = aColour;
    vKind = aKind;
}
)";

inline const char* SpriteFs = R"(
in vec4 vColour;
in float vKind;
uniform int uStyle;
uniform vec3 uLineColour;
out vec4 fragColour;
void writeLogDepth();
void main()
{
    vec2 q = gl_PointCoord * 2.0 - 1.0;
    float r = length(q);
    float a;
    vec3 col = vColour.rgb;
    if (uStyle == 0)
    {
        // Elite: a crisp dot or an outlined star with rays.
        if (vKind > 0.5)
        {
            float ring = 1.0 - clamp(abs(r - 0.35) * 18.0, 0.0, 1.0);
            float rays = (abs(q.x) < 0.04 || abs(q.y) < 0.04 || abs(abs(q.x) - abs(q.y)) < 0.05) && r > 0.45 && r < 0.95
                ? 1.0 - r : 0.0;
            a = max(ring, rays * 1.2);
        }
        else
            a = r < 0.6 ? 1.0 : 0.0;
        col = uLineColour;
    }
    else if (uStyle == 2)
    {
        if (vKind > 0.5)
        {
            float flare = max(1.0 - abs(q.x) * 14.0 - abs(q.y) * 1.4, 1.0 - abs(q.y) * 14.0 - abs(q.x) * 1.4);
            float core = 1.0 - smoothstep(0.10, 0.16, r);
            float ring = (1.0 - smoothstep(0.0, 0.03, abs(r - 0.42))) * 0.35;
            a = max(max(flare, core), ring);
            col = mix(col, vec3(1.0), core);
        }
        else
            a = max(1.0 - abs(q.x) * 4.0 - abs(q.y) * 0.9, 1.0 - abs(q.y) * 4.0 - abs(q.x) * 0.9);
    }
    else
    {
        a = vKind > 0.5 ? exp(-r * r * 9.0) + 0.25 * exp(-r * 3.5) : exp(-r * r * 4.0);
    }
    a = clamp(a, 0.0, 1.0) * vColour.a;
    if (a < 0.004)
        discard;
    fragColour = vec4(col * a, a);
    writeLogDepth();
}
)";

// ---- asteroids -------------------------------------------------------------
inline const char* MeshVs = R"(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aAlbedo;
uniform mat4 uViewProj;
uniform mat3 uRot;
uniform vec3 uCentre;
out vec3 vWorld;
out vec3 vNormal;
out vec3 vAlbedo;
void applyLogDepth();
void main()
{
    vec3 p = uRot * aPos + uCentre;
    vWorld = p;
    vNormal = uRot * aNormal;
    vAlbedo = aAlbedo;
    gl_Position = uViewProj * vec4(p, 1.0);
    applyLogDepth();
}
)";

inline const char* MeshFs = R"(
in vec3 vWorld;
in vec3 vNormal;
in vec3 vAlbedo;
uniform int uStyle;
uniform vec3 uLineColour;
uniform vec3 uSunDir;
uniform vec3 uSunColour;
uniform float uLinePass;
out vec4 fragColour;
void writeLogDepth();
void main()
{
    vec3 n = normalize(vNormal);
    vec3 L = normalize(uSunDir);
    float l = dot(n, L);
    vec3 col;
    if (uStyle == 0)
        col = uLinePass > 0.5 ? uLineColour * (0.35 + 0.65 * clamp(l * 0.5 + 0.5, 0.0, 1.0)) : vec3(0.0);
    else if (uStyle == 2)
    {
        vec3 light = l > 0.35 ? vec3(1.0) : (l > 0.0 ? vec3(0.62, 0.55, 0.62) : vec3(0.16, 0.17, 0.32));
        col = floor(vAlbedo * 6.0 + 0.5) / 6.0 * 1.4 * light * uSunColour;
        float nv = dot(n, normalize(-vWorld));
        if (nv < 0.22)
            col = vec3(0.05, 0.05, 0.10);
    }
    else
        col = vAlbedo * uSunColour * (max(l, 0.0) + 0.02);
    fragColour = vec4(col, 1.0);
    writeLogDepth();
}
)";
} // namespace render::celestial_flight::shaders
