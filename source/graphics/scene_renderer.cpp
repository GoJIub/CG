#include "scene_renderer.hpp"
#include "gpu_buffer.hpp"
#include "light_gizmos.hpp"
#include "pipeline_utils.hpp"

#include "geometry/octahedron_mesh.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <iostream>

namespace {

struct StorageBufferResources {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    std::size_t capacity = 0;
};

struct SceneResources {
    VkBuffer uniform_buffer = VK_NULL_HANDLE;
    VmaAllocation uniform_allocation = nullptr;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    StorageBufferResources point_lights;
    StorageBufferResources spot_lights;
};

struct ObjectResources {
    VkBuffer uniform_buffer = VK_NULL_HANDLE;
    VmaAllocation uniform_allocation = nullptr;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
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

	VkDescriptorSetLayout scene_set_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout object_set_layout = VK_NULL_HANDLE;

	VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

	std::array<ObjectResources, scene::object_count> objects{};

	SceneResources scene{};

	light_gizmos::Resources gizmos;
};

Resources resources;


void destroy_resources() {
	auto& context = graphics::internal::context;

	light_gizmos::destroy(
		resources.gizmos,
		context.device,
		context.allocator
	);

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
		for (auto& object_resources : resources.objects) {
			object_resources.descriptor_set = VK_NULL_HANDLE;
		}
		resources.scene.descriptor_set = VK_NULL_HANDLE;
	}

	if (resources.scene_set_layout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(
			context.device,
			resources.scene_set_layout,
			nullptr
		);

		resources.scene_set_layout = VK_NULL_HANDLE;
	}

	if (resources.object_set_layout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(
			context.device,
			resources.object_set_layout,
			nullptr
		);
		resources.object_set_layout = VK_NULL_HANDLE;
	}

	if (resources.scene.uniform_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			resources.scene.uniform_buffer,
			resources.scene.uniform_allocation
		);

		resources.scene.uniform_buffer = VK_NULL_HANDLE;
		resources.scene.uniform_allocation = nullptr;
	}

	if (resources.scene.spot_lights.buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			resources.scene.spot_lights.buffer,
			resources.scene.spot_lights.allocation
		);

		resources.scene.spot_lights.buffer = VK_NULL_HANDLE;
		resources.scene.spot_lights.allocation = nullptr;
		resources.scene.spot_lights.capacity = 0;
	}

	if (resources.scene.point_lights.buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			resources.scene.point_lights.buffer,
			resources.scene.point_lights.allocation
		);

		resources.scene.point_lights.buffer = VK_NULL_HANDLE;
		resources.scene.point_lights.allocation = nullptr;
		resources.scene.point_lights.capacity = 0;
	}

	for (auto& object_resources : resources.objects) {
		if (object_resources.uniform_buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(
				context.allocator,
				object_resources.uniform_buffer,
				object_resources.uniform_allocation
			);
			object_resources.uniform_buffer = VK_NULL_HANDLE;
			object_resources.uniform_allocation = nullptr;
		}
	}

	if (resources.vertex_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			resources.vertex_buffer,
			resources.vertex_buffer_allocation
		);
		resources.vertex_buffer = VK_NULL_HANDLE;
		resources.vertex_buffer_allocation = nullptr;
	}
	if (resources.index_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			resources.index_buffer,
			resources.index_buffer_allocation
		);
		resources.index_buffer = VK_NULL_HANDLE;
		resources.index_buffer_allocation = nullptr;
	}

	if (resources.vertex_shader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(
			context.device,
			resources.vertex_shader,
			nullptr
		);
		resources.vertex_shader = VK_NULL_HANDLE;
	}

	if (resources.fragment_shader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(
			context.device,
			resources.fragment_shader,
			nullptr
		);
		resources.fragment_shader = VK_NULL_HANDLE;
	}
}

void update_storage_buffer_descriptor(
    const StorageBufferResources& storage,
    std::size_t element_size,
    std::uint32_t binding
) {
    const VkDescriptorBufferInfo buffer_info{
        .buffer = storage.buffer,
        .offset = 0,
        .range =
            static_cast<VkDeviceSize>(storage.capacity) *
            element_size
    };

    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = resources.scene.descriptor_set,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffer_info
    };

    vkUpdateDescriptorSets(
        graphics::internal::context.device,
        1, &write,
        0, nullptr
    );
}

bool ensure_storage_buffer_capacity(
    StorageBufferResources& storage,
    std::size_t count,
    std::size_t element_size,
    std::uint32_t binding
) {
    if (element_size == 0) {
        std::cerr << "Storage buffer element size must be nonzero\n";
        return false;
    }

    const std::size_t required =
        std::max(count, std::size_t{1});

    if (required <= storage.capacity) {
        return true;
    }

    auto& context = graphics::internal::context;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(
        context.physical_device,
        &properties
    );

    const std::size_t max_capacity =
        properties.limits.maxStorageBufferRange / element_size;

    if (required > max_capacity) {
        std::cerr << "Requested storage buffer size exceeds limit\n";
        return false;
    }

    const std::size_t doubled_capacity =
        storage.capacity > max_capacity / 2
            ? max_capacity
            : storage.capacity * 2;

    const std::size_t new_capacity =
        std::max(required, doubled_capacity);

    VkBuffer new_buffer = VK_NULL_HANDLE;
    VmaAllocation new_allocation = nullptr;

    if (!gpu_buffer::create_buffer(
		context.allocator,
        static_cast<VkDeviceSize>(new_capacity) * element_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        nullptr,
        new_buffer,
        new_allocation
    )) {
        return false;
    }

    const VkBuffer old_buffer = storage.buffer;
    const VmaAllocation old_allocation = storage.allocation;

    storage.buffer = new_buffer;
    storage.allocation = new_allocation;
    storage.capacity = new_capacity;

    update_storage_buffer_descriptor(
        storage,
        element_size,
        binding
    );

    if (old_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(
            context.allocator,
            old_buffer,
            old_allocation
        );
    }

    return true;
}

} // namespace


namespace scene_renderer {

bool initialize() {
	auto& context = graphics::internal::context;

	resources.vertex_shader = pipeline_utils::load_shader_module(context.device, "shaders/octahedron.vert.spv");
	if (resources.vertex_shader == VK_NULL_HANDLE) {
		return false;
	}

	resources.fragment_shader = pipeline_utils::load_shader_module(context.device, "shaders/octahedron.frag.spv");
	if (resources.fragment_shader == VK_NULL_HANDLE) {
		destroy_resources();
		return false;
	}

	if (!gpu_buffer::create_buffer(
		context.allocator,
		sizeof(octahedron::vertices),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		octahedron::vertices.data(),
		resources.vertex_buffer,
		resources.vertex_buffer_allocation
	)) {
		destroy_resources();
		return false;
	}

	if (!gpu_buffer::create_buffer(
		context.allocator,
		sizeof(octahedron::indices),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		octahedron::indices.data(),
		resources.index_buffer,
		resources.index_buffer_allocation
	)) {
		destroy_resources();
		return false;
	}

	const scene::SceneUniformData initial_scene_uniforms{};

	if (!gpu_buffer::create_buffer(
		context.allocator,
		sizeof(initial_scene_uniforms),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		&initial_scene_uniforms,
		resources.scene.uniform_buffer,
		resources.scene.uniform_allocation
	)) {
		destroy_resources();
		return false;
	}

	const scene::PointLightData empty_point_light{};

	if (!gpu_buffer::create_buffer(
		context.allocator,
		sizeof(empty_point_light),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		&empty_point_light,
		resources.scene.point_lights.buffer,
		resources.scene.point_lights.allocation
	)) {
		destroy_resources();
		return false;
	}

	resources.scene.point_lights.capacity = 1;

	const scene::SpotLightData empty_spot_light{};

	if (!gpu_buffer::create_buffer(
		context.allocator,
		sizeof(empty_spot_light),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		&empty_spot_light,
		resources.scene.spot_lights.buffer,
		resources.scene.spot_lights.allocation
	)) {
		destroy_resources();
		return false;
	}

	resources.scene.spot_lights.capacity = 1;

	const scene::ObjectUniformData initial_uniforms{};

	for (std::size_t i = 0; i < resources.objects.size(); ++i) {
		if (!gpu_buffer::create_buffer(
			context.allocator,
			sizeof(initial_uniforms),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			&initial_uniforms,
			resources.objects[i].uniform_buffer,
			resources.objects[i].uniform_allocation
		)) {
			destroy_resources();
			return false;
		}
	}

	const VkDescriptorSetLayoutBinding global_uniform_binding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags =
			VK_SHADER_STAGE_VERTEX_BIT |
			VK_SHADER_STAGE_FRAGMENT_BIT
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
		&resources.object_set_layout
	) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor set layout\n";
		destroy_resources();
		return false;
	}

	const VkDescriptorSetLayoutBinding scene_bindings[] = {
		global_uniform_binding,

		VkDescriptorSetLayoutBinding{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		},

		VkDescriptorSetLayoutBinding{
			.binding = 2,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		}
	};

	const VkDescriptorSetLayoutCreateInfo scene_layout_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount =
			static_cast<uint32_t>(std::size(scene_bindings)),
		.pBindings = scene_bindings
	};

	if (vkCreateDescriptorSetLayout(
		context.device,
		&scene_layout_info,
		nullptr,
		&resources.scene_set_layout
	) != VK_SUCCESS) {
		std::cerr << "Failed to create scene descriptor set layout\n";
		destroy_resources();
		return false;
	}

	constexpr uint32_t uniform_set_count =
		static_cast<uint32_t>(scene::object_count + 1);

	const VkDescriptorPoolSize pool_sizes[] = {
		{
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			uniform_set_count
		},
		{
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			2
		}
	};

	const VkDescriptorPoolCreateInfo pool_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = uniform_set_count,
		.poolSizeCount =
			static_cast<uint32_t>(std::size(pool_sizes)),
		.pPoolSizes = pool_sizes,
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

	const VkDescriptorSetAllocateInfo scene_allocate_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = resources.descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &resources.scene_set_layout,
	};

	if (vkAllocateDescriptorSets(
		context.device,
		&scene_allocate_info,
		&resources.scene.descriptor_set
	) != VK_SUCCESS) {
		std::cerr << "Failed to allocate scene descriptor set\n";
		destroy_resources();
		return false;
	}

	const VkDescriptorBufferInfo scene_buffer_info{
		.buffer = resources.scene.uniform_buffer,
		.offset = 0,
		.range = sizeof(scene::SceneUniformData),
	};

	const VkWriteDescriptorSet scene_write{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = resources.scene.descriptor_set,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &scene_buffer_info,
	};

	vkUpdateDescriptorSets(
		context.device,
		1,
		&scene_write,
		0,
		nullptr
	);

	update_storage_buffer_descriptor(
		resources.scene.point_lights,
		sizeof(scene::PointLightData),
		1
	);

	update_storage_buffer_descriptor(
		resources.scene.spot_lights,
		sizeof(scene::SpotLightData),
		2
	);

	const VkDescriptorSetAllocateInfo allocate_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = resources.descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &resources.object_set_layout,
	};

	for (auto& object_resources : resources.objects) {
		if (vkAllocateDescriptorSets(
			context.device,
			&allocate_info,
			&object_resources.descriptor_set
		) != VK_SUCCESS) {
			std::cerr << "Failed to allocate descriptor sets\n";
			destroy_resources();
			return false;
		}

		const VkDescriptorBufferInfo uniform_buffer_info{
			.buffer = object_resources.uniform_buffer,
			.offset = 0,
			.range = sizeof(scene::ObjectUniformData),
		};

		const VkWriteDescriptorSet write_descriptor_set{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = object_resources.descriptor_set,
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
	}

	const VkDescriptorSetLayout set_layouts[] = {
		resources.scene_set_layout,
		resources.object_set_layout,
	};

	const VkPipelineLayoutCreateInfo pipeline_layout_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount =
			static_cast<uint32_t>(std::size(set_layouts)),
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

	VkPipelineVertexInputStateCreateInfo vertex_input_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions =
			&octahedron::vertex_binding_description,
		.vertexAttributeDescriptionCount =
			static_cast<uint32_t>(
				octahedron::vertex_attribute_descriptions.size()
			),
		.pVertexAttributeDescriptions =
			octahedron::vertex_attribute_descriptions.data()
	};

	if (!pipeline_utils::create_graphics_pipeline(
		context.device,
		context.render_pass,
		resources.vertex_shader,
		resources.fragment_shader,
		vertex_input_info,
		resources.pipeline_layout,
		resources.pipeline
	)) {
		destroy_resources();
		return false;
	}

	if (!light_gizmos::initialize(
		resources.gizmos,
		context.device,
		context.allocator,
		context.render_pass,
		resources.scene_set_layout
	)) {
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


bool update_uniform_buffers(const scene::FrameUniformData& uniforms) {
	if (!ensure_storage_buffer_capacity(
		resources.scene.point_lights,
		uniforms.point_lights.size(),
		sizeof(scene::PointLightData),
		1
	)) {
		return false;
	}

	if (!ensure_storage_buffer_capacity(
		resources.scene.spot_lights,
		uniforms.spot_lights.size(),
		sizeof(scene::SpotLightData),
		2
	)) {
		return false;
	}

	if (!uniforms.point_lights.empty()) {
		if (!gpu_buffer::write_buffer(
			graphics::internal::context.allocator,
			resources.scene.point_lights.allocation,
			uniforms.point_lights.data(),
			uniforms.point_lights.size() *
				sizeof(scene::PointLightData)
		)) {
			return false;
		}
	}

	if (!uniforms.spot_lights.empty()) {
		if (!gpu_buffer::write_buffer(
			graphics::internal::context.allocator,
			resources.scene.spot_lights.allocation,
			uniforms.spot_lights.data(),
			uniforms.spot_lights.size() *
				sizeof(scene::SpotLightData)
		)) {
			return false;
		}
	}

    if (!gpu_buffer::write_buffer(
		graphics::internal::context.allocator,
        resources.scene.uniform_allocation,
        &uniforms.scene,
        sizeof(uniforms.scene)
    )) {
        return false;
    }

    for (std::size_t i = 0; i < resources.objects.size(); ++i) {
        if (!gpu_buffer::write_buffer(
			graphics::internal::context.allocator,
            resources.objects[i].uniform_allocation,
            &uniforms.objects[i],
            sizeof(uniforms.objects[i])
        )) {
            return false;
        }
    }

    return true;
}

void render(
    const graphics::internal::FrameData& fd,
    const scene::FrameUniformData& uniforms
) {
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

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipeline);

	const VkDeviceSize vertex_buffer_offset = 0;
	vkCmdBindVertexBuffers(
		command_buffer, 0, 1,
		&resources.vertex_buffer,
		&vertex_buffer_offset
	);
	vkCmdBindIndexBuffer(
		command_buffer,
		resources.index_buffer,
		0,
		VK_INDEX_TYPE_UINT16
	);

	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		resources.pipeline_layout,
		0,
		1,
		&resources.scene.descriptor_set,
		0,
		nullptr
	);

	for (const auto& object_resources : resources.objects) {
		vkCmdBindDescriptorSets(
			command_buffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			resources.pipeline_layout,
			1,
			1,
			&object_resources.descriptor_set,
			0, nullptr
		);

		vkCmdDrawIndexed(
			command_buffer,
			static_cast<uint32_t>(octahedron::indices.size()),
			1,
			0,
			0,
			0
		);
	}

	light_gizmos::render_light_markers(
		resources.gizmos,
		command_buffer,
		resources.scene.descriptor_set,
		uniforms.light_markers
	);

	light_gizmos::render_spot_light_cones(
		resources.gizmos,
		command_buffer,
		resources.scene.descriptor_set,
		uniforms.spot_light_cone_models
	);

	vkCmdEndRenderPass(command_buffer);
	if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
		std::cerr << "Failed to end command buffer\n";
		return;
	}
}

} // namespace scene_renderer
