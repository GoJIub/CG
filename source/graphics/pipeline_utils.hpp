#pragma once

#include <vulkan/vulkan_core.h>

namespace pipeline_utils {

VkShaderModule load_shader_module(
    VkDevice device,
    const char* path
);

bool create_graphics_pipeline(
    VkDevice device,
    VkRenderPass render_pass,
    VkShaderModule vertex_shader,
    VkShaderModule fragment_shader,
    const VkPipelineVertexInputStateCreateInfo& vertex_input_info,
    VkPipelineLayout pipeline_layout,
    VkPipeline& out_pipeline,
    VkPrimitiveTopology topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
);

} // namespace pipeline_utils