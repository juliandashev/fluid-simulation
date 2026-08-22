#pragma once

#include "defs.hpp"

struct GLFWwindow;

// RAII wrapper around Dear ImGui; confines every ImGui symbol to debug_gui.cc.
class DebugGui {
public:
    explicit DebugGui(GLFWwindow* window);
    ~DebugGui();

    DebugGui(const DebugGui&) = delete;             // owns a global context
    DebugGui& operator=(const DebugGui&) = delete;

    void begin_frame();                        // start a new ImGui frame
    void draw_params(SimParams& params);
    void end_frame();                          // render the UI on top of the scene
    bool wants_mouse() const;                  // true when the cursor is over the panel
};
