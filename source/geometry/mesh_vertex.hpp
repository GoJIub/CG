#pragma once

#include <array>
#include <vulkan/vulkan_core.h>

namespace geometry {

struct MeshVertex {
    float position[3];
    float uv[2];
    float normal[3];
};

extern const VkVertexInputBindingDescription
    mesh_vertex_binding_description;

extern const std::array<VkVertexInputAttributeDescription, 3>
    mesh_vertex_attribute_descriptions;

} // namespace geometry