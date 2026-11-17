#version 330 core

// Instanced twin of large_object_fill.vert: same outputs, the model matrix
// comes per instance (locations 4..7), VP is shared by the batch.
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 3) in vec3 materialColor; // material base colour (model data)
layout(location = 4) in mat4 instanceModel;

uniform mat4 VP;
uniform vec3 cameraPos;

out vec3 vNormal;
out vec3 vColor;
out vec3 vWorldPos;
out vec3 vViewPos;
out vec3 vObjectPos;
out vec3 vObjectNormal;  // the mesh's own normal (facade windows run along the module's axes)
out float vInstSeed;     // per instance (its position): which windows of this building are lit

void main()
{
    // the facade seed rides in the unused [0][3] slot (SceneRenderer fillModel): read it, then a clean affine matrix
    vInstSeed = instanceModel[0].w;
    mat4 M = instanceModel;
    M[0].w = 0.0;
    vec4 worldPos = M * vec4(position, 1.0);
    vWorldPos = worldPos.xyz;
    gl_Position = VP * worldPos;
    vViewPos = gl_Position.xyz;
    vObjectPos = position;
    vObjectNormal = normal;

    mat3 normalMat = transpose(inverse(mat3(M)));
    vNormal = normalize(normalMat * normal);
    vColor = materialColor;
}
