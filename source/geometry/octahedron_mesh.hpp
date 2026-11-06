#pragma once

#include "mesh_vertex.hpp"

#include <array>
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace octahedron {

using Vertex = geometry::MeshVertex;

inline constexpr float octahedron_radius = 1.0f;

extern const std::array<Vertex, 24> vertices;
extern const std::array<uint16_t, 24> indices;

} // namespace octahedron