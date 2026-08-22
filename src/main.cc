#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "dam_break.hpp"
#include "debug_gui.hpp"
#include "defs.hpp"
#include "eos.hpp"
#include "history.hpp"
#include "input.hpp"
#include "renderer.hpp"
#include "simulation.hpp"
#include "logger.hpp"

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

    // vsync, locks to monitor refresh rate, no tearing
    glfwSwapInterval(1);

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

    // Scoped so Renderer's destructor runs (and frees its GL objects)
    // while the GL context still exists
    {
        SimParams params;

        Simulation sim(NUM_PARTICLES);

        bool testing = false;

        Logger logger("run.csv");
        DamBreakLogger dam_logger(dam_break_filename(params));

        // One spawn path for startup and reset, so R cannot drift from launch.
        auto spawn = [&] {
            if (DAM_BREAK_MODE) {
                sim.spawn_dam_break(params.target_density, DAM_ASPECT);
                dam_logger.restart(dam_break_filename(params));

                // Ritter front speed against c: checks the scene is still
                // weakly compressible. Reprinted per reset; the panel moves k.
                const float_t u_ritter =
                    2.0f * std::sqrt(std::abs(params.gravity) * sim.column_height());
                std::cout << "  c = " << sound_speed(params) << ", Ritter front speed = "
                          << u_ritter << " -> Mach " << mach_number(u_ritter, params)
                          << " (weakly-compressible SPH wants <= 0.1)\n";
            } else {
                sim.spawn_particles(testing, params.target_density);
            }
        };

        spawn();

        Renderer renderer(NUM_PARTICLES);

        // Live parameter panel. RAII: owns the ImGui context for this scope, so
        // it tears down while the GL context is still current (like Renderer).
        DebugGui gui(window);

        // Controlling space
        Input input(window);
        History history(MAX_HISTORY, NUM_PARTICLES);

        // Benchmark mode starts paused: the logger names its file from the
        // panel at spawn, so stepping immediately writes a stray default run.
        bool paused = DAM_BREAK_MODE;
        if (paused) {
            std::cout << "Benchmark mode: paused. Set the panel, press R to spawn, "
                         "space to run.\n";
        }

        // Frame calculation variables
        double_t now = 0.0;
        double_t previous = glfwGetTime();
        double_t accumulator = 0.0;
        double_t frame_time = 0.0;

        double_t fps_timer = glfwGetTime();
        double_t elapsed = 0;
        double_t fps = 0;
        double_t sim_time = 0.0;

        int32_t frame_count = 0;
        uint64_t step_count = 0;
        bool acoustic_warned = false;

        constexpr double DUMP_AT = 1e30;  // off; set a sim_time to snapshot particles.csv
        bool dumped = false;

        // Refreshed only when a step will consume it; already one frame stale by design.
        glm::vec2 kin(0.0f);

        // Refreshed on the measurement cadence; starts at rest.
        DensityStats rho_stats{};
        rho_stats.min = rho_stats.median = rho_stats.p90 = rho_stats.max =
            params.target_density;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            gui.begin_frame();
            gui.draw_params(params);

            bool toggle = input.is_space_key_pressed();
            bool step_fwd = input.is_right_arrow_key_pressed();
            bool step_back = input.is_left_arrow_key_pressed();
            bool reset = input.is_R_key_pressed();

            now = glfwGetTime();
            frame_time = now - previous;
            previous = now;

            if (frame_time > 0.25) {
                frame_time = 0.25;
            }

            if (toggle) {
                paused = !paused;
            }

            if (reset) {
                spawn();
                history.clear();
                accumulator = 0.0;
                sim_time = 0.0;
            }

            bool interacting = !gui.wants_mouse() && (input.is_left_mouse_button_down() ||
                                                      input.is_right_mouse_button_down());
            glm::vec2 world(0.0f);

            if (interacting) {
                int32_t w, h;
                glfwGetWindowSize(window, &w, &h);

                world = screen_to_world(input.cursor_pixels(), w, h);

                float_t strength = input.is_left_mouse_button_down() ? INTERACTION_STRENGTH
                                                                     : -INTERACTION_STRENGTH;

                sim.set_interaction(world, strength);
            } else {
                sim.set_interaction(glm::vec2(0.0f), 0.0f);
            }

            float_t dt = params.dt;
            float_t time_scale = params.time_scale;

            if (dt < DT_MIN) {
                dt = DT_MIN;  // a typo'd 0 would stall the loop; also keeps clamp's lo <= hi
            }

            // Three CFL conditions: velocity, acceleration, acoustic.
            // params.dt is the ceiling.
            if (!paused || step_fwd) {
                kin = sim.max_kinematics();  // x: max speed, y: max accel
            }

            float_t cfl_dt = dt;
            if (kin.x > 1e-6f) {  // at rest the divisions blow up
                cfl_dt = CFL_LAMBDA * KERNEL_RADIUS / kin.x;
            }
            if (kin.y > 1e-6f) {
                cfl_dt = std::min(cfl_dt, CFL_LAMBDA_FORCE * std::sqrt(KERNEL_RADIUS / kin.y));
            }

            // Set by EOS stiffness, not the flow. Takes the densest particle
            // of the last sample, floored at rho_0.
            const float_t rho_for_cfl = std::max(params.target_density, rho_stats.max);

            const float_t acoustic_dt =
                CFL_LAMBDA_SOUND * KERNEL_RADIUS / sound_speed_at(params, rho_for_cfl);
            cfl_dt = std::min(cfl_dt, acoustic_dt);

            dt = std::clamp(cfl_dt, DT_MIN, dt);

            // DT_MIN wins the clamp, so a stiff EOS is silently under-resolved.
            if (acoustic_dt < DT_MIN && !acoustic_warned) {
                acoustic_warned = true;
                std::cout << "WARNING: acoustic CFL wants dt <= " << acoustic_dt
                          << " but DT_MIN is " << DT_MIN << ". Pressure waves are under-resolved;"
                          << " lower DT_MIN or the pressure multiplier.\n";
            }

            if (!paused) {
                accumulator += frame_time * time_scale;

                int32_t steps = 0;
                while (accumulator >= dt && steps < MAX_SUBSTEPS) {
                    history.save(sim.position_buffer(), sim.velocity_buffer(), sim.speed_buffer());
                    sim.step(dt, params);
                    accumulator -= dt;
                    ++steps;

                    sim_time += dt;
                    if (step_count++ % 4 == 0) {
                        rho_stats = sim.density_stats();
                        logger.log(step_count, sim_time, dt, kin.x, kin.y, rho_stats,
                                   sim.accel_stats(params.gravity));

                        if (!dumped && sim_time >= DUMP_AT) {
                            dumped = true;
                            sim.dump_particles("particles.csv", params.gravity);
                        }

                        // Stalls on a readback; rides the CSV cadence.
                        if (DAM_BREAK_MODE) {
                            const float_t origin = sim.column_origin_x();
                            dam_logger.log(sim_time,
                                           surge_front(sim.read_positions(), origin), origin,
                                           sim.column_width(), params);
                        }
                    }
                }

                if (accumulator >= dt) {
                    accumulator = 0.0f;
                }

            } else {
                if (step_fwd) {
                    history.save(sim.position_buffer(), sim.velocity_buffer(), sim.speed_buffer());
                    sim.step(dt, params);
                }

                if (step_back) {
                    history.restore(sim.position_buffer(), sim.velocity_buffer(),
                                    sim.speed_buffer());
                }
            }

            glClearColor(0.02f, 0.04f, 0.10f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            renderer.render(sim.position_buffer(), sim.speed_buffer(), sim.count());

            if (interacting) {
                renderer.draw_circle(world, INTERACTION_RADIUS, glm::vec3(1.0f, 0.2f, 0.2f));
            }

            gui.end_frame();

            glfwSwapBuffers(window);

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
