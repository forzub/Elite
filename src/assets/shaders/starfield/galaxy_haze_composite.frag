#version 330 core

in vec2 vUv;

out vec4 FragColor;

uniform sampler2D uHazeTexture;
uniform int uSceneStyle; // 0 Elite Classic, 1 Modern, 2 Anime

void main()
{
    vec4 haze = texture(uHazeTexture, vUv);
    if (uSceneStyle == 0)
    {
        // Elite Classic: a black sky, the Milky Way only as a faint grey.
        float l = dot(haze.rgb, vec3(0.3, 0.5, 0.2));
        haze = vec4(vec3(l), haze.a * 0.25);
    }
    else if (uSceneStyle == 2)
    {
        // Anime: flat, desaturated bands of haze.
        float l = dot(haze.rgb, vec3(0.3, 0.5, 0.2));
        vec3 c = mix(vec3(l), haze.rgb, 0.45);
        haze = vec4(floor(c * 6.0 + 0.5) / 6.0, floor(haze.a * 4.0 + 0.5) / 4.0);
    }
    FragColor = haze;
}
