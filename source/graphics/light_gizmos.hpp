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
    VkQueue queue,
    std::uint32_t queue_family_index,
    VkRenderPass render_pass,
    VkDescriptorSetLayout scene_set_layout,
    VkDescriptorSetLayout object_set_layout
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
    VkDescriptorSet object_descriptor_set,
    std::size_t marker_count,
    std::size_t first_record,
    VkDeviceSize stride
);

void render_spot_light_cones(
    const Resources& gizmos,
    VkCommandBuffer command_buffer,
    VkDescriptorSet scene_descriptor_set,
    VkDescriptorSet object_descriptor_set,
    std::size_t cone_count,
    std::size_t first_record,
    VkDeviceSize stride
);

} // namespace light_gizmos