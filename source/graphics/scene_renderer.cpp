#include "scene_renderer.hpp"
#include "gpu_buffer.hpp"
#include "gpu_upload.hpp"
#include "light_gizmos.hpp"
#include "materials_gpu.hpp"
#include "pipeline_utils.hpp"

#include "geometry/floor_mesh.hpp"
#include "geometry/mesh_vertex.hpp"
#include "geometry/octahedron_mesh.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <iostream>
#include <limits>
#include <vector>

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

struct ObjectUniformResources {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VkDeviceSize stride = 0;

    std::vector<std::byte> upload_data;

	std::size_t capacity = 0;
    std::size_t max_capacity = 0;
};

struct IndexedMeshResources {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_allocation = nullptr;

    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation index_allocation = nullptr;
};

struct UniformRecordLayout {
    std::size_t floor_record = 0;
    std::size_t first_sphere_record = 0;
    std::size_t first_cone_record = 0;
    std::size_t record_count = 0;
};

struct Resources {
	VkShaderModule vertex_shader = VK_NULL_HANDLE;
	VkShaderModule fragment_shader = VK_NULL_HANDLE;

	IndexedMeshResources floor_mesh;
    IndexedMeshResources octahedron_mesh;

	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	VkDescriptorSetLayout scene_set_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout object_set_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout marker_set_layout = VK_NULL_HANDLE;

    VkDescriptorSet marker_descriptor_set = VK_NULL_HANDLE;

	VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

    materials_gpu::Resources material_resources;
	ObjectUniformResources object_uniforms;

	SceneResources scene{};

	light_gizmos::Resources gizmos;
};

Resources resources;

UniformRecordLayout compute_uniform_record_layout(
    const scene::FrameUniformData& uniforms
) {
    assert(
        uniforms.marker_objects.size() ==
        uniforms.light_markers.size() +
        uniforms.spot_light_cone_models.size()
    );

    UniformRecordLayout layout{};

    layout.floor_record = uniforms.objects.size();

    layout.first_sphere_record =
        layout.floor_record + 1;

    layout.first_cone_record =
        layout.first_sphere_record +
        uniforms.light_markers.size();

    layout.record_count =
        layout.first_sphere_record +
        uniforms.marker_objects.size();

    return layout;
}

void destroy_indexed_mesh(
    VmaAllocator allocator,
    IndexedMeshResources& mesh
) {
    if (mesh.vertex_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(
            allocator,
            mesh.vertex_buffer,
            mesh.vertex_allocation
        );
    }

    if (mesh.index_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(
            allocator,
            mesh.index_buffer,
            mesh.index_allocation
        );
    }

    mesh = {};
}

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
        for (auto& material : resources.material_resources.materials) {
            material.descriptor_set = VK_NULL_HANDLE;
        }
		resources.scene.descriptor_set = VK_NULL_HANDLE;
		resources.marker_descriptor_set = VK_NULL_HANDLE;
	}

    materials_gpu::destroy_textures(
        context.device,
        context.allocator,
        resources.material_resources
    );

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

    if (resources.marker_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(
            context.device,
            resources.marker_set_layout,
            nullptr
        );

        resources.marker_set_layout = VK_NULL_HANDLE;
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

    destroy_indexed_mesh(
        context.allocator,
        resources.floor_mesh
    );

    if (resources.object_uniforms.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(
            context.allocator,
            resources.object_uniforms.buffer,
            resources.object_uniforms.allocation
        );

        resources.object_uniforms = {};
    }

    destroy_indexed_mesh(
        context.allocator,
        resources.octahedron_mesh
    );

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

void update_object_uniform_descriptor(
    VkDescriptorSet descriptor_set,
    VkBuffer buffer
) {
    const VkDescriptorBufferInfo buffer_info{
        .buffer = buffer,
        .offset = 0,
        .range = sizeof(scene::ObjectUniformData)
    };

    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType =
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .pBufferInfo = &buffer_info
    };

    vkUpdateDescriptorSets(
        graphics::internal::context.device,
        1,
        &write,
        0,
        nullptr
    );
}

bool ensure_object_uniform_capacity(std::size_t required_count) {
    auto& object_uniforms = resources.object_uniforms;

    if (object_uniforms.stride == 0 ||
        object_uniforms.max_capacity == 0) {
        std::cerr << "Object uniform buffer is not initialized\n";
        return false;
    }

    if (required_count > object_uniforms.max_capacity) {
        std::cerr << "Requested uniform capacity exceeds limits\n";
        return false;
    }

    if (required_count <= object_uniforms.capacity) {
        return true;
    }

    auto& context = graphics::internal::context;

    const std::size_t doubled_capacity =
        object_uniforms.capacity >
            object_uniforms.max_capacity / 2
            ? object_uniforms.max_capacity
            : object_uniforms.capacity * 2;

    const std::size_t new_capacity =
        std::max(required_count, doubled_capacity);

    const VkDeviceSize new_size =
        object_uniforms.stride * new_capacity;

    VkBuffer new_buffer = VK_NULL_HANDLE;
    VmaAllocation new_allocation = nullptr;

    if (!gpu_buffer::create_buffer(
        context.allocator,
        new_size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        nullptr,
        new_buffer,
        new_allocation
    )) {
        return false;
    }

    materials_gpu::update_uniform_buffer_descriptors(
        context.device,
        new_buffer,
        resources.material_resources
    );

    update_object_uniform_descriptor(
        resources.marker_descriptor_set,
        new_buffer
    );

    vmaDestroyBuffer(
        context.allocator,
        object_uniforms.buffer,
        object_uniforms.allocation
    );

    object_uniforms.buffer = new_buffer;
    object_uniforms.allocation = new_allocation;
    object_uniforms.capacity = new_capacity;

    object_uniforms.upload_data.resize(
        static_cast<std::size_t>(new_size)
    );

    return true;
}

} // namespace


namespace scene_renderer {

bool initialize() {
	auto& context = graphics::internal::context;

	resources.vertex_shader = pipeline_utils::load_shader_module(context.device, "shaders/mesh.vert.spv");
	if (resources.vertex_shader == VK_NULL_HANDLE) {
		return false;
	}

	resources.fragment_shader = pipeline_utils::load_shader_module(context.device, "shaders/mesh.frag.spv");
	if (resources.fragment_shader == VK_NULL_HANDLE) {
		destroy_resources();
		return false;
	}

    if (!materials_gpu::load_textures(
        context,
        resources.material_resources
    )) {
        destroy_resources();
        return false;
    }

	if (!gpu_upload::create_geometry_buffer(
		context.device,
		context.allocator,
		context.graphics_queue,
		context.graphics_queue_index,
		sizeof(octahedron::vertices),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		octahedron::vertices.data(),
		resources.octahedron_mesh.vertex_buffer,
		resources.octahedron_mesh.vertex_allocation
	)) {
		destroy_resources();
		return false;
	}

	if (!gpu_upload::create_geometry_buffer(
		context.device,
		context.allocator,
		context.graphics_queue,
		context.graphics_queue_index,
		sizeof(octahedron::indices),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		octahedron::indices.data(),
		resources.octahedron_mesh.index_buffer,
		resources.octahedron_mesh.index_allocation
	)) {
		destroy_resources();
		return false;
	}

    if (!gpu_upload::create_geometry_buffer(
        context.device,
        context.allocator,
        context.graphics_queue,
        context.graphics_queue_index,
        sizeof(floor_mesh::vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        floor_mesh::vertices.data(),
        resources.floor_mesh.vertex_buffer,
        resources.floor_mesh.vertex_allocation
    )) {
        destroy_resources();
        return false;
    }

    if (!gpu_upload::create_geometry_buffer(
        context.device,
        context.allocator,
        context.graphics_queue,
        context.graphics_queue_index,
        sizeof(floor_mesh::indices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        floor_mesh::indices.data(),
        resources.floor_mesh.index_buffer,
        resources.floor_mesh.index_allocation
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

	VkPhysicalDeviceProperties device_properties{};
    vkGetPhysicalDeviceProperties(
        context.physical_device,
        &device_properties
    );

    const VkDeviceSize alignment =
        device_properties.limits.minUniformBufferOffsetAlignment;

    const VkDeviceSize object_size =
        sizeof(scene::ObjectUniformData);

    if (object_size >
        device_properties.limits.maxUniformBufferRange) {
        std::cerr << "Object uniform record exceeds device limit\n";
        destroy_resources();
        return false;
    }

    resources.object_uniforms.stride =
        ((object_size + alignment - 1) / alignment) * alignment;

    const VkDeviceSize stride =
        resources.object_uniforms.stride;

    const VkDeviceSize max_dynamic_offset =
        std::numeric_limits<std::uint32_t>::max();

    const VkDeviceSize max_records_by_offset =
        max_dynamic_offset / stride + 1;

    const VkDeviceSize max_records_by_buffer_size =
        std::numeric_limits<VkDeviceSize>::max() / stride;

    const VkDeviceSize max_records_by_upload_size =
        static_cast<VkDeviceSize>(
            resources.object_uniforms.upload_data.max_size()
        ) / stride;

    resources.object_uniforms.max_capacity =
        static_cast<std::size_t>(
            std::min({
                max_records_by_offset,
                max_records_by_buffer_size,
                max_records_by_upload_size
            })
        );

    if (scene::object_count >
        resources.object_uniforms.max_capacity) {
        std::cerr << "Initial uniform capacity exceeds limits\n";
        destroy_resources();
        return false;
    }

    const VkDeviceSize buffer_size =
        resources.object_uniforms.stride * scene::object_count;

    if (!gpu_buffer::create_buffer(
        context.allocator,
        buffer_size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        nullptr,
        resources.object_uniforms.buffer,
        resources.object_uniforms.allocation
    )) {
        destroy_resources();
        return false;
    }

    resources.object_uniforms.upload_data.resize(
        static_cast<std::size_t>(buffer_size)
    );

	resources.object_uniforms.capacity = scene::object_count;

	const VkDescriptorSetLayoutBinding global_uniform_binding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags =
			VK_SHADER_STAGE_VERTEX_BIT |
			VK_SHADER_STAGE_FRAGMENT_BIT
	};

    const VkDescriptorSetLayoutBinding object_uniform_binding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount = 1,
        .stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT |
            VK_SHADER_STAGE_FRAGMENT_BIT
    };

    const VkDescriptorSetLayoutBinding object_bindings[] = {
        object_uniform_binding,

        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        },

		VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        },

        VkDescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        }
    };

	const VkDescriptorSetLayoutCreateInfo set_layout_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount =
            static_cast<uint32_t>(std::size(object_bindings)),
        .pBindings = object_bindings
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

    const VkDescriptorSetLayoutCreateInfo marker_layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &object_uniform_binding
    };

    if (vkCreateDescriptorSetLayout(
        context.device,
        &marker_layout_info,
        nullptr,
        &resources.marker_set_layout
    ) != VK_SUCCESS) {
        std::cerr << "Failed to create marker descriptor layout\n";
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
        static_cast<uint32_t>(scene::material_count + 2);

    const VkDescriptorPoolSize pool_sizes[] = {
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1
        },
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            static_cast<uint32_t>(scene::material_count + 1)
        },
        {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            2
        },
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            static_cast<uint32_t>(scene::material_count * 3)
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

    if (!materials_gpu::create_descriptor_sets(
        context.device,
        resources.descriptor_pool,
        resources.object_set_layout,
        resources.object_uniforms.buffer,
        resources.material_resources
    )) {
        destroy_resources();
        return false;
    }

    const VkDescriptorSetAllocateInfo marker_allocate_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = resources.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &resources.marker_set_layout
    };

    if (vkAllocateDescriptorSets(
        context.device,
        &marker_allocate_info,
        &resources.marker_descriptor_set
    ) != VK_SUCCESS) {
        std::cerr << "Failed to allocate marker descriptor set\n";
        destroy_resources();
        return false;
    }

    update_object_uniform_descriptor(
        resources.marker_descriptor_set,
        resources.object_uniforms.buffer
    );

	const VkDescriptorSetLayout set_layouts[] = {
		resources.scene_set_layout,
		resources.object_set_layout,
	};

	const VkPushConstantRange texture_push_range{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(scene::TexturePushConstants)
    };

	const VkPipelineLayoutCreateInfo pipeline_layout_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount =
			static_cast<uint32_t>(std::size(set_layouts)),
		.pSetLayouts = set_layouts,
		.pushConstantRangeCount = 1,
        .pPushConstantRanges = &texture_push_range
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
			&geometry::mesh_vertex_binding_description,
		.vertexAttributeDescriptionCount =
			static_cast<uint32_t>(
				geometry::mesh_vertex_attribute_descriptions.size()
			),
		.pVertexAttributeDescriptions =
			geometry::mesh_vertex_attribute_descriptions.data()
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
		context.graphics_queue,
		context.graphics_queue_index,
		context.render_pass,
		resources.scene_set_layout,
        resources.marker_set_layout
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
    const auto record_layout =
        compute_uniform_record_layout(uniforms);

    if (!ensure_object_uniform_capacity(
        record_layout.record_count
    )) {
        return false;
    }

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

    auto& object_uniforms = resources.object_uniforms;

    for (std::size_t i = 0; i < uniforms.objects.size(); ++i) {
        const std::size_t offset = static_cast<std::size_t>(
            object_uniforms.stride * i
        );

        std::memcpy(
            object_uniforms.upload_data.data() + offset,
            &uniforms.objects[i],
            sizeof(uniforms.objects[i])
        );
    }

    const std::size_t floor_offset =
        static_cast<std::size_t>(
            object_uniforms.stride *
            record_layout.floor_record
        );

    std::memcpy(
        object_uniforms.upload_data.data() + floor_offset,
        &uniforms.floor,
        sizeof(uniforms.floor)
    );

	for (std::size_t i = 0;
         i < uniforms.marker_objects.size();
         ++i) {
        const std::size_t record_index =
            record_layout.first_sphere_record + i;

        const std::size_t offset =
            static_cast<std::size_t>(
                object_uniforms.stride * record_index
            );

        std::memcpy(
            object_uniforms.upload_data.data() + offset,
            &uniforms.marker_objects[i],
            sizeof(uniforms.marker_objects[i])
        );
    }

    if (!gpu_buffer::write_buffer(
        graphics::internal::context.allocator,
        object_uniforms.allocation,
        object_uniforms.upload_data.data(),
        object_uniforms.upload_data.size()
    )) {
        return false;
    }

    return true;
}

void render(
    const graphics::internal::FrameData& fd,
    const scene::FrameUniformData& uniforms
) {
	auto& context = graphics::internal::context;

    const auto record_layout =
        compute_uniform_record_layout(uniforms);

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
		&resources.octahedron_mesh.vertex_buffer,
		&vertex_buffer_offset
	);
	vkCmdBindIndexBuffer(
		command_buffer,
		resources.octahedron_mesh.index_buffer,
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

    for (std::size_t i = 0; i < uniforms.objects.size(); ++i) {
        const std::size_t material_index =
            static_cast<std::size_t>(uniforms.material_ids[i]);

        const auto& material =
            resources.material_resources.materials.at(material_index);

        const uint32_t dynamic_offset =
            static_cast<uint32_t>(
                resources.object_uniforms.stride * i
            );

        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            resources.pipeline_layout,
            1,
            1,
            &material.descriptor_set,
            1,
            &dynamic_offset
        );

        const auto& texture_effect = uniforms.texture_effects[i];

        vkCmdPushConstants(
            command_buffer,
            resources.pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(texture_effect),
            &texture_effect
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

    vkCmdBindVertexBuffers(
        command_buffer,
        0,
        1,
        &resources.floor_mesh.vertex_buffer,
        &vertex_buffer_offset
    );

    vkCmdBindIndexBuffer(
        command_buffer,
        resources.floor_mesh.index_buffer,
        0,
        VK_INDEX_TYPE_UINT16
    );

    const auto& floor_material = resources.material_resources.materials.at(
        static_cast<std::size_t>(uniforms.floor_material_id)
    );

    const std::uint32_t floor_dynamic_offset =
        static_cast<std::uint32_t>(
            resources.object_uniforms.stride *
            record_layout.floor_record
        );

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        resources.pipeline_layout,
        1,
        1,
        &floor_material.descriptor_set,
        1,
        &floor_dynamic_offset
    );

    const scene::TexturePushConstants floor_effect{};

    vkCmdPushConstants(
        command_buffer,
        resources.pipeline_layout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(floor_effect),
        &floor_effect
    );

    vkCmdDrawIndexed(
        command_buffer,
        static_cast<std::uint32_t>(floor_mesh::indices.size()),
        1,
        0,
        0,
        0
    );

    light_gizmos::render_light_markers(
        resources.gizmos,
        command_buffer,
        resources.scene.descriptor_set,
        resources.marker_descriptor_set,
        uniforms.light_markers.size(),
        record_layout.first_sphere_record,
        resources.object_uniforms.stride
    );

    light_gizmos::render_spot_light_cones(
        resources.gizmos,
        command_buffer,
        resources.scene.descriptor_set,
        resources.marker_descriptor_set,
        uniforms.spot_light_cone_models.size(),
        record_layout.first_cone_record,
        resources.object_uniforms.stride
    );

	vkCmdEndRenderPass(command_buffer);
	if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
		std::cerr << "Failed to end command buffer\n";
		return;
	}
}

} // namespace scene_renderer
