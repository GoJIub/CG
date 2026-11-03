#include "pipeline_utils.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace pipeline_utils {

VkShaderModule load_shader_module(
    VkDevice device,
    const char* path
) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) {
		std::cerr << "Failed to open shader file: " << path << '\n';
		return VK_NULL_HANDLE;
	}
	const size_t size = file.tellg();

	if (size == 0) {
		std::cerr << "Failed to read shader file: " << path << std::endl;
		return VK_NULL_HANDLE;
	}
	if (size % sizeof(uint32_t) != 0) {
		std::cerr << "Shader file size is not a multiple of 4: " << path << std::endl;
		return VK_NULL_HANDLE;
	}

	std::vector<uint32_t> buffer(size / sizeof(uint32_t));

	file.seekg(0);
	file.read(
		reinterpret_cast<char*>(buffer.data()),
		static_cast<std::streamsize>(size)
	);
	if (!file) {
		std::cerr << "Failed to read shader file: " << path << std::endl;
		return VK_NULL_HANDLE;
	}
	file.close();

	VkShaderModuleCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = size,
		.pCode = buffer.data(),
	};

	VkShaderModule result;
	if (vkCreateShaderModule(device, &info, nullptr, &result) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	return result;
}

bool create_graphics_pipeline(
    VkDevice device,
    VkRenderPass render_pass,
    VkShaderModule vertex_shader,
    VkShaderModule fragment_shader,
    const VkPipelineVertexInputStateCreateInfo& vertex_input_info,
    VkPipelineLayout pipeline_layout,
    VkPipeline& out_pipeline,
    VkPrimitiveTopology topology
) {
	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertex_shader,
			.pName = "main"
		},
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragment_shader,
			.pName = "main"
		}
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = topology,
		.primitiveRestartEnable = VK_FALSE
	};

	const VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineViewportStateCreateInfo viewport_state_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	VkPipelineDynamicStateCreateInfo dynamic_state_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(std::size(dynamic_states)),
		.pDynamicStates = dynamic_states
	};

	VkPipelineRasterizationStateCreateInfo rasterization_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f
	};

	VkPipelineMultisampleStateCreateInfo multisample_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	VkPipelineDepthStencilStateCreateInfo depth_stencil_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS
	};

	VkPipelineColorBlendAttachmentState color_blend_attachment{
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
						  VK_COLOR_COMPONENT_G_BIT |
						  VK_COLOR_COMPONENT_B_BIT |
						  VK_COLOR_COMPONENT_A_BIT
	};

	VkPipelineColorBlendStateCreateInfo color_blend_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment
	};

	VkGraphicsPipelineCreateInfo pipeline_info{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = static_cast<uint32_t>(shader_stages.size()),
		.pStages = shader_stages.data(),
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly_info,
		.pViewportState = &viewport_state_info,
		.pRasterizationState = &rasterization_info,
		.pMultisampleState = &multisample_info,
		.pDepthStencilState = &depth_stencil_info,
		.pColorBlendState = &color_blend_info,
		.pDynamicState = &dynamic_state_info,
		.layout = pipeline_layout,
		.renderPass = render_pass,
		.subpass = 0
	};

	if (vkCreateGraphicsPipelines(
		device,
		VK_NULL_HANDLE,
		1,
		&pipeline_info,
		nullptr,
		&out_pipeline
	) != VK_SUCCESS) {
		std::cerr << "Failed to create graphics pipeline\n";
		return false;
	}

	return true;
}

} // namespace pipeline_utils