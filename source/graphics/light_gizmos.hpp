#pragma once

#include "scene/scene.hpp"

#include <cstddef>
#include <vector>

#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

namespace light_gizmos {

struct MeshResources {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_allocation = nullptr;

    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation index_allocation = nullptr;
};

struct VertexBufferResources {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
};

struct MarkerPipelineResources {
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipeline cone_pipeline = VK_NULL_HANDLE;
};

struct Resources {
    MeshResources sphere_mesh;
    VertexBufferResources cone_vertices;
    MarkerPipelineResources markers;
};


bool initialize(
    Resources& gizmos,
    VkDevice device,
    VmaAllocator allocator,
    VkRenderPass render_pass,
    VkDescriptorSetLayout scene_set_layout
);

void destroy(
    Resources& gizmos,
    VkDevice device,
    VmaAllocator allocator
);

void render_light_markers(
    const Resources& gizmos,
    VkCommandBuffer command_buffer,
    VkDescriptorSet scene_descriptor_set,
    const std::vector<scene::LightMarkerData>& markers
);

void render_spot_light_cones(
    const Resources& gizmos,
    VkCommandBuffer command_buffer,
    VkDescriptorSet scene_descriptor_set,
    const std::vector<glm::mat4>& models
);

} // namespace light_gizmos