#version 430 core
// PPI radar post effects (audit #7 D4): core_legacy's vertex layout - drawn by its quad path with a program override
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;
uniform mat4 uMvp;
out vec4 vColor;
out vec2 vTexCoord;
void main()
{
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vColor = aColor;
    vTexCoord = aTexCoord;
}
