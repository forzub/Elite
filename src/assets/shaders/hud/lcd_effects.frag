#version 430 core
// ported from GLSL 120 (audit #7 D4): the core profile rejects 120; drawn through
// core_legacy's program override (same attribute layout, vTexCoord)
in vec2 vTexCoord;
in vec4 vColor;
layout(location = 0) out vec4 fragColor;

uniform sampler2D uScope;
uniform sampler2D uScopePrevious;  // Предыдущий кадр
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
    vec2 uv = vTexCoord;
    vec2 originalUv = uv;
    
    vec2 center = vec2(radarCenterX, radarCenterY);
    vec2 radius = vec2(radarRadiusX, radarRadiusY);
    
    bool inside = isInsideEllipse(uv, center, radius);
    
    // ===== CABLE LINES =====
    if (cableLineIntensity > 0.0 && inside) {
        // the line runs ACROSS THE SCOPE: placed in the ellipse's own height (the scope is flattened by its perspective -
        // a 0.3..0.8 texture row was mostly outside it and the mask cut it off)
        float cy = radarCenterY / resolution.y;
        float ry = radarRadiusY / resolution.y;
        float movement = sin(time * 0.3) * 0.5;
        // it jumps between two rows (was floor(t*0.5) - floor(t*0.4): that GREW with time and left the screen)
        float freezeStep = step(0.5, fract(time * 0.45));
        float pos = cy + ry * (movement + 0.25 * freezeStep);
        float glitch = step(0.95, rand(vec2(time, 0.0))) * 0.1 * ry;
        pos += glitch * sin(time * 50.0);
        float lineWidth = cableLineWidth * ry * 3.0;

        float dist = abs(uv.y - pos);
        
        if (dist < lineWidth) {
            float stepPattern = floor(uv.x * 20.0) / 20.0;
            float brightness = 0.7 + 0.3 * sin(stepPattern * 50.0 + time);
            float edgeNoise = rand(vec2(uv.x * 10.0, time));
            if (dist > lineWidth * 0.8) {
                brightness *= edgeNoise;
            }
            float gray = 0.8 + 0.2 * sin(uv.x * 30.0 + time);
            fragColor = vec4(cableLineColor, 1.0) * brightness * cableLineIntensity;
            return;
        }
    }
    
    // ===== LCD TEARING =====
    if (lcdTearingIntensity > 0.0 && inside) {
        float shift = 0.0;
        // the torn rows inside the scope's (flattened) ellipse: tearingLineN in 0..1 -> across its height
        float tcy = radarCenterY / resolution.y;
        float try = radarRadiusY / resolution.y;
        float t1 = tcy + try * (tearingLine1 * 2.0 - 1.0) * 0.9;
        float t2 = tcy + try * (tearingLine2 * 2.0 - 1.0) * 0.9;
        float t3 = tcy + try * (tearingLine3 * 2.0 - 1.0) * 0.9;
        if (abs(uv.y - t1) < 0.02 * try * 3.0) {
            shift = tearingShift1 / resolution.x;
        }
        else if (abs(uv.y - t2) < 0.015 * try * 3.0) {
            shift = tearingShift2 / resolution.x;
        }
        else if (abs(uv.y - t3) < 0.01 * try * 3.0) {
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
    vec4 currentColor = texture(uScope, uv);
    vec4 prevColor = texture(uScopePrevious, originalUv);
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
        // a burnt-in ghost of the picture, displaced (no previous frame is kept: the scope samples itself offset)
        vec3 ghost = texture(uScopePrevious, originalUv + vec2(0.035, -0.025)).rgb;
        finalColor.rgb = max(finalColor.rgb, ghost * 0.9 * retentionIntensity);
    }
    
    // ===== ПОСТОЯННЫЕ ЭФФЕКТЫ =====
    if (backlightFlickerIntensity > 0.0 && inside) {
        float flicker = 1.0 + backlightFlickerIntensity * 0.3 * sin(time * 120.0);
        finalColor.rgb *= flicker;
    }
    
    if (pixelResponseIntensity > 0.0 && inside) {
        vec4 left = texture(uScope, uv - vec2(1.0/resolution.x, 0.0));
        vec4 right = texture(uScope, uv + vec2(1.0/resolution.x, 0.0));
        finalColor = mix(finalColor, (left + right) * 0.5, 0.1 * pixelResponseIntensity);
    }
    
    // ===== МАСКА =====
    if (!inside) {
        finalColor = vec4(0.0, 0.0, 0.0, 0.0);
    }
    
    fragColor = finalColor;
}