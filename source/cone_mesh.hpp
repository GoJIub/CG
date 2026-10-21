#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <vulkan/vulkan_core.h>

namespace cone {

inline constexpr std::uint32_t circle_segments = 32;
inline constexpr std::uint32_t side_line_count = 4;

inline constexpr std::size_t vertex_count =
    2 * (circle_segments + side_line_count + 1);

struct Vertex {
    float position[3];
};

extern const std::array<Vertex, vertex_count> vertices;

extern const VkVertexInputBindingDescription
    vertex_binding_description;

extern const std::array<VkVertexInputAttributeDescription, 1>
    vertex_attribute_descriptions;

} // namespace cone