#version 330 core

in vec3 vColor;
in float vAlpha;
out vec4 FragColor;

void main()
{
    // a soft round point
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(c, c);
    if (r2 > 1.0)
        discard;
    float fall = exp(-3.0 * r2);
    FragColor = vec4(vColor * vAlpha * fall, 1.0);
}
