#pragma once

#include <glm/glm.hpp>
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>

#include "src/render/ShaderProgram.h"

struct LightingParams {
    // ===== БАЗОВОЕ ОСВЕЩЕНИЕ =====
    glm::vec3 lightDir = glm::vec3(0.4f, 0.7f, 0.5f);
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 ambientColor = glm::vec3(0.3f, 0.3f, 0.35f);
    
    // ===== ЦВЕТ КОРПУСА =====
    glm::vec3 hullColor = glm::vec3(0.8f, 0.8f, 0.9f);        // основной цвет корпуса
    glm::vec3 detailColor = glm::vec3(0.6f, 0.6f, 0.8f);      // цвет деталей
    glm::vec3 glowColor = glm::vec3(0.5f, 0.7f, 1.0f);        // цвет свечения
    
    // ===== ЭФФЕКТЫ ПОВЕРХНОСТИ =====
    float fresnelPower = 2.0f;        // степень френеля (подсветка краев)
    float rimIntensity = 0.5f;         // интенсивность подсветки краев
    float edgeIntensity = 0.3f;        // интенсивность ребер
    float normalBlend = 0.15f;          // смешивание с цветом нормалей
    
    // ===== ЦВЕТ КОРПУСА ПО ЗОНАМ (для будущего использования) =====
    glm::vec3 hullColorNear = glm::vec3(0.03f, 0.03f, 0.04f);
    glm::vec3 hullColorMid = glm::vec3(0.3f, 0.3f, 0.35f);
    glm::vec3 hullColorFar = glm::vec3(0.5f, 0.5f, 0.6f);
    
    // ===== ДИСТАНЦИИ =====
    struct Distances {
        float close = 500.0f;
        float medium = 5000.0f;
        float far = 20000.0f;
        float horizon = 40000.0f;
    } dist;
    
    // ===== СЕТКА (для корпуса) =====
    struct Grid {
        bool enabled = false;
        float cellSize = 100.0f;
        float lineWidth = 1.0f;
        glm::vec3 lineColor = glm::vec3(0.93f, 0.93f, 0.94f);
        float lineAlpha = 0.3f;
        float glow = 0.2f;
        glm::vec3 cellSizeXYZ = glm::vec3(100.0f);
        glm::vec3 offset = glm::vec3(0.0f);
    } grid;
    
    // ===== РЕБРА =====
    glm::vec3 edgeColor = glm::vec3(1.0f, 1.0f, 1.0f);
    float edgeAlpha = 1.0f;
    float edgeThickness = 1.0f;
    float edgeFadeStart = 500.0f;
    float edgeFadeEnd = 5000.0f;
    
    // ===== ТУМАН =====
    struct Fog {
        bool enabled = true;
        float startDistance = 300.0f;
        float endDistance = 3000.0f;
        glm::vec3 nearColor = glm::vec3(0.05f, 0.05f, 0.1f);
        glm::vec3 farColor = glm::vec3(0.0f, 0.0f, 0.02f);
        float intensity = 0.5f;
    } fog;

    // ============================================================
    // UNIFORM APPLICATORS
    // ============================================================
    
    void applyLargeObjectUniforms(GLuint shader, const glm::mat4& mvp, 
                           const glm::mat4& model, const glm::vec3& cameraPos) const {
        glUseProgram(shader);
     
        // Матрицы
        setUniformMat4(shader, "MVP", mvp);
        setUniformMat4(shader, "M", model);
        
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
        setUniformMat3(shader, "normalMat", normalMat);
        
        // Камера
        setUniformVec3(shader, "cameraPos", cameraPos);
        
        // Освещение
        setUniformVec3(shader, "lightDir", lightDir);
        setUniformVec3(shader, "lightColor", lightColor);
        setUniformVec3(shader, "ambientColor", ambientColor);
        
        // Цвета корпуса
        setUniformVec3(shader, "hullColor", hullColor);
        setUniformVec3(shader, "detailColor", detailColor);
        setUniformVec3(shader, "glowColor", glowColor);
        
        // Эффекты поверхности
        setUniform1f(shader, "fresnelPower", fresnelPower);
        setUniform1f(shader, "rimIntensity", rimIntensity);
        setUniform1f(shader, "edgeIntensity", edgeIntensity);
        setUniform1f(shader, "normalBlend", normalBlend);
        
        // Дистанции (для будущего использования)
        setUniform1f(shader, "distClose", dist.close);
        setUniform1f(shader, "distMedium", dist.medium);
        setUniform1f(shader, "distFar", dist.far);
        setUniform1f(shader, "distHorizon", dist.horizon);
        
        // Цвета корпуса по зонам (для будущего использования)
        setUniformVec3(shader, "hullColorNear", hullColorNear);
        setUniformVec3(shader, "hullColorMid", hullColorMid);
        setUniformVec3(shader, "hullColorFar", hullColorFar);
        
        // Сетка
        setUniform1i(shader, "gridEnabled", grid.enabled);
        setUniform1f(shader, "gridCellSize", grid.cellSize);
        setUniform1f(shader, "gridLineWidth", grid.lineWidth);
        setUniformVec3(shader, "gridLineColor", grid.lineColor);
        setUniform1f(shader, "gridLineAlpha", grid.lineAlpha);
        setUniform1f(shader, "gridGlow", grid.glow);
        setUniformVec3(shader, "gridCellSizeXYZ", grid.cellSizeXYZ);
        setUniformVec3(shader, "gridOffset", grid.offset);
        
        // Туман
        setUniform1i(shader, "fogEnabled", fog.enabled);
        setUniform1f(shader, "fogStart", fog.startDistance);
        setUniform1f(shader, "fogEnd", fog.endDistance);
        setUniformVec3(shader, "fogNearColor", fog.nearColor);
        setUniformVec3(shader, "fogFarColor", fog.farColor);
        setUniform1f(shader, "fogIntensity", fog.intensity);
    }
    

    void applyEdgeUniforms(GLuint shader, const glm::mat4& mvp, 
                           const glm::vec3& cameraPos,
                           int viewportWidth, int viewportHeight) const {
        glUseProgram(shader);
        
        setUniformMat4(shader, "MVP", mvp);
        setUniform2f(shader, "viewport", (float)viewportWidth, (float)viewportHeight);
        setUniformVec3(shader, "cameraPos", cameraPos);
        
        // Параметры ребер
        setUniformVec3(shader, "color", edgeColor);
        setUniform1f(shader, "alpha", edgeAlpha);
        setUniform1f(shader, "thickness", edgeThickness);
        setUniform1f(shader, "fadeStart", edgeFadeStart);
        setUniform1f(shader, "fadeEnd", edgeFadeEnd);
    }



    void applyShipUniforms(GLuint shader, const glm::mat4& mvp, 
            const glm::mat4& model, const glm::vec3& cameraPos) const {
                glUseProgram(shader);

   
                // Матрицы
                setUniformMat4(shader, "MVP", mvp);
                setUniformMat4(shader, "M", model);
                
                glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
                setUniformMat3(shader, "normalMat", normalMat);
                
                // Камера
                setUniformVec3(shader, "cameraPos", cameraPos);
                
                // Освещение
                setUniformVec3(shader, "lightDir", lightDir);
                setUniformVec3(shader, "lightColor", lightColor);
                setUniformVec3(shader, "ambientColor", ambientColor);
                
                // Цвета корпуса для mesh_fill
                setUniformVec3(shader, "hullColor", hullColor);
                setUniformVec3(shader, "detailColor", detailColor);
                setUniformVec3(shader, "glowColor", glowColor);
                
                // Эффекты поверхности
                setUniform1f(shader, "fresnelPower", fresnelPower);
                setUniform1f(shader, "rimIntensity", rimIntensity);
                setUniform1f(shader, "edgeIntensity", edgeIntensity);
                setUniform1f(shader, "normalBlend", normalBlend);
                
                // Туман
                setUniform1i(shader, "fogEnabled", fog.enabled);
                setUniform1f(shader, "fogStart", fog.startDistance);
                setUniform1f(shader, "fogEnd", fog.endDistance);
                setUniformVec3(shader, "fogNearColor", fog.nearColor);
                setUniformVec3(shader, "fogFarColor", fog.farColor);
                setUniform1f(shader, "fogIntensity", fog.intensity);
            }

private:
    void setUniform1i(GLuint shader, const char* name, int value) const {
        GLint loc = glGetUniformLocation(shader, name);
        if (loc != -1) glUniform1i(loc, value);
    }
    
    void setUniform1f(GLuint shader, const char* name, float value) const {
        GLint loc = glGetUniformLocation(shader, name);
        if (loc != -1) glUniform1f(loc, value);
    }
    
    void setUniform2f(GLuint shader, const char* name, float x, float y) const {
        GLint loc = glGetUniformLocation(shader, name);
        if (loc != -1) glUniform2f(loc, x, y);
    }
    
    void setUniformVec3(GLuint shader, const char* name, const glm::vec3& value) const {
        GLint loc = glGetUniformLocation(shader, name);
        if (loc != -1) glUniform3fv(loc, 1, glm::value_ptr(value));
    }
    
    void setUniformMat3(GLuint shader, const char* name, const glm::mat3& value) const {
        GLint loc = glGetUniformLocation(shader, name);
        if (loc != -1) glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(value));
    }
    
    void setUniformMat4(GLuint shader, const char* name, const glm::mat4& value) const {
        GLint loc = glGetUniformLocation(shader, name);
        if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
    }

public:
    // =================== PRESETS ====================
    
    
    
    static LightingParams asteroid() {
        LightingParams p;
        
        p.dist.close = 300.0f;
        p.dist.medium = 3000.0f;
        p.dist.far = 10000.0f;
        
        p.hullColorNear = glm::vec3(0.5f, 0.4f, 0.3f);
        p.hullColorMid = glm::vec3(0.6f, 0.5f, 0.4f);
        p.hullColorFar = glm::vec3(0.7f, 0.6f, 0.5f);
        
        p.grid.enabled = true;
        p.grid.lineColor = glm::vec3(0.6f, 0.5f, 0.4f);
        p.grid.cellSize = 80.0f;
        p.grid.lineAlpha = 0.2f;
        
        return p;
    }
    
    static LightingParams planet() {
        LightingParams p;
        
        p.dist.close = 2000.0f;
        p.dist.medium = 20000.0f;
        p.dist.far = 100000.0f;
        p.dist.horizon = 200000.0f;
        
        p.hullColorNear = glm::vec3(0.2f, 0.3f, 0.6f);
        p.hullColorMid = glm::vec3(0.3f, 0.4f, 0.7f);
        p.hullColorFar = glm::vec3(0.4f, 0.5f, 0.8f);
        
        p.grid.enabled = false;
        
        return p;
    }


    static LightingParams station() {
    LightingParams p;
    
    // ===== ОСВЕЩЕНИЕ =====
    p.lightDir = glm::vec3(0.4f, 0.7f, 0.5f);
    p.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    p.ambientColor = glm::vec3(1.0f, 0.3f, 0.35f);
    
    // ===== ДИСТАНЦИИ =====
    p.dist.close = 500.0f;
    p.dist.medium = 5000.0f;
    p.dist.far = 100000.0f;
    p.dist.horizon = 200000.0f;
    
    // ===== ЦВЕТА КОРПУСА ПО ЗОНАМ =====
    p.hullColorNear = glm::vec3(0.1f, 0.1f, 0.14f);
    p.hullColorMid = glm::vec3(0.3f, 0.3f, 0.35f);
    p.hullColorFar = glm::vec3(0.5f, 0.5f, 0.6f);

    // ===== РЕБРА (для edge-шейдера) =====
    p.edgeColor = glm::vec3(0.50f, 0.50f, 0.55f);  // светло-серые
    p.edgeAlpha = 1.0f;
    p.edgeThickness = 1.0f;
    p.edgeFadeStart = 500.0f;
    p.edgeFadeEnd = 7000.0f;
    
    // ===== СЕТКА =====
    p.grid.enabled = false;        // по умолчанию выключена
    p.grid.lineColor = glm::vec3(0.4f, 0.4f, 0.5f);
    p.grid.cellSizeXYZ = glm::vec3(100.0f);
    p.grid.lineWidth = 1.0f;
    p.grid.lineAlpha = 0.3f;
    p.grid.glow = 0.2f;
    
    // ===== ТУМАН =====
    p.fog.enabled = true;
    p.fog.startDistance = 100.0f;
    p.fog.endDistance = 60000.0f;
    p.fog.nearColor = glm::vec3(0.03f, 0.03f, 0.05f);
    p.fog.farColor = glm::vec3(0.0f, 0.0f, 0.02f);
    p.fog.intensity = 0.5f;
    
    // Эти параметры не используются в large_object_fill, но могут быть нужны для других шейдеров
    p.hullColor = glm::vec3(0.8f, 0.8f, 0.9f);  // дефолт
    
    return p;
}




static LightingParams ship() {
    LightingParams p;
    
    // ===== ОСВЕЩЕНИЕ =====
    p.lightDir = glm::vec3(0.4f, 0.7f, 0.5f);
    p.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);  // пока оставим белый свет для тестов
    p.ambientColor = glm::vec3(1.0f, 0.3f, 0.35f);
    
    // ===== ЦВЕТ КОРПУСА =====
    p.hullColor = glm::vec3(0.04f, 0.04f, 0.08f);  // основной цвет
    
    // ===== РЕБРА (для edge-шейдера) =====
    p.edgeColor = glm::vec3(0.50f, 0.5f, 0.55f);
    p.edgeAlpha = 1.0f;
    p.edgeThickness = 1.0f;
    p.edgeFadeStart = 100.0f;
    p.edgeFadeEnd = 1000.0f;

    // ===== ЭФФЕКТЫ ПОВЕРХНОСТИ =====
    p.fresnelPower = 2.0f;
    p.rimIntensity = 0.5f;
    p.normalBlend = 0.15f;
    
    // ===== ТУМАН =====
    p.fog.enabled = true;
    p.fog.startDistance = 100.0f;
    p.fog.endDistance = 3000.0f;
    p.fog.nearColor = glm::vec3(0.05f, 0.05f, 0.1f);
    p.fog.farColor = glm::vec3(0.0f, 0.0f, 0.02f);
    p.fog.intensity = 0.5f;
    
    
    return p;
}
    
};