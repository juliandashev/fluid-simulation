#include "debug_gui.hpp"

#include <algorithm>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace fluid {
namespace ui {

DebugGui::DebugGui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);  // true: chains our existing GLFW callbacks
    ImGui_ImplOpenGL3_Init("#version 430");      // matches the GL 4.3 core context
}

DebugGui::~DebugGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void DebugGui::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

// InputFloat takes arbitrary typed text, so every value here reaches the solver unchecked.
namespace {

void clamp_params(SimParams& params) {
    // Divisor and sqrt argument in create_column; the ceiling keeps its int32_t cast in range.
    constexpr float_t MIN_DENSITY = 0.01f;
    constexpr float_t MAX_DENSITY = 1000.0f;

    params.dt = std::max(params.dt, DT_MIN);
    params.target_density = std::clamp(params.target_density, MIN_DENSITY, MAX_DENSITY);
    params.pressure_multiplier = std::max(params.pressure_multiplier, 0.0f);
    params.viscosity = std::max(params.viscosity, 0.0f);
    params.tension_threshold = std::max(params.tension_threshold, 0.0f);
    params.tension_strength = std::max(params.tension_strength, 0.0f);
    params.cohesion_strength = std::max(params.cohesion_strength, 0.0f);
    params.time_scale = std::max(params.time_scale, 0.0f);
    params.draw_scale = std::clamp(params.draw_scale, 0.1f, 4.0f);
    // gravity is deliberately unclamped: negative is its normal state.
}

}  // namespace

void DebugGui::draw_params(SimParams& params) {
    ImGui::Begin("Sim Params");
    // InputFloat: type a value directly (commits on Enter / focus loss).
    // The two numbers are the [-]/[+] step-button increments (step, step_fast).
    ImGui::InputFloat("gravity", &params.gravity, 0.5f, 2.0f, "%.2f");
    ImGui::InputFloat("pressure", &params.pressure_multiplier, 25.0f, 100.0f, "%.1f");
    ImGui::InputFloat("density", &params.target_density, 0.01f, 0.1f, "%.3f");
    ImGui::InputFloat("viscosity", &params.viscosity, 0.5f, 2.0f, "%.2f");
    ImGui::InputFloat("dt", &params.dt, 0.0005f, 0.001f, "%.5f");
    ImGui::InputFloat("time scale", &params.time_scale, 0.1f, 0.5f, "%.2f");
    ImGui::InputFloat("tension threshold", &params.tension_threshold, 0.1f, 0.5f, "%.2f");
    ImGui::InputFloat("tension strength", &params.tension_strength, 0.1f, 0.5f, "%.2f");
    ImGui::InputFloat("cohesion", &params.cohesion_strength, 5.0f, 25.0f, "%.1f");
    ImGui::InputFloat("body force x", &params.body_accel_x, 1.0f, 5.0f, "%.2f");
    ImGui::InputFloat("particle size", &params.draw_scale, 0.05f, 0.2f, "%.2f");

    if (ImGui::Button("Reset to defaults")) {
        params = SimParams{};
    }

    clamp_params(params);
    ImGui::End();
}

void DebugGui::end_frame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool DebugGui::wants_mouse() const { return ImGui::GetIO().WantCaptureMouse; }

}  // namespace ui
}  // namespace fluid
