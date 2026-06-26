#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "defs.hpp"
#include "particle.hpp"
#include "renderer.hpp"
#include "simulation.hpp"
#include "input.hpp"

#include <glm/geometric.hpp>

#include <cstdlib>
#include <vector>
#include <iostream>
#include <random>
#include <deque>

// convert pixels (top-left, y-down) to world uints
// inverting the vertex shader's transform
glm::vec2 screen_to_world(glm::vec2 px, int32_t w, int32_t h) {
    float_t aspect = static_cast<float_t>(w) / h;
    float_t ndc_x = (px.x / w) * 2.0f - 1.0f;
    float_t ndc_y = 1.0f - (px.y / h) * 2.0f;
    return glm::vec2(ndc_x * aspect, ndc_y) * DOMAIN_MAX;  // mirror the shader's x-correction
}

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

        // Controlling space
        Input input(window);
        bool paused = false;
        std::deque<std::vector<Particle>> history;

        // Frame calculation variables
        double_t now = 0.0;
        double_t previous = glfwGetTime();
        double_t accumulator = 0.0;
        double_t frame_time = 0.0;

        double_t fps_timer = glfwGetTime();
        double_t elapsed = 0;
        double_t fps = 0;
        int32_t frame_count = 0;

        while (!glfwWindowShouldClose(window)) {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            bool toggle = input.space_pressed();
            bool step_fwd = input.right_pressed();
            bool step_back = input.left_pressed();

            now = glfwGetTime();
            frame_time = now - previous;
            previous = now;

            if (frame_time > 0.25) {
                frame_time = 0.25;
            }

            const double_t STEP = DT * 10.0;

            if (toggle) {
                paused = !paused;
            }

            bool interacting = input.left_mouse_held() || input.right_mouse_held();
            glm::vec2 world(0.0f);

            if (interacting) {
                int32_t w, h;
                glfwGetWindowSize(window, &w, &h);

                world = screen_to_world(input.cursor_pixels(), w, h);

                float_t strength =
                    input.left_mouse_held() ? INTERACTION_STRENGTH : -INTERACTION_STRENGTH;

                sim.set_interaction(world, strength);
            } else {
                sim.set_interaction(glm::vec2(0.0f), 0.0f);
            }

            if (!paused) {
                accumulator += frame_time * 10.0;

                while (accumulator >= STEP) {
                    history.push_back(particles);

                    if (history.size() > MAX_HISTORY) {
                        history.pop_front();
                    }

                    sim.simulation_step(STEP);
                    accumulator -= STEP;
                }
            } else {
                if (step_fwd) {
                    history.push_back(particles);
                    sim.simulation_step(STEP);
                }

                if (step_back && !history.empty()) {
                    particles = history.back();
                    history.pop_back();
                }
            }

            glClearColor(0.02f, 0.04f, 0.10f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            renderer.render(particles);

            if (interacting) {
                renderer.draw_circle(world, INTERACTION_RADIUS, glm::vec3(1.0f, 0.2f, 0.2f));
            }

            glfwSwapBuffers(window);
            glfwPollEvents();

            frame_count++;
            elapsed = glfwGetTime() - fps_timer;

            if (elapsed >= 1.0) {
                fps = frame_count / elapsed;
                glfwSetWindowTitle(window, ("Fluid Simulation - " +
                                            std::to_string(static_cast<int32_t>(fps)) + " FPS")
                                               .c_str());
                frame_count = 0;
                fps_timer = glfwGetTime();
            }
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
