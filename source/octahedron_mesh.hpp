#pragma once

#include <array>
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace octahedron {

inline constexpr float octahedron_radius = 1.0f;

struct Vertex {
    float position[3];
    float color[3];
};

extern const std::array<Vertex, 6> vertices;
extern const std::array<uint16_t, 24> indices;

extern const VkVertexInputBindingDescription vertex_binding_description;
extern const std::array<VkVertexInputAttributeDescription, 2> vertex_attribute_descriptions;

} // namespace octahedron