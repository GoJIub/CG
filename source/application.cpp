#include "application.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>

#include <array>
#include <cstdint>

#include <fstream>
#include <iostream>
#include <vector>

#include <cstddef>

#include <cstring>

namespace {

constexpr float default_imgui_scale = 1.5f;

struct UiSettings {
	float scale = default_imgui_scale;
};

UiSettings ui_settings;
bool imgui_scale_pending = false;
ImGuiStyle unscaled_imgui_style{};

void apply_imgui_scale(float scale) {
	// На случай, если старое значение уже было установлено.
	ImGui::GetIO().FontGlobalScale = 1.0f;

	ImGui::GetStyle() = unscaled_imgui_style;
	ImGui::GetStyle().FontScaleMain = scale;
	ImGui::GetStyle().ScaleAllSizes(scale);
}

constexpr float octahedron_radius = 1.0f;
constexpr float minimum_rotation_axis_length = 0.0001f;

struct Vertex {
    float position[3];
    float color[3];
};

struct GlobalUniforms {
    glm::mat4 model{ 1.0f };
    glm::mat4 view{ 1.0f };
    glm::mat4 projection{ 1.0f };
};

struct SceneSettings {
	float rotation_speed_degrees = 35.0f;
	glm::vec3 rotation_axis{ 0.0f, 1.0f, 0.0f };

	glm::vec3 camera_position{ 2.7f, 2.0f, 3.2f };
	glm::vec3 camera_target{ 0.0f, 0.0f, 0.0f };
	glm::vec3 camera_up{ 0.0f, 1.0f, 0.0f };

	float field_of_view_degrees = 45.0f;
	float near_plane = 0.1f;
	float far_plane = 100.0f;

	bool use_perspective_projection = true;
	float orthographic_half_height = 2.2f;

	glm::vec3 position{ 0.0f, 0.0f, 0.0f };
	glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
};

struct AnimationState {
	double previous_time = 0.0;
	float rotation_angle = 0.0f;
	bool started = false;
};

SceneSettings scene_settings;
AnimationState animation_state;

static_assert(sizeof(GlobalUniforms) == sizeof(float) * 4 * 4 * 3);

constexpr std::array<Vertex, 6> vertices = {
	Vertex{{0.0f, octahedron_radius, 0.0f}, {1.0f, 0.0f, 0.0f}},
	Vertex{{octahedron_radius, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
	Vertex{{0.0f, 0.0f, -octahedron_radius}, {0.0f, 1.0f, 0.0f}},
	Vertex{{-octahedron_radius, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
	Vertex{{0.0f, 0.0f, octahedron_radius}, {0.0f, 1.0f, 0.0f}},
	Vertex{{0.0f, -octahedron_radius, 0.0f}, {1.0f, 0.0f, 0.0f}}
};

constexpr std::array<uint16_t, 24> indices = {
	0, 1, 2,
	0, 2, 3,
	0, 3, 4,
	0, 4, 1,
	5, 2, 1,
	5, 3, 2,
	5, 4, 3,
	5, 1, 4
};

const VkVertexInputBindingDescription vertex_binding_description{
	.binding = 0,
	.stride = sizeof(Vertex),
	.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
};

const std::array<VkVertexInputAttributeDescription, 2> vertex_attribute_descriptions = {
	VkVertexInputAttributeDescription{
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, position)
	},
	VkVertexInputAttributeDescription{
		.location = 1,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, color)
	}
};

struct Resources {
	VkShaderModule vertex_shader = VK_NULL_HANDLE;
	VkShaderModule fragment_shader = VK_NULL_HANDLE;

	VkBuffer vertex_buffer = VK_NULL_HANDLE;
	VmaAllocation vertex_buffer_allocation = nullptr;

	VkBuffer index_buffer = VK_NULL_HANDLE;
	VmaAllocation index_buffer_allocation = nullptr;

	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	VkBuffer uniform_buffer = VK_NULL_HANDLE;
	VmaAllocation uniform_buffer_allocation = nullptr;

	VkDescriptorSetLayout global_uniform_set_layout = VK_NULL_HANDLE;

	VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
	VkDescriptorSet global_uniform_set = VK_NULL_HANDLE;
};

Resources resources;

bool create_buffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    const void* source_data,
    VkBuffer& out_buffer,
    VmaAllocation& out_allocation
) {
	VkBufferCreateInfo buffer_info{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo alloc_info{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
				 VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VmaAllocationInfo allocation_info{};
	VkResult result = vmaCreateBuffer(
		graphics::internal::context.allocator,
		&buffer_info,
		&alloc_info,
		&out_buffer,
		&out_allocation,
		&allocation_info
	);

	if (result != VK_SUCCESS) {
		std::cerr << "Failed to create buffer\n";
		return false;
	}

	std::memcpy(allocation_info.pMappedData, source_data, static_cast<size_t>(size));

	vmaFlushAllocation(
		graphics::internal::context.allocator,
		out_allocation,
		0,
		size
	);

	return true;
}

bool update_uniform_buffer(const GlobalUniforms& uniforms) {
	auto& context = graphics::internal::context;

	void* mapped_data = nullptr;
	if (vmaMapMemory(
		context.allocator,
		resources.uniform_buffer_allocation,
		&mapped_data
	) != VK_SUCCESS) {
		std::cerr << "Failed to map uniform buffer memory\n";
		return false;
	}

	std::memcpy(mapped_data, &uniforms, sizeof(uniforms));

	vmaFlushAllocation(
		context.allocator,
		resources.uniform_buffer_allocation,
		0,
		sizeof(uniforms)
	);

	vmaUnmapMemory(
		context.allocator,
		resources.uniform_buffer_allocation
	);

	return true;
}

bool create_graphics_pipeline() {
	auto& context = graphics::internal::context;

	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = resources.vertex_shader,
			.pName = "main"
		},
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = resources.fragment_shader,
			.pName = "main"
		}
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_binding_description,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attribute_descriptions.size()),
		.pVertexAttributeDescriptions = vertex_attribute_descriptions.data()
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
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
		.layout = resources.pipeline_layout,
		.renderPass = context.render_pass,
		.subpass = 0
	};

	if (vkCreateGraphicsPipelines(
		context.device,
		VK_NULL_HANDLE,
		1,
		&pipeline_info,
		nullptr,
		&resources.pipeline
	) != VK_SUCCESS) {
		std::cerr << "Failed to create graphics pipeline\n";
		return false;
	}

	return true;
}

VkShaderModule loadShaderModule(const char path[]) {
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
	if (vkCreateShaderModule(graphics::internal::context.device,
							 &info, nullptr, &result) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	return result;
}

void destroy_resources() {
	auto& context = graphics::internal::context;

	if (resources.pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(
			context.device,
			resources.pipeline,
			nullptr
		);
		resources.pipeline = VK_NULL_HANDLE;
	}
	if (resources.pipeline_layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(
			context.device,
			resources.pipeline_layout,
			nullptr
		);
		resources.pipeline_layout = VK_NULL_HANDLE;
	}

	if (resources.descriptor_pool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(
			context.device,
			resources.descriptor_pool,
			nullptr
		);
		resources.descriptor_pool = VK_NULL_HANDLE;
		resources.global_uniform_set = VK_NULL_HANDLE;
	}

	if (resources.global_uniform_set_layout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(
			context.device,
			resources.global_uniform_set_layout,
			nullptr
		);
		resources.global_uniform_set_layout = VK_NULL_HANDLE;
	}

	if (resources.uniform_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			resources.uniform_buffer,
			resources.uniform_buffer_allocation
		);
		resources.uniform_buffer = VK_NULL_HANDLE;
		resources.uniform_buffer_allocation = nullptr;
	}

	if (resources.vertex_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, resources.vertex_buffer, resources.vertex_buffer_allocation);
		resources.vertex_buffer = VK_NULL_HANDLE;
		resources.vertex_buffer_allocation = nullptr;
	}
	if (resources.index_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, resources.index_buffer, resources.index_buffer_allocation);
		resources.index_buffer = VK_NULL_HANDLE;
		resources.index_buffer_allocation = nullptr;
	}

	if (resources.vertex_shader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context.device, resources.vertex_shader, nullptr);
		resources.vertex_shader = VK_NULL_HANDLE;
	}

	if (resources.fragment_shader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context.device, resources.fragment_shader, nullptr);
		resources.fragment_shader = VK_NULL_HANDLE;
	}
}

}

namespace application {

void configure_imgui() {
	unscaled_imgui_style = ImGui::GetStyle();
	apply_imgui_scale(ui_settings.scale);
}

void apply_pending_imgui_scale() {
	if (!imgui_scale_pending) {
		return;
	}

	apply_imgui_scale(ui_settings.scale);
	imgui_scale_pending = false;
}

bool initialize() {
	auto& context = graphics::internal::context;

	resources.vertex_shader = loadShaderModule("shaders/octahedron.vert.spv");
	if (resources.vertex_shader == VK_NULL_HANDLE) {
		return false;
	}

	resources.fragment_shader = loadShaderModule("shaders/octahedron.frag.spv");
	if (resources.fragment_shader == VK_NULL_HANDLE) {
		destroy_resources();
		return false;
	}

	if (!create_buffer(
		sizeof(vertices),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		vertices.data(),
		resources.vertex_buffer,
		resources.vertex_buffer_allocation
	)) {
		destroy_resources();
		return false;
	}

	if (!create_buffer(
		sizeof(indices),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		indices.data(),
		resources.index_buffer,
		resources.index_buffer_allocation
	)) {
		destroy_resources();
		return false;
	}

	const GlobalUniforms initial_uniforms{};

	if (!create_buffer(
		sizeof(initial_uniforms),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		&initial_uniforms,
		resources.uniform_buffer,
		resources.uniform_buffer_allocation
	)) {
		destroy_resources();
		return false;
	}

	const VkDescriptorSetLayoutBinding global_uniform_binding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	};

	const VkDescriptorSetLayoutCreateInfo set_layout_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &global_uniform_binding,
	};

	if (vkCreateDescriptorSetLayout(
		context.device,
		&set_layout_info,
		nullptr,
		&resources.global_uniform_set_layout
	) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor set layout\n";
		destroy_resources();
		return false;
	}

	const VkDescriptorPoolSize pool_size{
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
	};

	const VkDescriptorPoolCreateInfo pool_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &pool_size,
	};

	if (vkCreateDescriptorPool(
		context.device,
		&pool_info,
		nullptr,
		&resources.descriptor_pool
	) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor pool\n";
		destroy_resources();
		return false;
	}

	const VkDescriptorSetAllocateInfo allocate_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = resources.descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &resources.global_uniform_set_layout,
	};

	if (vkAllocateDescriptorSets(
		context.device,
		&allocate_info,
		&resources.global_uniform_set
	) != VK_SUCCESS) {
		std::cerr << "Failed to allocate descriptor sets\n";
		destroy_resources();
		return false;
	}

	const VkDescriptorBufferInfo uniform_buffer_info{
		.buffer = resources.uniform_buffer,
		.offset = 0,
		.range = sizeof(GlobalUniforms),
	};

	const VkWriteDescriptorSet write_descriptor_set{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = resources.global_uniform_set,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &uniform_buffer_info
	};

	vkUpdateDescriptorSets(
		context.device,
		1,
		&write_descriptor_set,
		0,
		nullptr
	);

	const VkDescriptorSetLayout set_layouts[] = {
		resources.global_uniform_set_layout,
	};

	const VkPipelineLayoutCreateInfo pipeline_layout_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = set_layouts
	};

	if (vkCreatePipelineLayout(
		context.device,
		&pipeline_layout_info,
		nullptr,
		&resources.pipeline_layout
	) != VK_SUCCESS) {
		std::cerr << "Failed to create pipeline layout\n";
		destroy_resources();
		return false;
	}

	if (!create_graphics_pipeline()) {
		destroy_resources();
		return false;
	}

	return true;
}

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);

	destroy_resources();
}

void draw_scene_controls() {
	ImGui::Begin("Scene controls");

	ImGui::SliderFloat(
		"Rotation speed",
		&scene_settings.rotation_speed_degrees,
		-180.0f,
		180.0f,
		"%.1f deg/s"
	);

	ImGui::SliderFloat3(
		"Rotation axis",
		&scene_settings.rotation_axis.x,
		-1.0f,
		1.0f
	);

	ImGui::Checkbox(
		"Perspective projection",
		&scene_settings.use_perspective_projection
	);

	if (scene_settings.use_perspective_projection) {
		ImGui::SliderFloat(
			"Field of view",
			&scene_settings.field_of_view_degrees,
			20.0f,
			120.0f,
			"%.1f deg"
		);
	} else {
		ImGui::SliderFloat(
			"Orthographic half height",
			&scene_settings.orthographic_half_height,
			0.5f,
			10.0f,
			"%.2f"
		);
	}

	ImGui::DragFloat3(
		"Position",
		&scene_settings.position.x,
		0.01f,
		-3.0f,
		3.0f,
		"%.2f"
	);

	ImGui::DragFloat3(
		"Scale",
		&scene_settings.scale.x,
		0.01f,
		0.1f,
		5.0f,
		"%.2f"
	);

	if (ImGui::Button("Reset scene")) {
		scene_settings = SceneSettings{};
		animation_state = AnimationState{};
	}

	if (ImGui::SliderFloat(
		"Interface scale",
		&ui_settings.scale,
		1.0f,
		2.5f,
		"%.2fx"
	)) {
		imgui_scale_pending = true;
	}

	ImGui::End();
}

void update(double time) {
	draw_scene_controls();

	auto& context = graphics::internal::context;

	if (context.swapchain_extent.height == 0) {
		return;
	}

	if (!animation_state.started) {
		animation_state.previous_time = time;
		animation_state.started = true;
	}

	const float delta_time =
		static_cast<float>(time - animation_state.previous_time);

	animation_state.previous_time = time;

	animation_state.rotation_angle +=
		delta_time *
		glm::radians(scene_settings.rotation_speed_degrees);

	const float rotation_angle = animation_state.rotation_angle;

	const float aspect_ratio =
		static_cast<float>(context.swapchain_extent.width) /
		static_cast<float>(context.swapchain_extent.height);

	GlobalUniforms uniforms{};

	if (glm::length(scene_settings.rotation_axis) <
		minimum_rotation_axis_length) {
		scene_settings.rotation_axis = glm::vec3(0.0f, 1.0f, 0.0f);
	}

	const glm::vec3 rotation_axis =
		glm::normalize(scene_settings.rotation_axis);

	glm::mat4 model(1.0f);

	model = glm::translate(model, scene_settings.position);
	model = glm::rotate(model, rotation_angle, rotation_axis);
	model = glm::scale(model, scene_settings.scale);

	uniforms.model = model;

	uniforms.view = glm::lookAt(
		scene_settings.camera_position,
		scene_settings.camera_target,
		scene_settings.camera_up
	);

	if (scene_settings.use_perspective_projection) {
		uniforms.projection = glm::perspective(
			glm::radians(scene_settings.field_of_view_degrees),
			aspect_ratio,
			scene_settings.near_plane,
			scene_settings.far_plane
		);
	} else {
		const float half_height =
			scene_settings.orthographic_half_height;

		const float half_width =
			aspect_ratio * half_height;

		uniforms.projection = glm::ortho(
			-half_width,
			half_width,
			-half_height,
			half_height,
			scene_settings.near_plane,
			scene_settings.far_plane
		);
	}

	// У GLM и Vulkan различается направление оси Y экрана.
	uniforms.projection[1][1] *= -1.0f;

	update_uniform_buffer(uniforms);
}

void render(const graphics::internal::FrameData& fd) {
	auto& context = graphics::internal::context;

	VkCommandBuffer command_buffer = fd.command_buffer;
	vkResetCommandBuffer(command_buffer, 0);

	VkCommandBufferBeginInfo command_begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	if (vkBeginCommandBuffer(command_buffer, &command_begin_info) != VK_SUCCESS) {
		std::cerr << "Failed to begin recording command buffer\n";
		return;
	}

	VkClearValue clear_values[] = {
		{.color = {{0.05f, 0.08f, 0.12f, 1.0f}}},
		{.depthStencil = {1.0f, 0}}
	};

	VkRenderPassBeginInfo render_pass_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = context.render_pass,
		.framebuffer = fd.framebuffer,
		.renderArea = { .extent = context.swapchain_extent },
		.clearValueCount = sizeof(clear_values) / sizeof(clear_values[0]),
		.pClearValues = clear_values
	};

	vkCmdBeginRenderPass(
		command_buffer,
		&render_pass_info,
		VK_SUBPASS_CONTENTS_INLINE
	);

	const VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(context.swapchain_extent.width),
		.height = static_cast<float>(context.swapchain_extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	const VkRect2D scissor{ .extent = context.swapchain_extent };

	vkCmdSetViewport(command_buffer, 0, 1, &viewport);
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);


	vkCmdBindPipeline(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		resources.pipeline
	);

	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		resources.pipeline_layout,
		0, // set = 0
		1,
		&resources.global_uniform_set,
		0,
		nullptr
	);

	const VkDeviceSize vertex_buffer_offset = 0;

	vkCmdBindVertexBuffers(
		command_buffer,
		0,
		1,
		&resources.vertex_buffer,
		&vertex_buffer_offset
	);

	vkCmdBindIndexBuffer(
		command_buffer,
		resources.index_buffer,
		0,
		VK_INDEX_TYPE_UINT16
	);

	vkCmdDrawIndexed(
		command_buffer,
		static_cast<uint32_t>(indices.size()),
		1,
		0,
		0,
		0
	);

	vkCmdEndRenderPass(command_buffer);
	if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
		std::cerr << "Failed to end command buffer\n";
		return;
	}
}

} // namespace application
