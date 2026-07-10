#pragma once

#include <glad/gl.h>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>

class Shader {
public:
    explicit Shader(const std::string& compute_path);
    Shader(const std::string& vert_path, const std::string& frag_path);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    void set_float(const char* name, float value) const;
    void set_uint(const char* name, uint32_t value) const;
    void set_vec2(const char* name, const glm::vec2& v) const;
    void set_vec3(const char* name, const glm::vec3& v) const;
    void set_vec4(const char* name, const glm::vec4& v) const;
    void set_mat4(const char* name, const glm::mat4& m) const;
    void set_int(const char* name, int value) const;
    void set_vec2_array(const char* name, const glm::vec2* data, int count) const;
    void set_float_array(const char* name, const float* data, int count) const;

private:
    GLuint id_;

    static GLuint compile(GLenum type, const std::string& src);
    static std::string read_file(const std::string& path);
};
