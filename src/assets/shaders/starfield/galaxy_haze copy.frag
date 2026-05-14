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

float hash(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(a, b, u.x),
        mix(c, d, u.x),
        u.y
    );
}

float fbm(vec2 p)
{
    float v = 0.0;
    float a = 0.5;

    for (int i = 0; i < 5; ++i)
    {
        v += noise(p) * a;
        p *= 2.07;
        a *= 0.52;
    }

    return v;
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

    // Координаты направления в галактической системе.
    float x = dot(dir, centerDir);
    float y = dot(dir, eastDir);
    float z = dot(dir, northDir);

    float longitude = atan(y, x);
    float latitude = asin(clamp(z, -1.0, 1.0));

    float bandWidthRad = radians(max(uBandWidthDeg, 1.0));
    float coreWidthRad = radians(max(uCoreWidthDeg, 1.0));

    // Главная полоса: ярче у галактической плоскости.
    float band = exp(-pow(latitude / bandWidthRad, 2.0));

    // Широкая внешняя дымка.
    float wideBand = exp(-pow(latitude / (bandWidthRad * 2.45), 2.0));

    // Центр галактики: яркое утолщение около longitude=0.
    float coreLongitude = exp(-pow(longitude / coreWidthRad, 2.0));
    float coreLatitude  = exp(-pow(latitude / (bandWidthRad * 1.85), 2.0));
    float core = coreLongitude * coreLatitude;

    // Противоположная сторона тоже должна быть видна, но слабее.
    float antiCenter = exp(-pow((abs(longitude) - PI) / 0.75, 2.0));

    // Шумовые координаты вдоль галактического пояса.
    vec2 galUv = vec2(
        longitude / PI * 3.0,
        latitude / bandWidthRad
    );

    float cloudA = fbm(galUv * vec2(2.2, 0.85) + vec2(0.13, 1.71));
    float cloudB = fbm(galUv * vec2(7.5, 2.40) + vec2(4.11, 0.37));
    float cloud = mix(cloudA, cloudB, 0.42);

    // Пылевая полоса. Не чёрная дырка, а грязная прожилка.
    float laneOffset =
        0.32 * sin(longitude * 2.1 + cloudA * 1.7)
        + 0.16 * sin(longitude * 5.3 + 1.2);

    float dustLane = exp(-pow((galUv.y - laneOffset) / 0.28, 2.0));
    dustLane *= smoothstep(0.10, 0.95, band);
    dustLane *= 0.35 + 0.65 * coreLongitude;

    float density =
        wideBand * 0.18 +
        band * (0.36 + cloud * 0.42) +
        core * 1.15 +
        antiCenter * band * 0.16;

    density *= 1.0 - dustLane * uDustStrength;

    // Неровный, живой край — иначе будет не космос, а неоновый ремень.
    float edgeBreakup = 0.78 + cloudB * 0.34;
    density *= edgeBreakup;

    density = max(density, 0.0);

    vec3 cold = vec3(0.23, 0.29, 0.46);
    vec3 neutral = vec3(0.50, 0.52, 0.66);
    vec3 warm = vec3(0.95, 0.73, 0.48);

    vec3 color = mix(cold, neutral, saturate(band * 0.75 + cloud * 0.25));
    color = mix(color, warm, saturate(core * 0.72));

    float alpha = density * uIntensity;

    // Очень важно: фон должен быть тонким.
    // Если переборщить, будет не Млечный путь, а грязное стекло.
    alpha = clamp(alpha, 0.0, 0.38);

    FragColor = vec4(color, alpha);
}