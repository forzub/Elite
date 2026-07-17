#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uScene;
uniform vec2 uSceneTexelSize;
uniform float uThreshold;
uniform float uKnee;

vec3 downsampleScene(vec2 uv)
{
    vec2 halfTexel = uSceneTexelSize * 0.5;

    return 0.25 * (
        texture(uScene, uv + vec2(-halfTexel.x, -halfTexel.y)).rgb +
        texture(uScene, uv + vec2( halfTexel.x, -halfTexel.y)).rgb +
        texture(uScene, uv + vec2(-halfTexel.x,  halfTexel.y)).rgb +
        texture(uScene, uv + vec2( halfTexel.x,  halfTexel.y)).rgb
    );
}

void main()
{
    vec3 color = downsampleScene(vUv);
    float brightness = max(max(color.r, color.g), color.b);

    float knee = max(uKnee, 0.0001);
    float soft = brightness - uThreshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.0001);

    float contribution = max(soft, brightness - uThreshold);
    contribution /= max(brightness, 0.0001);

    FragColor = vec4(color * max(contribution, 0.0), 1.0);
}