#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;
in vec3 vViewPos;
in vec3 vObjectPos;
out vec4 FragColor;

// ===== UNIFORMS =====
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;  // теперь используется полностью

uniform float distClose;
uniform float distMedium;
uniform float distFar;
uniform float distHorizon;
uniform vec3 hullColorNear;
uniform vec3 hullColorMid;
uniform vec3 hullColorFar;

uniform bool gridEnabled;
uniform float gridCellSize;
uniform float gridLineWidth;
uniform vec3 gridLineColor;
uniform float gridLineAlpha;
uniform float gridGlow;
uniform vec3 gridCellSizeXYZ;
uniform vec3 gridOffset;

uniform bool fogEnabled;
uniform float fogStart;
uniform float fogEnd;
uniform vec3 fogNearColor;
uniform vec3 fogFarColor;
uniform float fogIntensity;

uniform vec3 cameraPos;

void main()
{
    vec3 n = normalize(vNormal);
    vec3 l = normalize(lightDir);
    vec3 viewDir = normalize(-vViewPos);
    
    float NdotL = max(dot(n, l), 0.0);
    
    // ===== ПРАВИЛЬНОЕ ОСВЕЩЕНИЕ =====
    // Диффузное освещение
    float diffuse = 0.7 * NdotL;
    
    // Ambient как очень слабая добавка (не сумма!)
    float ambientStrength = 0.1;  // можно сделать uniform'ом
    vec3 ambient = ambientColor * ambientStrength;
    
    // Итоговое освещение: ambient + диффуз (ambient не доминирует!)
    vec3 lighting = ambient + vec3(diffuse);
    
    // ===== ЦВЕТ КОРПУСА ПО ДИСТАНЦИИ =====
    float dist = length(vWorldPos - cameraPos);
    vec3 hullColor;
    
    if (dist < distClose) {
        hullColor = hullColorNear;
    } else if (dist < distMedium) {
        float t = (dist - distClose) / (distMedium - distClose);
        hullColor = mix(hullColorNear, hullColorMid, t);
    } else if (dist < distFar) {
        float t = (dist - distMedium) / (distFar - distMedium);
        hullColor = mix(hullColorMid, hullColorFar, t);
    } else {
        float t = min((dist - distFar) / (distHorizon - distFar), 1.0);
        hullColor = mix(hullColorFar, hullColorFar * 1.5, t);
    }
    
    vec3 finalColor = hullColor * lighting * lightColor;
    
    // ===== СЕТКА =====
    if (gridEnabled) {
        vec3 localPos = vObjectPos + gridOffset;
        
        vec3 tangent = normalize(cross(n, vec3(0.0, 1.0, 0.0)));
        if (abs(dot(tangent, n)) > 0.99) {
            tangent = normalize(cross(n, vec3(1.0, 0.0, 0.0)));
        }
        vec3 bitangent = normalize(cross(n, tangent));
        
        vec2 planarPos = vec2(dot(localPos, tangent), dot(localPos, bitangent));
        
        vec3 cellSize = gridCellSizeXYZ;
        vec2 gridCoords = planarPos / vec2(cellSize.x, cellSize.y);
        vec2 fracCoords = fract(gridCoords);
        vec2 distToLine = min(fracCoords, 1.0 - fracCoords) * vec2(cellSize.x, cellSize.y);
        
        float minDist = min(distToLine.x, distToLine.y);
        
        float worldToPixel = dist / 500.0;
        float lineWidth = gridLineWidth * worldToPixel;
        
        float gridFactor = 1.0 - smoothstep(0.0, lineWidth, minDist);
        
        float glow = exp(-minDist * minDist / (lineWidth * lineWidth * 100.0)) * gridGlow;
        gridFactor = max(gridFactor, glow);
        
        float facing = abs(dot(n, viewDir));
        gridFactor *= facing;
        
        finalColor = mix(finalColor, gridLineColor, gridFactor * gridLineAlpha);
    }
    
    // ===== ТУМАН =====
    if (fogEnabled) {
        float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
        vec3 fogColor = mix(fogNearColor, fogFarColor, fogFactor);
        finalColor = mix(finalColor, fogColor, fogFactor * fogIntensity);
    }
    
    FragColor = vec4(finalColor, 1.0);
}