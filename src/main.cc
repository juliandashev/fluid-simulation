#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "defs.hpp"
#include "particle.hpp"
#include "renderer.hpp"
#include "simulation.hpp"

#include <glm/geometric.hpp>

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

    // Scoped so Renderer's destructor runs (and frees its GL objects)
    // while the GL context still exists
    {
        Simulation sim(particles);
        sim.create_particles(1337);

        Renderer renderer(particles.size());

        while (!glfwWindowShouldClose(window)) {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            sim.simulation_step();

            renderer.draw_density_field(particles);  // background (clears the screen)
            renderer.render(particles);              // particles on top

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
