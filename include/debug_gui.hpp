#pragma once

#include <cstdint>

#include "color_field.hpp"
#include "defs.hpp"

struct GLFWwindow;

namespace fluid {
namespace ui {

// RAII wrapper around Dear ImGui; confines every ImGui symbol to debug_gui.cc.
class DebugGui {
public:
    explicit DebugGui(GLFWwindow* window);
    ~DebugGui();

    DebugGui(const DebugGui&) = delete;             // owns a global context
    DebugGui& operator=(const DebugGui&) = delete;

    void begin_frame();                        // start a new ImGui frame
    void draw_params(SimParams& params);
    void draw_controls(const char* text);  // key strip across the top
    void draw_legend(gl::ColorField field, const SimParams& params);

    // Renders EXPERIMENTS[]; returns the chosen Experiment id, or -1 if nothing was clicked.
    int32_t draw_experiments(int32_t current);
    void end_frame();                          // render the UI on top of the scene
    bool wants_mouse() const;                  // true when the cursor is over the panel
    bool wants_keyboard() const;               // true while a panel field has focus
};

}  // namespace ui
}  // namespace fluid
