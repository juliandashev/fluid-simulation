#include "shader.hpp"

#include <cstdlib>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <sstream>

const uint16_t LOG_SIZE = 512;

std::string Shader::read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Cannot open shader: " << path << "\n";
        std::exit(EXIT_FAILURE);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint Shader::compile(GLenum type, const std::string& src) {
    GLuint shader = glCreateShader(type);
    const char* c_src = src.c_str();
    glShaderSource(shader, 1, &c_src, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[LOG_SIZE];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        std::cerr << "Shader compile error:\n" << log << "\n";
        std::exit(EXIT_FAILURE);
    }
    return shader;
}

Shader::Shader(const std::string& vert_path, const std::string& frag_path) {
    GLuint vert = compile(GL_VERTEX_SHADER, read_file(vert_path));
    GLuint frag = compile(GL_FRAGMENT_SHADER, read_file(frag_path));

    id_ = glCreateProgram();
    glAttachShader(id_, vert);
    glAttachShader(id_, frag);
    glLinkProgram(id_);

    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok;
    glGetProgramiv(id_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[LOG_SIZE];
        glGetProgramInfoLog(id_, sizeof(log), nullptr, log);
        glDeleteProgram(id_);
        id_ = 0;
        std::cerr << "Shader link error:\n" << log << "\n";
        std::exit(EXIT_FAILURE);
    }
}

Shader::~Shader() {
    if (id_) glDeleteProgram(id_);
}

void Shader::use() const { glUseProgram(id_); }

void Shader::set_float(const char* name, float value) const {
    glUniform1f(glGetUniformLocation(id_, name), value);
}

void Shader::set_vec3(const char* name, const glm::vec3& v) const {
    glUniform3fv(glGetUniformLocation(id_, name), 1, glm::value_ptr(v));
}

void Shader::set_vec4(const char* name, const glm::vec4& v) const {
    glUniform4fv(glGetUniformLocation(id_, name), 1, glm::value_ptr(v));
}

void Shader::set_mat4(const char* name, const glm::mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(id_, name), 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::set_int(const char* name, int value) const {
    glUniform1i(glGetUniformLocation(id_, name), value);
}
