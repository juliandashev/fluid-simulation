#include "debug_gui.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

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

void DebugGui::draw_params(SimParams& params) {
    ImGui::Begin("Sim Params");
    // InputFloat: type a value directly (commits on Enter / focus loss).
    // The two numbers are the [-]/[+] step-button increments (step, step_fast).
    ImGui::InputFloat("gravity", &params.gravity, 0.5f, 2.0f, "%.2f");
    ImGui::InputFloat("pressure", &params.pressure_multiplier, 25.0f, 100.0f, "%.1f");
    ImGui::InputFloat("density", &params.target_density, 0.01f, 0.1f, "%.3f");
    ImGui::InputFloat("k_near", &params.near_pressure_multiplier, 10.0f, 50.0f, "%.1f");
    ImGui::InputFloat("viscosity", &params.viscosity, 0.5f, 2.0f, "%.2f");
    ImGui::InputFloat("dt", &params.dt, 0.001f, 0.005f, "%.4f");
    ImGui::InputFloat("time scale", &params.time_scale, 0.1f, 0.5f, "%.2f");

    if (ImGui::Button("Reset to defaults")) {
        params = SimParams{};
    }
    ImGui::End();
}

void DebugGui::end_frame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool DebugGui::wants_mouse() const { return ImGui::GetIO().WantCaptureMouse; }
