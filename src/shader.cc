#include "shader.hpp"

#include "eos.hpp"

#include <cstdlib>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <sstream>

namespace fluid {
namespace gl {

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

// #define rather than a uniform: a literal exponent strength-reduces to
// multiplies, a uniform one becomes exp2(u*log2(x)).
namespace {

std::string shader_defines() {
    std::ostringstream ss;
    ss << "#define EOS_EXPONENT " << EOS_EXPONENT << ".0\n";
    return ss.str();
}

// After #version, which must stay the literal first line the driver sees.
std::string with_defines(const std::string& src) {
    const std::size_t version_end = src.find('\n', src.find("#version"));
    return src.substr(0, version_end + 1) + shader_defines() + "#line 2\n" +
           src.substr(version_end + 1);
}

}  // namespace

// GLSL has no #include, so one is provided here. #line after each substitution
// keeps compiler errors addressable.
std::string Shader::resolve_includes(const std::string& src) {
    std::istringstream in(src);
    std::ostringstream out;
    std::string line;

    for (uint32_t number = 1; std::getline(in, line); ++number) {
        const std::size_t open = line.find("#include \"");
        if (open == std::string::npos) {
            out << line << '\n';
            continue;
        }

        const std::size_t first = open + 10;
        const std::size_t last = line.find('"', first);
        if (last == std::string::npos) {
            std::cerr << "Malformed #include: " << line << "\n";
            std::exit(EXIT_FAILURE);
        }

        out << read_file(std::string(SHADER_DIR) + "/" + line.substr(first, last - first))
            << "\n#line " << (number + 1) << '\n';
    }

    return out.str();
}

Shader::Shader(const std::string& compute_path) {
    GLuint comp = compile(GL_COMPUTE_SHADER, resolve_includes(with_defines(read_file(compute_path))));

    id_ = glCreateProgram();
    glAttachShader(id_, comp);
    glLinkProgram(id_);

    glDeleteShader(comp);

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

Shader::Shader(const std::string& vert_path, const std::string& frag_path) {
    GLuint vert = compile(GL_VERTEX_SHADER, resolve_includes(with_defines(read_file(vert_path))));
    GLuint frag = compile(GL_FRAGMENT_SHADER, resolve_includes(with_defines(read_file(frag_path))));

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

void Shader::set_vec2(const char* name, const glm::vec2& v) const {
    glUniform2f(glGetUniformLocation(id_, name), v.x, v.y);
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

void Shader::set_ivec2(const char* name, int x, int y) const {
    glUniform2i(glGetUniformLocation(id_, name), x, y);
}

void Shader::set_vec2_array(const char* name, const glm::vec2* data, int count) const {
    glUniform2fv(glGetUniformLocation(id_, name), count, glm::value_ptr(data[0]));
}

void Shader::set_float_array(const char* name, const float* data, int count) const {
    glUniform1fv(glGetUniformLocation(id_, name), count, data);
}

void Shader::set_uint(const char* name, uint32_t value) const {
    glUniform1ui(glGetUniformLocation(id_, name), value);
}

}  // namespace gl
}  // namespace fluid
