#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;
in vec3 vViewPos;
out vec4 FragColor;

// Параметры (пока не используются)
uniform vec3 lightDir = vec3(0.4, 0.7, 0.5);
uniform vec3 lightColor = vec3(1.0, 1.0, 1.0);
uniform vec3 ambientColor = vec3(0.3, 0.3, 0.35);
uniform float fresnelPower = 2.0;
uniform float rimIntensity = 0.5;
uniform vec3 hullColor = vec3(0.0, 0.0, 0.0);

// Grid параметры
uniform bool gridEnabled = false;
uniform vec3 gridLineColor = vec3(1.0, 0.0, 0.0);
uniform float gridLineAlpha = 1.0;
uniform vec3 gridCellSizeXYZ = vec3(100.0);
uniform float gridGlow = 0.2;

void main()
{
    vec3 n = normalize(vNormal);
    
    // ВАРИАНТ 1: Визуализация нормалей как цвет
    vec3 finalColor = n * 0.5 + 0.5;  // переводим из [-1,1] в [0,1]
    
    // Сетка (опционально, можно пока отключить)
    if (gridEnabled) {
        vec3 gridPos = vWorldPos;
        vec3 gridCoords = gridPos / gridCellSizeXYZ;
        vec3 fracCoords = fract(gridCoords);
        vec3 distToLine = min(fracCoords, 1.0 - fracCoords);
        float minDist = min(distToLine.x, min(distToLine.y, distToLine.z));
        
        float gridFactor = 0.0;
        if (minDist < 0.01) {
            gridFactor = 1.0;
        }
        
        float glow = exp(-minDist * 100.0) * gridGlow;
        gridFactor = max(gridFactor, glow);
        
        finalColor = mix(finalColor, gridLineColor, gridFactor * gridLineAlpha);
    }
    
    FragColor = vec4(finalColor, 1.0);
}