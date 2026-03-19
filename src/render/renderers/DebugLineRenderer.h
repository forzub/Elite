#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <memory>

#include "src/render/ShaderLibrary.h" 


// Вспомогательная структура для хранения данных линий
struct LineVertex {
    glm::vec3 position;
    glm::vec4 color;
};





class DebugLineRenderer {
private:
    GLuint vao = 0;
    GLuint vbo = 0;
    std::vector<LineVertex> vertices;
    GLuint shaderProgram = 0;
    GLuint mvpLocation = 0;
    bool initialized = false;
    
public:
    DebugLineRenderer() {
        // Не создаем OpenGL объекты в конструкторе!
    }
    
    bool initialize() {
         if (initialized) return true;
    
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to load OpenGL functions" << std::endl;
        return false;
    }
    
    // ✅ Только загрузка из библиотеки, без хардкода
    shaderProgram = ShaderLibrary::instance().get("debug_lines");
    
    if (shaderProgram == 0) {
        std::cerr << "Failed to load debug_lines shader from library" << std::endl;
        return false;
    }
    
    mvpLocation = glGetUniformLocation(shaderProgram, "MVP");
        
        // Создаем VAO и VBO
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        
        if (!vao || !vbo) {
            std::cerr << "Failed to create VAO or VBO" << std::endl;
            return false;
        }
        
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        
        // Позиция (layout = 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
        
        // Цвет (layout = 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, color));
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        
        initialized = true;
        std::cout << "DebugLineRenderer initialized successfully" << std::endl;
        return true;
    }
    
    // Временный метод для создания шейдера отладки
    GLuint createDebugLineShader() {
        const char* vertexShaderSource = R"(
            #version 330 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec4 aColor;
            
            uniform mat4 MVP;
            
            out vec4 vColor;
            
            void main() {
                gl_Position = MVP * vec4(aPos, 1.0);
                vColor = aColor;
            }
        )";
        
        const char* fragmentShaderSource = R"(
            #version 330 core
            in vec4 vColor;
            out vec4 FragColor;
            
            void main() {
                FragColor = vColor;
            }
        )";
        
        // Используем существующую функцию compileShader из ShaderUtils
        // Но она не включена, поэтому пока оставляем локальную компиляцию
        // В идеале - добавить файлы шейдеров и загружать через InitShaders()
        
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        
        GLint success;
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
            return 0;
        }
        
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
            glDeleteShader(vertexShader);
            return 0;
        }
        
        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, NULL, infoLog);
            std::cerr << "Shader linking failed: " << infoLog << std::endl;
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            glDeleteProgram(program);
            return 0;
        }
        
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        
        return program;
    }
    
    ~DebugLineRenderer() {
        if (initialized) {
            if (vao) glDeleteVertexArrays(1, &vao);
            if (vbo) glDeleteBuffers(1, &vbo);
            // Не удаляем шейдерную программу, так как она может быть в ShaderLibrary
            // if (shaderProgram) glDeleteProgram(shaderProgram);
        }
    }
    
    void begin() {
        if (!initialized) return;
        vertices.clear();
    }
    
    void addLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color) {
        if (!initialized) return;
        vertices.push_back({start, color});
        vertices.push_back({end, color});
    }
    
    void end(const glm::mat4& mvp) {
        if (!initialized || vertices.empty()) return;
        
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));
        
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(LineVertex), vertices.data(), GL_DYNAMIC_DRAW);
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glDrawArrays(GL_LINES, 0, vertices.size());
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        
        glDisable(GL_BLEND);
    }
    
    bool isInitialized() const { return initialized; }
};