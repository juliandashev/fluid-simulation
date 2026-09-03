#pragma once

#include <cstdint>

namespace fluid {
namespace gl {

// Split out of renderer.hpp: bindings.hpp needs the enumerators, and pulling
// glad in for them would put the whole GL header set behind every binding.
enum class ColorField : int32_t { None, Speed, Pressure, Temperature };  // Temperature: reserved

}  // namespace gl
}  // namespace fluid
