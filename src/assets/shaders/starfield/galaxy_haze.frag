#version 330 core

in vec2 vUv;

out vec4 FragColor;

uniform mat4 uInvProjection;
uniform mat4 uInvViewRotation;

uniform vec3 uGalacticCenterDir;
uniform vec3 uGalacticNorthDir;

uniform float uBandWidthDeg;
uniform float uCoreWidthDeg;
uniform float uIntensity;
uniform float uDustStrength;

const float PI = 3.14159265358979323846;

float saturate(float v)
{
    return clamp(v, 0.0, 1.0);
}

float softWaveNoise(vec2 p)
{
    float v = 0.0;

    v += sin(p.x * 1.7 + p.y * 0.8 + 0.4) * 0.22;
    v += sin(p.x * 3.1 - p.y * 1.4 + 2.1) * 0.17;
    v += sin(p.x * 5.3 + p.y * 2.6 + 4.7) * 0.11;
    v += sin(p.x * 9.2 - p.y * 4.1 + 1.3) * 0.06;

    return v * 0.5 + 0.5;
}

float softDirectionalNoise(vec3 p)
{
    float v = 0.0;

    v += sin(dot(p, vec3(1.7, 0.8, 1.3)) + 0.4) * 0.22;
    v += sin(dot(p, vec3(3.1, -1.4, 2.6)) + 2.1) * 0.17;
    v += sin(dot(p, vec3(5.3, 2.6, -3.7)) + 4.7) * 0.11;
    v += sin(dot(p, vec3(9.2, -4.1, 6.4)) + 1.3) * 0.06;

    return v * 0.5 + 0.5;
}

vec3 reconstructWorldDir()
{
    vec2 ndc = vUv * 2.0 - 1.0;

    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 view = uInvProjection * clip;

    vec3 viewDir = normalize(view.xyz / max(abs(view.w), 0.00001));
    vec3 worldDir = normalize((uInvViewRotation * vec4(viewDir, 0.0)).xyz);

    return worldDir;
}

void main()
{
    vec3 dir = reconstructWorldDir();

    vec3 centerDir = normalize(uGalacticCenterDir);
    vec3 northDir = normalize(uGalacticNorthDir);
    vec3 eastDir = normalize(cross(northDir, centerDir));

    float x = dot(dir, centerDir);
    float y = dot(dir, eastDir);
    float z = dot(dir, northDir);

    float longitude = atan(y, x);
    float latitude = asin(clamp(z, -1.0, 1.0));

    float bandWidthRad = radians(max(uBandWidthDeg, 1.0));
    float coreWidthRad = radians(max(uCoreWidthDeg, 1.0));

    float band = exp(-pow(latitude / bandWidthRad, 2.0));
    float wideBand = exp(-pow(latitude / (bandWidthRad * 2.45), 2.0));

    float coreLongitude = exp(-pow(longitude / coreWidthRad, 2.0));
    float coreLatitude  = exp(-pow(latitude / (bandWidthRad * 1.85), 2.0));
    float core = coreLongitude * coreLatitude;

    float antiCenter = exp(-pow((abs(longitude) - PI) / 0.75, 2.0));

    float latNorm = latitude / bandWidthRad;

    // Шум теперь сферический: без longitude-разреза, без ступеньки.
    vec3 noiseDir = normalize(
        centerDir * x * 1.15 +
        eastDir * y * 1.35 +
        northDir * z * 0.85
    );

    float cloudA = softDirectionalNoise(noiseDir * 1.7 + vec3(0.2, 1.7, 0.4));
    float cloudB = softDirectionalNoise(noiseDir * 3.4 + vec3(3.1, 0.4, 2.2));
    float cloudC = softDirectionalNoise(noiseDir * 6.8 + vec3(1.2, 4.6, 5.1));

    float cloud = cloudA * 0.52 + cloudB * 0.31 + cloudC * 0.17;

    // Эти sin(longitude) безопасны, потому что sin периодичен.
    // Шов даёт не sin, а использование longitude как прямой координаты шума.
    float laneOffset =
        0.28 * sin(longitude * 2.1 + cloudA * 1.7)
        + 0.13 * sin(longitude * 5.3 + 1.2);

    float dustLane = exp(-pow((latNorm - laneOffset) / 0.28, 2.0));
    
    dustLane *= smoothstep(0.10, 0.95, band);
    dustLane *= 0.35 + 0.65 * coreLongitude;

    float density =
        wideBand * 0.18 +
        band * (0.36 + cloud * 0.42) +
        core * 1.15 +
        antiCenter * band * 0.08;

    density *= 1.0 - dustLane * uDustStrength;

    density *= 0.78 + cloudB * 0.32;
    density = max(density, 0.0);

    vec3 cold = vec3(0.23, 0.29, 0.46);
    vec3 neutral = vec3(0.50, 0.52, 0.66);
    vec3 warm = vec3(0.95, 0.73, 0.48);

    vec3 color = mix(cold, neutral, saturate(band * 0.75 + cloud * 0.25));
    color = mix(color, warm, saturate(core * 0.72));

    float alpha = density * uIntensity;
    alpha = clamp(alpha, 0.0, 0.38);

    FragColor = vec4(color, alpha);
}