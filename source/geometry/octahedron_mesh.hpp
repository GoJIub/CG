#pragma once

#include "mesh_vertex.hpp"

#include <array>
#include <cstdint>
#include <vulkan/vulkan_core.h>

using Vertex = geometry::MeshVertex;

namespace octahedron {

inline constexpr float octahedron_radius = 1.0f;

extern const std::array<Vertex, 24> vertices;
extern const std::array<uint16_t, 24> indices;

extern const VkVertexInputBindingDescription vertex_binding_description;
extern const std::array<VkVertexInputAttributeDescription, 3> vertex_attribute_descriptions;

} // namespace octahedron