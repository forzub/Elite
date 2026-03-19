#version 330 core

in vec3 vNormal;
in vec3 vColor;
in vec3 vWorldPos;
in vec3 vViewPos;

out vec4 FragColor;

// ===== ОСВЕЩЕНИЕ =====
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

// ===== ЦВЕТА КОРПУСА =====
uniform vec3 hullColor;
uniform vec3 detailColor;
uniform vec3 glowColor;

// ===== ЭФФЕКТЫ ПОВЕРХНОСТИ =====
uniform float fresnelPower;
uniform float rimIntensity;
uniform float edgeIntensity;
uniform float normalBlend;

// ===== ТУМАН =====
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

    vec3 finalColor = hullColor;
    
    float NdotL = max(dot(n, l), 0.0);

    // Rim light - только на краях и очень слабо
    float rimFactor = 1.0 - max(dot(n, viewDir), 0.0);
    float rim = pow(rimFactor, fresnelPower) * 0.1;  // уменьшил до 0.1

    // Fake specular - только на освещенных поверхностях
    float specular = 0.0;
    if (NdotL > 0.0) {
        specular = pow(NdotL, 8.0) * 0.2;  // уменьшил до 0.2
    }

    // Базовая подсветка
    float lighting = 0.4 + 0.6 * NdotL;

    // Добавляем эффекты УМНОЖЕНИЕМ, а не сложением
    lighting *= (1.0 + rim + specular);  // теперь rim и specular УМНОЖАЮТСЯ

    vec3 ambient = ambientColor * 0.1;
    vec3 litColor = hullColor * (lightColor * lighting + ambient);

    vec3 normalColor = n * 0.5 + 0.5;
    finalColor = mix(litColor, normalColor, normalBlend * 0.1);  // normalBlend тоже уменьшил
    
    // Туман
    if (fogEnabled) {
        float dist = length(vWorldPos - cameraPos);
        float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
        fogFactor *= fogIntensity;
        vec3 finalFogColor = mix(fogNearColor, fogFarColor, fogFactor);
        finalColor = mix(finalColor, finalFogColor, fogFactor);
    }
    
    
    // Гамма-коррекция
    finalColor = pow(finalColor, vec3(1.0/1.1));
    
    FragColor = vec4(finalColor, 1.0);
}