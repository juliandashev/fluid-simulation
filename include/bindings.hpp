#pragma once

#include <GLFW/glfw3.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

#include "color_field.hpp"
#include "input.hpp"

namespace fluid {
namespace ui {

// Everything a binding is allowed to touch. The three flags are per-frame
// requests: cleared before each dispatch, read by the loop straight after.
struct AppState {
    bool& paused;
    bool& show_params;
    bool& show_experiments;
    bool& field_view;
    bool& blow_mode;
    gl::ColorField& color_field;
    bool step_fwd = false;
    bool step_back = false;
    bool reset = false;
};

struct Binding {
    int32_t key;
    const char* label;
    const char* help;  // startup banner
    const char* hud;   // on-screen strip; kept terse, it shares one line
    void (*run)(AppState&);
};

// One table drives both the dispatch and the startup banner, so a new key
// cannot end up bound but undocumented.
inline constexpr Binding BINDINGS[] = {
    {GLFW_KEY_SPACE, "Space", "pause / resume", "pause",
     [](AppState& s) { s.paused = !s.paused; }},
    {GLFW_KEY_RIGHT, "Right", "step one frame forward", "step +",
     [](AppState& s) { s.step_fwd = true; }},
    {GLFW_KEY_LEFT, "Left", "step one frame back", "step -",
     [](AppState& s) { s.step_back = true; }},
    {GLFW_KEY_R, "R", "respawn the scene", "respawn",
     [](AppState& s) { s.reset = true; }},
    {GLFW_KEY_H, "H", "show / hide the parameter panel", "panel",
     [](AppState& s) { s.show_params = !s.show_params; }},
    {GLFW_KEY_E, "E", "show / hide the experiment menu", "scenes",
     [](AppState& s) { s.show_experiments = !s.show_experiments; }},
    {GLFW_KEY_F, "F", "particles / reconstructed field", "field",
     [](AppState& s) { s.field_view = !s.field_view; }},
    {GLFW_KEY_B, "B", "drag blows air sideways instead of pulling", "blow",
     [](AppState& s) { s.blow_mode = !s.blow_mode; }},
    {GLFW_KEY_S, "S", "colour by speed (again for plain fluid)", "speed",
     [](AppState& s) {
         s.color_field = s.color_field == gl::ColorField::Speed ? gl::ColorField::None
                                                                : gl::ColorField::Speed;
     }},
    {GLFW_KEY_P, "P", "colour by pressure (again for plain fluid)", "pressure",
     [](AppState& s) {
         s.color_field = s.color_field == gl::ColorField::Pressure ? gl::ColorField::None
                                                                   : gl::ColorField::Pressure;
     }},
};

// One line for the on-screen strip, from the same table as everything else.
inline std::string hud_line() {
    std::string out;

    for (const Binding& b : BINDINGS) {
        if (!out.empty()) {
            out += "   ";
        }
        out += "(";
        out += b.label;
        out += ") ";
        out += b.hud;
    }

    return out + "   (Esc) quit";
}

inline void print_bindings() {
    std::cout << "Keys:\n";
    for (const Binding& b : BINDINGS) {
        std::cout << "  " << std::left << std::setw(7) << b.label << b.help << "\n";
    }
    std::cout << "  " << std::left << std::setw(7) << "Esc" << "quit\n";
}

// Polls every bound key even when disabled, or a key held down while typing in
// the panel would fire a stale edge on the frame the field loses focus.
inline void dispatch(Input& in, AppState& state, bool enabled) {
    state.step_fwd = false;
    state.step_back = false;
    state.reset = false;

    for (const Binding& b : BINDINGS) {
        const bool edge = in.pressed(b.key);

        if (edge && enabled) {
            b.run(state);
        }
    }
}

}  // namespace ui
}  // namespace fluid
