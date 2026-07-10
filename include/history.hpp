#pragma once

#include <glad/gl.h>

#include "defs.hpp"

// GPU-resident snapshot ring: a bounded stack of (position, velocity, speed)
// copies taken with glCopyBufferSubData, so snapshots never leave the GPU.
// Ring bookkeeping (head_/count_) lives on the CPU. Requires a current GL
// context.
class History {
public:
    History(uint32_t max_snapshots, uint32_t particle_count)
        : max_(max_snapshots),
          vec2_bytes_(particle_count * sizeof(float_t) * 2),
          float_bytes_(particle_count * sizeof(float_t)) {
        glGenBuffers(1, &pos_history_);
        glBindBuffer(GL_COPY_WRITE_BUFFER, pos_history_);
        glBufferData(GL_COPY_WRITE_BUFFER, max_ * vec2_bytes_, nullptr, GL_DYNAMIC_COPY);

        glGenBuffers(1, &vel_history_);
        glBindBuffer(GL_COPY_WRITE_BUFFER, vel_history_);
        glBufferData(GL_COPY_WRITE_BUFFER, max_ * vec2_bytes_, nullptr, GL_DYNAMIC_COPY);

        glGenBuffers(1, &speed_history_);
        glBindBuffer(GL_COPY_WRITE_BUFFER, speed_history_);
        glBufferData(GL_COPY_WRITE_BUFFER, max_ * float_bytes_, nullptr, GL_DYNAMIC_COPY);

        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    ~History() {
        glDeleteBuffers(1, &pos_history_);
        glDeleteBuffers(1, &vel_history_);
        glDeleteBuffers(1, &speed_history_);
    }

    History(const History&) = delete;
    History& operator=(const History&) = delete;

    void save(GLuint pos, GLuint vel, GLuint speed) {
        copy(pos, pos_history_, 0, head_ * vec2_bytes_, vec2_bytes_);
        copy(vel, vel_history_, 0, head_ * vec2_bytes_, vec2_bytes_);
        copy(speed, speed_history_, 0, head_ * float_bytes_, float_bytes_);

        head_ = (head_ + 1) % max_;
        if (count_ < max_) {
            count_++;
        }
    }

    void restore(GLuint pos, GLuint vel, GLuint speed) {
        if (count_ == 0) {
            return;
        }
        head_ = (head_ + max_ - 1) % max_;
        count_--;

        copy(pos_history_, pos, head_ * vec2_bytes_, 0, vec2_bytes_);
        copy(vel_history_, vel, head_ * vec2_bytes_, 0, vec2_bytes_);
        copy(speed_history_, speed, head_ * float_bytes_, 0, float_bytes_);
    }

    void clear() {
        head_ = 0;
        count_ = 0;
    }

private:
    // GPU->GPU memcpy; never touches CPU memory.
    static void copy(GLuint src, GLuint dst, GLintptr src_offset, GLintptr dst_offset,
                     GLsizeiptr bytes) {
        glBindBuffer(GL_COPY_READ_BUFFER, src);
        glBindBuffer(GL_COPY_WRITE_BUFFER, dst);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, src_offset, dst_offset,
                            bytes);
    }

    GLuint pos_history_ = 0;
    GLuint vel_history_ = 0;
    GLuint speed_history_ = 0;

    uint32_t head_ = 0;
    uint32_t count_ = 0;
    uint32_t max_;
    GLsizeiptr vec2_bytes_;   // one snapshot's positions (or velocities) in bytes
    GLsizeiptr float_bytes_;  // one snapshot's speeds in bytes
};
