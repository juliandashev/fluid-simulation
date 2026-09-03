#include "debug_gui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "experiment.hpp"

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
    params.target_speed = std::max(params.target_speed, 0.0f);
    params.wing_aoa_deg = std::clamp(params.wing_aoa_deg, -60.0f, 60.0f);
    // Upper bound leaves the wake room to recover before the periodic seam.
    params.wing_chord_frac = std::clamp(params.wing_chord_frac, 0.1f, 1.2f);
    params.render_smoothing = std::clamp(params.render_smoothing, 1.0f, 4.0f);
    params.blow_speed = std::clamp(params.blow_speed, 0.0f, 150.0f);
    // gravity is deliberately unclamped: negative is its normal state.
}

// Mirrors ramp_color() in particle.frag; the two must stay in step or the bar
// lies about what the particles are showing.
ImU32 ramp_color(float_t t) {
    const ImVec4 blue(0.3f, 0.6f, 1.0f, 1.0f);
    const ImVec4 green(0.2f, 0.9f, 0.3f, 1.0f);
    const ImVec4 yellow(1.0f, 0.9f, 0.2f, 1.0f);
    const ImVec4 red(1.0f, 0.2f, 0.15f, 1.0f);

    ImVec4 a = blue, b = green;
    float_t local = t / 0.3333f;

    if (t >= 0.6666f) {
        a = yellow, b = red, local = (t - 0.6666f) / 0.3334f;
    } else if (t >= 0.3333f) {
        a = green, b = yellow, local = (t - 0.3333f) / 0.3333f;
    }

    local = std::clamp(local, 0.0f, 1.0f);

    return ImGui::ColorConvertFloat4ToU32(ImVec4(a.x + (b.x - a.x) * local,
                                                 a.y + (b.y - a.y) * local,
                                                 a.z + (b.z - a.z) * local, 1.0f));
}

// The bar is linear in the quantity, so the colour has to carry the same warp
// the vertex shader applies - otherwise the labels sit at the wrong colours.
float_t warp(float_t u, bool speed) {
    return speed ? std::pow(u, 1.5f) : std::sqrt(u);
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
    ImGui::InputFloat("target speed", &params.target_speed, 1.0f, 5.0f, "%.1f");
    ImGui::InputFloat("wing angle", &params.wing_aoa_deg, 1.0f, 5.0f, "%.1f");
    ImGui::InputFloat("wing chord", &params.wing_chord_frac, 0.05f, 0.2f, "%.2f");
    ImGui::InputFloat("blow speed", &params.blow_speed, 5.0f, 20.0f, "%.0f");
    ImGui::InputFloat("render smoothing", &params.render_smoothing, 0.1f, 0.5f, "%.2f");
    ImGui::InputFloat("particle size", &params.draw_scale, 0.05f, 0.2f, "%.2f");
    ImGui::InputFloat("pressure min", &params.pressure_min, 25.0f, 100.0f, "%.0f");
    ImGui::InputFloat("pressure max", &params.pressure_max, 25.0f, 100.0f, "%.0f");

    if (ImGui::Button("Reset to defaults")) {
        params = SimParams{};
    }

    clamp_params(params);
    ImGui::End();
}

void DebugGui::draw_controls(const char* text) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    const ImVec2 size = ImGui::CalcTextSize(text);

    const float_t x = 0.5f * (screen.x - size.x);
    const float_t y = 8.0f;

    // A plate behind it: the strip sits over the fluid, which is any colour.
    draw->AddRectFilled(ImVec2(x - 10.0f, y - 4.0f), ImVec2(x + size.x + 10.0f, y + size.y + 4.0f),
                        IM_COL32(12, 16, 28, 170), 4.0f);
    draw->AddText(ImVec2(x, y), IM_COL32(196, 202, 216, 255), text);
}

void DebugGui::draw_legend(gl::ColorField field, const SimParams& params) {
    if (field != gl::ColorField::Speed && field != gl::ColorField::Pressure) {
        return;
    }

    const bool speed = field == gl::ColorField::Speed;
    const float_t lo = speed ? 0.0f : params.pressure_min;
    const float_t hi = speed ? SPEED_COLOR_MAX : params.pressure_max;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 screen = ImGui::GetIO().DisplaySize;

    constexpr float_t BAR_W = 300.0f;
    constexpr float_t BAR_H = 12.0f;
    constexpr int32_t SEGMENTS = 128;

    const float_t x0 = 0.5f * (screen.x - BAR_W);
    const float_t y0 = 60.0f;  // clears the control strip

    for (int32_t i = 0; i < SEGMENTS; ++i) {
        const float_t u = static_cast<float_t>(i) / SEGMENTS;
        const float_t x = x0 + u * BAR_W;

        // +1 px of overlap, or seams show through between the strips.
        draw->AddRectFilled(ImVec2(x, y0), ImVec2(x + BAR_W / SEGMENTS + 1.0f, y0 + BAR_H),
                            ramp_color(warp(u, speed)));
    }

    draw->AddRect(ImVec2(x0, y0), ImVec2(x0 + BAR_W, y0 + BAR_H), IM_COL32(255, 255, 255, 90));

    const ImU32 ink = IM_COL32(230, 232, 238, 255);
    const char* title = speed ? "speed" : "pressure";
    const ImVec2 title_size = ImGui::CalcTextSize(title);

    draw->AddText(ImVec2(x0 + 0.5f * (BAR_W - title_size.x), y0 - 18.0f), ink, title);

    for (int32_t i = 0; i < 3; ++i) {
        const float_t u = 0.5f * i;
        char label[32];
        std::snprintf(label, sizeof(label), "%.0f", lo + u * (hi - lo));

        const ImVec2 size = ImGui::CalcTextSize(label);
        const float_t x = x0 + u * BAR_W - 0.5f * size.x;

        draw->AddText(ImVec2(x, y0 + BAR_H + 4.0f), ink, label);
    }
}

int32_t DebugGui::draw_experiments(int32_t current) {
    int32_t picked = -1;

    ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Experiments");

    // Driven by the same table as --help and the startup banner. Keyed on the
    // spec's id rather than its slot, so reordering the table changes nothing.
    for (const ExperimentSpec& spec : EXPERIMENTS) {
        const int32_t id = static_cast<int32_t>(spec.id);

        if (ImGui::RadioButton(spec.name, id == current) && id != current) {
            picked = id;
        }

        ImGui::TextDisabled("    %s", spec.summary);
    }

    ImGui::End();

    return picked;
}

void DebugGui::end_frame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool DebugGui::wants_mouse() const { return ImGui::GetIO().WantCaptureMouse; }

bool DebugGui::wants_keyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }

}  // namespace ui
}  // namespace fluid
