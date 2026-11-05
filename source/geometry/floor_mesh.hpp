#pragma once

#include "mesh_vertex.hpp"

#include <array>
#include <cstdint>

namespace floor_mesh {

extern const std::array<geometry::MeshVertex, 4> vertices;
extern const std::array<std::uint16_t, 6> indices;

} // namespace floor_mesh