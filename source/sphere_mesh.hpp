#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <vulkan/vulkan_core.h>

namespace sphere {

inline constexpr std::uint32_t latitude_segments = 16;
inline constexpr std::uint32_t longitude_segments = 24;

inline constexpr std::size_t vertex_count =
    (latitude_segments + 1) * (longitude_segments + 1);

inline constexpr std::size_t index_count =
    6 * longitude_segments * (latitude_segments - 1);

struct Vertex {
    float position[3];
};

extern const std::array<Vertex, vertex_count> vertices;
extern const std::array<std::uint32_t, index_count> indices;

extern const VkVertexInputBindingDescription
    vertex_binding_description;

extern const std::array<VkVertexInputAttributeDescription, 1>
    vertex_attribute_descriptions;

} // namespace sphere