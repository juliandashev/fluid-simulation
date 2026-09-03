#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <iostream>
#include <vector>

#include "bindings.hpp"
#include "dam_break.hpp"
#include "debug_gui.hpp"
#include "defs.hpp"
#include "eos.hpp"
#include "experiment.hpp"
#include "field_view.hpp"
#include "history.hpp"
#include "input.hpp"
#include "renderer.hpp"
#include "simulation.hpp"
#include "logger.hpp"
#include "obstacle.hpp"
#include "pipe.hpp"
#include "wing.hpp"
#include "profile.hpp"

namespace fluid {

// convert pixels (top-left, y-down) to world uints
// inverting the vertex shader's transform
glm::vec2 screen_to_world(glm::vec2 px, int32_t w, int32_t h) {
    float_t aspect = static_cast<float_t>(w) / h;
    float_t ndc_x = (px.x / w) * 2.0f - 1.0f;
    float_t ndc_y = 1.0f - (px.y / h) * 2.0f;
    return glm::vec2(ndc_x * aspect, ndc_y) * DOMAIN_MAX;  // mirror the shader's x-correction
}

}  // namespace fluid

int main(int argc, char** argv) {
    using namespace fluid;

    Experiment experiment = Experiment::Sandbox;
    int32_t particles_override = 0;
    switch (parse_args(argc, argv, experiment, particles_override)) {
        case ArgsResult::Exit:  return EXIT_SUCCESS;
        case ArgsResult::Error: return EXIT_FAILURE;
        case ArgsResult::Ok:    break;
    }

    const ExperimentSpec* scene = &spec_of(experiment);
    Resolution res = resolution_of(*scene, particles_override);

    // Reprinted on every scene load, so the console tracks what is on screen.
    auto print_scene = [&] {
        std::cout << "Experiment: " << scene->name << " (" << scene->summary << ")\n"
                  << "  n_x = " << res.particles_across << ", " << res.count
                  << " particles, dx = " << res.spacing << ", h = " << res.kernel_radius
                  << "\n";
    };

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
        bool testing = false;

        log::Logger logger("run.csv");

        // Every one of these is sized from Resolution, which changes with the
        // scene - so a scene load destroys and rebuilds them rather than resizing.
        std::optional<Simulation> sim;
        std::optional<gl::Renderer> renderer;
        std::optional<gl::History> history;
        std::optional<dam_break::Logger> dam_logger;
        std::optional<profile::Logger> profile_logger;

        std::vector<obstacle::Quad> obstacles;
        float_t built_aoa = 0.0f;
        float_t built_chord = 0.0f;
        bool paused = false;

        // Rebuilt on spawn, and again whenever the panel moves the wing angle -
        // the solid grid has to be regenerated with it.
        auto build_geometry = [&] {
            std::vector<glm::vec2> solids;

            switch (experiment) {
            case Experiment::DamBreakObstacle:
                obstacles = dam_break::block(res);
                break;
            case Experiment::Pipe:
                obstacles = pipe::geometry(res);
                break;
            case Experiment::Wing: {
                // Filled as one polygon; a fan of quads would double up
                // particles along every shared edge.
                const obstacle::Poly w = wing::section(params.wing_aoa_deg, params.wing_chord_frac,
                                                     wing::TE_MIN_HALF * res.kernel_radius);
                obstacles = obstacle::to_quads(w);
                solids = obstacle::to_particles(w, res);
                break;
            }
            default:
                obstacles.clear();
                break;
            }

            if (experiment != Experiment::Wing) {
                solids = obstacle::to_particles(obstacles, res);
            }

            sim->spawn_solids(solids);
            built_aoa = params.wing_aoa_deg;
            built_chord = params.wing_chord_frac;
            return solids;
        };

        // One spawn path for startup and reset, so R cannot drift from launch.
        auto spawn = [&] {
            const std::vector<glm::vec2> solid_particles = build_geometry();

            switch (experiment) {
            case Experiment::Wing: {
                // The gas fills the whole visible domain, so the only walls are
                // the domain's own; free-slip, or they would drag the free stream.
                const float_t x = 0.5f * res.period_x;

                sim->set_periodic_x(res.period_x);
                sim->set_wall_slip(glm::vec2(1.0f, 1.0f));
                sim->spawn_at(obstacle::fill_region(glm::vec2(-x, DOMAIN_MIN + EPS),
                                                   glm::vec2(x, DOMAIN_MAX - EPS),
                                                   solid_particles, sim->count(), res));

                const float_t thickness = wing::thickness(params.wing_chord_frac);

                std::cout << "  c = " << sound_speed(params) << "; Mach 0.1 is "
                          << 0.1f * sound_speed(params) << "\n"
                          << "  wing thickness " << thickness << " = "
                          << thickness / res.kernel_radius << " h";

                if (thickness < res.kernel_radius) {
                    std::cout << " -- below h, the gas will leak through; raise --particles";
                }
                std::cout << "\n";
                break;
            }
            case Experiment::Pipe: {
                const float_t h = pipe::HALF_HEIGHT;
                const float_t x = 0.5f * res.period_x;

                sim->set_periodic_x(res.period_x);
                sim->spawn_at(obstacle::fill_region(glm::vec2(-x, -h), glm::vec2(x, h),
                                                    solid_particles, sim->count(), res));
                profile_logger->restart(profile::filename(scene->name, params));

                std::cout << "  c = " << sound_speed(params) << "; Mach 0.1 is "
                          << 0.1f * sound_speed(params)
                          << ". Body force " << params.body_accel_x
                          << " settles the throat near Mach "
                          << 0.16f * params.body_accel_x << ".\n"
                          << "  Faster flow needs a higher pressure multiplier too, or it "
                             "goes transonic and the throat chokes.\n";
                break;
            }
            case Experiment::DamBreak:
            case Experiment::DamBreakObstacle: {
                sim->spawn_dam_break(params.target_density);
                dam_logger->restart(dam_break::filename(params));
                profile_logger->restart(profile::filename(scene->name, params));

                // Ritter front speed against c: checks the scene is still
                // weakly compressible. Reprinted per reset; the panel moves k.
                const float_t u_ritter =
                    2.0f * std::sqrt(std::abs(params.gravity) * sim->column_height());
                std::cout << "  c = " << sound_speed(params) << ", Ritter front speed = "
                          << u_ritter << " -> Mach " << mach_number(u_ritter, params)
                          << " (weakly-compressible SPH wants <= 0.1)\n";
                break;
            }
            case Experiment::Sandbox:
                sim->spawn_particles(testing, params.target_density);
                break;
            }
        };

        // Full teardown and rebuild: a scene brings its own Resolution, and the
        // panel defaults are per-scene too, so nothing carries over but the window.
        auto load_scene = [&](Experiment next) {
            experiment = next;
            scene = &spec_of(experiment);
            res = resolution_of(*scene, particles_override);

            params = SimParams{};
            configure(experiment, params);

            sim.emplace(res);
            renderer.emplace(res.count);
            history.emplace(MAX_HISTORY, res.count);
            dam_logger.emplace(dam_break::filename(params));
            profile_logger.emplace(profile::filename(scene->name, params), pipe::duct(res));

            built_aoa = 0.0f;
            built_chord = 0.0f;

            // Benchmark mode starts paused: the logger names its file from the
            // panel at spawn, so stepping immediately writes a stray default run.
            paused = scene->starts_paused;

            print_scene();
            spawn();

            if (paused) {
                std::cout << "Benchmark mode: paused. Set the panel, press R to spawn, "
                             "space to run.\n";
            }
        };

        load_scene(experiment);
        ui::print_bindings();

        // Live parameter panel. RAII: owns the ImGui context for this scope, so
        // it tears down while the GL context is still current (like Renderer).
        ui::DebugGui gui(window);

        // Controlling space
        ui::Input input(window);
        gl::ColorField color_field = gl::ColorField::Speed;
        bool show_params = false;
        bool show_experiments = false;
        bool field_view = false;
        bool blow_mode = false;
        const std::string controls = ui::hud_line();

        // Coarser than the window on purpose; the field is smooth and gets
        // bilinearly upscaled, so the sampling cost stays small.
        gl::FieldView field(256, 144);

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
        glm::vec3 kin(0.0f);

        // Refreshed on the measurement cadence; starts at rest.
        log::DensityStats rho_stats{};
        rho_stats.min = rho_stats.median = rho_stats.p90 = rho_stats.max =
            params.target_density;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            gui.begin_frame();

            // The panel owns the keyboard while a field has focus, or typing a
            // digit into it would trip whatever binding shares that key.
            ui::AppState keys{paused, show_params,    show_experiments,
                              field_view, blow_mode, color_field};
            ui::dispatch(input, keys, !gui.wants_keyboard());

            if (show_params) {
                gui.draw_params(params);
            }

            if (show_experiments) {
                const int32_t picked = gui.draw_experiments(static_cast<int32_t>(experiment));

                if (picked >= 0) {
                    load_scene(static_cast<Experiment>(picked));
                    accumulator = 0.0;
                    sim_time = 0.0;
                }
            }

            gui.draw_controls(controls.c_str());
            gui.draw_legend(color_field, params);

            const bool step_fwd = keys.step_fwd;
            const bool step_back = keys.step_back;
            const bool reset = keys.reset;

            now = glfwGetTime();
            frame_time = now - previous;
            previous = now;

            if (frame_time > 0.25) {
                frame_time = 0.25;
            }

            if (experiment == Experiment::Wing &&
                (params.wing_aoa_deg != built_aoa || params.wing_chord_frac != built_chord)) {
                build_geometry();
            }

            if (reset) {
                spawn();
                history->clear();
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

                const bool pulling = input.is_left_mouse_button_down();

                if (blow_mode && pulling) {
                    // A jet along +x rather than a pull toward the cursor: held
                    // over one surface it imposes the asymmetry that circulation
                    // would otherwise have to build. See README.
                    sim->set_interaction(world, params.blow_speed, glm::vec2(1.0f, 0.0f));
                } else {
                    sim->set_interaction(world,
                                         pulling ? INTERACTION_STRENGTH : -INTERACTION_STRENGTH);
                }
            } else {
                sim->set_interaction(glm::vec2(0.0f), 0.0f);
            }

            float_t dt = params.dt;
            float_t time_scale = params.time_scale;

            if (dt < DT_MIN) {
                dt = DT_MIN;  // a typo'd 0 would stall the loop; also keeps clamp's lo <= hi
            }

            // Three CFL conditions: velocity, acceleration, acoustic.
            // params.dt is the ceiling.
            if (!paused || step_fwd) {
                kin = sim->max_kinematics();  // x: max speed, y: max accel, z: mean speed
            }

            float_t cfl_dt = dt;
            if (kin.x > 1e-6f) {  // at rest the divisions blow up
                cfl_dt = CFL_LAMBDA * res.kernel_radius / kin.x;
            }
            if (kin.y > 1e-6f) {
                cfl_dt = std::min(cfl_dt, CFL_LAMBDA_FORCE * std::sqrt(res.kernel_radius / kin.y));
            }

            // Set by EOS stiffness, not the flow. Takes the densest particle
            // of the last sample, floored at rho_0.
            const float_t rho_for_cfl = std::max(params.target_density, rho_stats.max);

            const float_t acoustic_dt =
                CFL_LAMBDA_SOUND * res.kernel_radius / sound_speed_at(params, rho_for_cfl);
            cfl_dt = std::min(cfl_dt, acoustic_dt);

            dt = std::clamp(cfl_dt, DT_MIN, dt);

            // DT_MIN wins the clamp, so a stiff EOS is silently under-resolved.
            if (acoustic_dt < DT_MIN && !acoustic_warned) {
                acoustic_warned = true;
                std::cout << "WARNING: acoustic CFL wants dt <= " << acoustic_dt
                          << " but DT_MIN is " << DT_MIN << ". Pressure waves are under-resolved;"
                          << " lower DT_MIN or the pressure multiplier.\n";
            }

            // A constant body force in a periodic domain has nothing to balance
            // it once the walls are free-slip, so the flow runs away. Ease the
            // drive off as the fastest particle approaches the target.
            SimParams driven = params;
            if (params.target_speed > 0.0f) {
                // On the mean, not the maximum: the fastest particles are the
                // ones accelerating past the wing, and governing on them cuts
                // the drive for everyone - including the wake that needs it.
                driven.body_accel_x =
                    params.body_accel_x *
                    std::clamp(1.0f - kin.z / params.target_speed, 0.0f, 1.0f);
            }

            if (!paused) {
                accumulator += frame_time * time_scale;

                int32_t steps = 0;
                while (accumulator >= dt && steps < MAX_SUBSTEPS) {
                    history->save(sim->position_buffer(), sim->velocity_buffer(), sim->speed_buffer());
                    sim->step(dt, driven);
                    accumulator -= dt;
                    ++steps;

                    sim_time += dt;
                    if (step_count++ % 4 == 0) {
                        rho_stats = sim->density_stats();
                        logger.log(step_count, sim_time, dt, kin.x, kin.y, rho_stats,
                                   sim->accel_stats(params.gravity));

                        if (!dumped && sim_time >= DUMP_AT) {
                            dumped = true;
                            sim->dump_particles("particles.csv", params.gravity);
                        }

                        if (scene->logs_profile) {
                            profile_logger->log(sim_time, sim->read_positions(),
                                               sim->read_speeds(), sim->read_densities(), params);
                        }

                        // Stalls on a readback; rides the CSV cadence.
                        if (scene->logs_front) {
                            const float_t origin = sim->column_origin_x();
                            dam_logger->log(sim_time,
                                           dam_break::surge_front(sim->read_positions(), res, origin), origin,
                                           sim->column_width(), params);
                        }
                    }
                }


                if (accumulator >= dt) {
                    accumulator = 0.0f;
                }

            } else {
                if (step_fwd) {
                    history->save(sim->position_buffer(), sim->velocity_buffer(), sim->speed_buffer());
                    sim->step(dt, driven);
                }

                if (step_back) {
                    history->restore(sim->position_buffer(), sim->velocity_buffer(),
                                    sim->speed_buffer());
                }
            }

            glClearColor(0.02f, 0.04f, 0.10f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            const GLuint field_buffer = color_field == gl::ColorField::Pressure
                                            ? sim->density_buffer()
                                            : sim->speed_buffer();

            if (field_view && color_field != gl::ColorField::None) {
                field.update(*sim, params);
                field.draw(color_field, params);
            }

            // Behind the fluid, so a particle that leaks in is still visible.
            renderer->draw_quads(obstacles);
            renderer->render(sim->position_buffer(), field_buffer, color_field, params, sim->count());

            if (interacting) {
                const bool blowing = blow_mode && input.is_left_mouse_button_down();

                renderer->draw_circle(world, INTERACTION_RADIUS,
                                      blowing ? glm::vec3(0.35f, 0.8f, 1.0f)
                                              : glm::vec3(1.0f, 0.2f, 0.2f));
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
