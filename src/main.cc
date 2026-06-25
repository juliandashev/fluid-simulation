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
#include <random>

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
    glfwSwapInterval(0);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to load OpenGL via GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    int32_t fb_w, fb_h;
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    glViewport(0, 0, fb_w, fb_h);

    glfwSetFramebufferSizeCallback(
        window, [](GLFWwindow*, int32_t w, int32_t h) { glViewport(0, 0, w, h); });

    std::vector<Particle> particles;

    // Scoped so Renderer's destructor runs (and frees its GL objects)
    // while the GL context still exists
    {
        Simulation sim(particles);
        bool testing = false;

        if (testing) {
            std::random_device rd;
            uint32_t seed = rd();

            std::cout << "Seed: " << seed << "\n";

            // Spawn particles and sparse them randomly
            sim.create_particles(seed);
        } else {
            // Spawn particles and spawn them in a square shape
            sim.create_particles();
        }

        Renderer renderer(particles.size());

        const double_t FIXED_DT = DT;
        const int32_t MAX_STEPS = 200;  // hard cap on catch-up steps per frame
        double_t previous = glfwGetTime();
        double_t accumulator = 0.0;

        while (!glfwWindowShouldClose(window)) {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            double_t now = glfwGetTime();
            accumulator += now - previous;
            previous = now;

            int32_t steps = 0;
            while (accumulator >= FIXED_DT && steps < MAX_STEPS) {
                sim.simulation_step(FIXED_DT * 3);
                accumulator -= FIXED_DT;
                steps++;
            }

            // If we hit the cap, the renderer can't keep up at this DT. Drop the
            // backlog instead of letting it grow forever (spiral of death)
            // the sim runs in slow motion, but it stays alive and responsive.
            if (steps == MAX_STEPS) {
                accumulator = 0.0;
            }

            glClearColor(0.02f, 0.04f, 0.10f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // renderer.draw_density_field(particles);  // background (clears the screen)
            renderer.render(particles);  // particles on top

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
