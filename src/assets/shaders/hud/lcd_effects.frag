#version 120

uniform sampler2D texture;
uniform sampler2D previousFrame;  // Предыдущий кадр
uniform float time;
uniform vec2 resolution;

// Параметры эллипса
uniform float radarCenterX;
uniform float radarCenterY;
uniform float radarRadiusX;
uniform float radarRadiusY;

// Постоянные эффекты
uniform float backlightFlickerIntensity;
uniform float pixelResponseIntensity;

// Cable Lines
uniform float cableLineIntensity;
uniform float cableLineWidth;
uniform vec3 cableLineColor;

// Tearing
uniform float lcdTearingIntensity;
uniform float tearingLine1, tearingLine2, tearingLine3;
uniform float tearingShift1, tearingShift2, tearingShift3;

// Digital Noise
uniform float noiseIntensity;
uniform float noiseAmount;

// Interference
uniform float interferenceIntensity;
uniform float interferenceSpeed;
uniform float interferenceAmplitude;

// Image Retention
uniform float retentionIntensity;

// Параметры для FREEZE
uniform float freezeIntensity;
uniform int freezeMode;
uniform float freezeHoldStrength;
uniform float freezeStripCount;
uniform float freezeFlickerSpeed;

bool isInsideEllipse(vec2 uv, vec2 center, vec2 radius)
{
    vec2 pixelCoord = uv * resolution;
    vec2 normalized = (pixelCoord - center) / radius;
    return (normalized.x * normalized.x + normalized.y * normalized.y) <= 1.0;
}

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec2 originalUv = uv;
    
    vec2 center = vec2(radarCenterX, radarCenterY);
    vec2 radius = vec2(radarRadiusX, radarRadiusY);
    
    bool inside = isInsideEllipse(uv, center, radius);
    
    // ===== CABLE LINES =====
    if (cableLineIntensity > 0.0 && inside) {
        float movement = sin(time * 0.3) * 0.2;
        float basePos = 0.5 + movement;
        float freezeStep = floor(time * 0.5) - floor(time * 0.4);
        float pos = mix(basePos, basePos + 0.1, freezeStep);
        float glitch = step(0.95, rand(vec2(time, 0.0))) * 0.1;
        pos += glitch * sin(time * 50.0);
        
        float dist = abs(uv.y - pos);
        
        if (dist < cableLineWidth) {
            float stepPattern = floor(uv.x * 20.0) / 20.0;
            float brightness = 0.7 + 0.3 * sin(stepPattern * 50.0 + time);
            float edgeNoise = rand(vec2(uv.x * 10.0, time));
            if (dist > cableLineWidth * 0.8) {
                brightness *= edgeNoise;
            }
            float gray = 0.8 + 0.2 * sin(uv.x * 30.0 + time);
            gl_FragColor = vec4(cableLineColor, 1.0) * brightness * cableLineIntensity;
            return;
        }
    }
    
    // ===== LCD TEARING =====
    if (lcdTearingIntensity > 0.0 && inside) {
        float shift = 0.0;
        if (abs(uv.y - tearingLine1) < 0.02) {
            shift = tearingShift1 / resolution.x;
        }
        else if (abs(uv.y - tearingLine2) < 0.015) {
            shift = tearingShift2 / resolution.x;
        }
        else if (abs(uv.y - tearingLine3) < 0.01) {
            shift = tearingShift3 / resolution.x;
        }
        uv.x += shift;
    }
    
    // ===== INTERFERENCE =====
    if (interferenceIntensity > 0.0 && inside) {
        float wave1 = sin(uv.y * 20.0 + time * interferenceSpeed) * interferenceAmplitude;
        float wave2 = cos(uv.y * 30.0 + time * interferenceSpeed * 1.3) * interferenceAmplitude * 0.5;
        uv.x += (wave1 + wave2) * interferenceIntensity / resolution.x;
    }
    
    // ===== ПОЛУЧАЕМ ЦВЕТА =====
    vec4 currentColor = texture2D(texture, uv);
    vec4 prevColor = texture2D(previousFrame, originalUv);
    vec4 finalColor = currentColor;
    
    // ===== FREEZE =====
    if (freezeIntensity > 0.0 && inside) {
        float freezeAmount = freezeIntensity * freezeHoldStrength;
        
        if (freezeMode == 0) {
            finalColor = mix(currentColor, prevColor, freezeAmount);
        }
        else if (freezeMode == 1) {
            float region = 0.5 + 0.5 * sin(uv.y * 20.0 + time * 2.0);
            finalColor = mix(currentColor, prevColor, freezeAmount * region);
        }
        else if (freezeMode == 2) {
            float stripWidth = 1.0 / freezeStripCount;
            float stripIndex = floor(uv.y / stripWidth);
            float stripMask = fract(stripIndex * 0.5) > 0.25 ? 1.0 : 0.0;
            finalColor = mix(currentColor, prevColor, freezeAmount * stripMask);
        }
        else if (freezeMode == 3) {
            float flicker = 0.5 + 0.5 * sin(time * freezeFlickerSpeed);
            finalColor = mix(currentColor, prevColor, freezeAmount * flicker);
        }
    }
    
    // ===== DIGITAL NOISE =====
    if (noiseIntensity > 0.0 && inside) {
        float noise = rand(uv + floor(time * 24.0));
        if (noise < noiseAmount * noiseIntensity * 0.1) {
            float r = rand(vec2(uv.x + time, uv.y));
            float g = rand(vec2(uv.x, uv.y + time));
            float b = rand(vec2(uv.x - time, uv.y));
            finalColor.rgb += vec3(r, g, b) * 0.3 * noiseIntensity;
        }
    }
    
    // ===== IMAGE RETENTION =====
    if (retentionIntensity > 0.0 && inside) {
        finalColor = mix(finalColor, prevColor, 0.1 * retentionIntensity);
    }
    
    // ===== ПОСТОЯННЫЕ ЭФФЕКТЫ =====
    if (backlightFlickerIntensity > 0.0 && inside) {
        float flicker = 1.0 + backlightFlickerIntensity * 0.3 * sin(time * 120.0);
        finalColor.rgb *= flicker;
    }
    
    if (pixelResponseIntensity > 0.0 && inside) {
        vec4 left = texture2D(texture, uv - vec2(1.0/resolution.x, 0.0));
        vec4 right = texture2D(texture, uv + vec2(1.0/resolution.x, 0.0));
        finalColor = mix(finalColor, (left + right) * 0.5, 0.1 * pixelResponseIntensity);
    }
    
    // ===== МАСКА =====
    if (!inside) {
        finalColor = vec4(0.0, 0.0, 0.0, 0.0);
    }
    
    gl_FragColor = finalColor;
}