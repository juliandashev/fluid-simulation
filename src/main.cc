#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "defs.hpp"
#include "particle.hpp"
#include "renderer.hpp"
#include "simulation.hpp"

#include <cstdlib>
#include <vector>
#include <iostream>

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialise GLFW\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Fluid Simulation", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to load OpenGL via GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::vector<Particle> particles;

    // Draw particles in a grid
    for (uint32_t y = 0; y < PARTICLE_GRID; y++) {
        for (uint32_t x = 0; x < PARTICLE_GRID; x++) {
            Particle p{};
            p.position =
                glm::vec2(x * PARTICLE_SPACING - (PARTICLE_GRID * PARTICLE_SPACING / 2.0f),
                          y * PARTICLE_SPACING - (PARTICLE_GRID * PARTICLE_SPACING / 2.0f));
            particles.push_back(p);
        }
    }

    // Scoped so Renderer's destructor runs (and frees its GL objects)
    // while the GL context still exists
    {
        Renderer renderer(particles.size());
        Simulation sim(particles);

        while (!glfwWindowShouldClose(window)) {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            sim.step();
            renderer.render(particles);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
