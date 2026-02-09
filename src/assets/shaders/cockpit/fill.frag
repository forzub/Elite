#version 330 core

in vec2 vLocal;    // [0..1] — КЛЮЧЕВО
in vec4 vColor;

out vec4 FragColor;

// 0 = solid
// 1 = linear gradient
// 2 = radial gradient
uniform int  uFillType;

uniform vec4 uColorA;
uniform vec4 uColorB;

// В SVG-пространстве [0..1]
uniform vec2 uGradFrom;
uniform vec2 uGradTo;

float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

void main()
{
    // ---------------- SOLID ----------------
    if (uFillType == 0)
    {
        FragColor = uColorA;
        return;
    }

    // ---------------- LINEAR GRADIENT ----------------
    if (uFillType == 1)
    {
        vec2 dir = uGradTo - uGradFrom;
        float len2 = dot(dir, dir);

        float t = 0.0;
        if (len2 > 1e-6)
        {
            t = dot(vLocal - uGradFrom, dir) / len2;
        }

        t = saturate(t);
        FragColor = mix(uColorA, uColorB, t);
        return;
    }

    // ---------------- RADIAL GRADIENT ----------------
    if (uFillType == 2)
    {
        float radius = length(uGradTo - uGradFrom);
        float dist   = length(vLocal - uGradFrom);

        float t = 0.0;
        if (radius > 1e-6)
        {
            t = dist / radius;
        }

        t = saturate(t);
        FragColor = mix(uColorA, uColorB, t);
        return;
    }

    // ---------------- FALLBACK ----------------
    FragColor = vec4(1.0, 0.0, 1.0, 1.0); // magenta = ошибка
}
