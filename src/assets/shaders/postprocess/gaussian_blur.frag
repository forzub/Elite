#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uImage;
uniform vec2 uDirection;

void main()
{
    vec3 color = texture(uImage, vUv).rgb * 0.2270270270;

    color += texture(uImage, vUv + uDirection * 1.3846153846).rgb * 0.3162162162;
    color += texture(uImage, vUv - uDirection * 1.3846153846).rgb * 0.3162162162;

    color += texture(uImage, vUv + uDirection * 3.2307692308).rgb * 0.0702702703;
    color += texture(uImage, vUv - uDirection * 3.2307692308).rgb * 0.0702702703;

    FragColor = vec4(color, 1.0);
}