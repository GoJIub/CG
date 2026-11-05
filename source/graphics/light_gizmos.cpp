#include "light_gizmos.hpp"
#include "gpu_upload.hpp"
#include "pipeline_utils.hpp"

#include "geometry/sphere_mesh.hpp"
#include "geometry/cone_mesh.hpp"

#include <cstdint>
#include <iterator>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

namespace light_gizmos {

bool initialize(
    Resources& gizmos,
    VkDevice device,
    VmaAllocator allocator,
    VkQueue queue,
    std::uint32_t queue_family_index,
    VkRenderPass render_pass,
    VkDescriptorSetLayout scene_set_layout,
    VkDescriptorSetLayout object_set_layout
) {
    gizmos.markers.vertex_shader =
		pipeline_utils::load_shader_module(device, "shaders/light_marker.vert.spv");

	if (gizmos.markers.vertex_shader == VK_NULL_HANDLE) {
		return false;
	}

	gizmos.markers.fragment_shader =
		pipeline_utils::load_shader_module(device, "shaders/light_marker.frag.spv");

	if (gizmos.markers.fragment_shader == VK_NULL_HANDLE) {
		return false;
	}

	if (!gpu_upload::create_geometry_buffer(
		device,
		allocator,
		queue,
		queue_family_index,
		sizeof(sphere::vertices),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		sphere::vertices.data(),
		gizmos.sphere_mesh.vertex_buffer,
		gizmos.sphere_mesh.vertex_allocation
	)) {
		return false;
	}

	if (!gpu_upload::create_geometry_buffer(
		device,
		allocator,
		queue,
		queue_family_index,
		sizeof(sphere::indices),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		sphere::indices.data(),
		gizmos.sphere_mesh.index_buffer,
		gizmos.sphere_mesh.index_allocation
	)) {
		return false;
	}

	if (!gpu_upload::create_geometry_buffer(
		device,
		allocator,
		queue,
		queue_family_index,
		sizeof(cone::vertices),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		cone::vertices.data(),
		gizmos.cone_vertices.buffer,
		gizmos.cone_vertices.allocation
	)) {
		return false;
	}

    const VkDescriptorSetLayout set_layouts[] = {
        scene_set_layout,
        object_set_layout
    };

    const VkPipelineLayoutCreateInfo marker_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount =
            static_cast<std::uint32_t>(std::size(set_layouts)),
        .pSetLayouts = set_layouts
    };

	if (vkCreatePipelineLayout(
		device,
		&marker_layout_info,
		nullptr,
		&gizmos.markers.pipeline_layout
	) != VK_SUCCESS) {
		std::cerr << "Failed to create marker pipeline layout\n";
		return false;
	}

    const VkPipelineVertexInputStateCreateInfo marker_vertex_input_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions =
			&sphere::vertex_binding_description,
		.vertexAttributeDescriptionCount =
			static_cast<std::uint32_t>(
				sphere::vertex_attribute_descriptions.size()
			),
		.pVertexAttributeDescriptions =
			sphere::vertex_attribute_descriptions.data()
	};

	const VkPipelineVertexInputStateCreateInfo cone_vertex_input_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &cone::vertex_binding_description,
		.vertexAttributeDescriptionCount =
			static_cast<std::uint32_t>(
				cone::vertex_attribute_descriptions.size()
			),
		.pVertexAttributeDescriptions =
			cone::vertex_attribute_descriptions.data()
	};

    if (!pipeline_utils::create_graphics_pipeline(
		device,
		render_pass,
		gizmos.markers.vertex_shader,
		gizmos.markers.fragment_shader,
		marker_vertex_input_info,
		gizmos.markers.pipeline_layout,
		gizmos.markers.pipeline
	)) {
		return false;
	}

	if (!pipeline_utils::create_graphics_pipeline(
		device,
		render_pass,
		gizmos.markers.vertex_shader,
		gizmos.markers.fragment_shader,
		cone_vertex_input_info,
		gizmos.markers.pipeline_layout,
		gizmos.markers.cone_pipeline,
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST
	)) {
		return false;
	}

    return true;
}

void destroy(
    Resources& gizmos,
    VkDevice device,
    VmaAllocator allocator
) {
    if (gizmos.markers.cone_pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(
			device,
			gizmos.markers.cone_pipeline,
			nullptr
		);
		gizmos.markers.cone_pipeline = VK_NULL_HANDLE;
	}

	if (gizmos.markers.pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(
			device,
			gizmos.markers.pipeline,
			nullptr
		);
		gizmos.markers.pipeline = VK_NULL_HANDLE;
	}
	if (gizmos.markers.pipeline_layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(
			device,
			gizmos.markers.pipeline_layout,
			nullptr
		);
		gizmos.markers.pipeline_layout = VK_NULL_HANDLE;
	}

    if (gizmos.cone_vertices.buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			allocator,
			gizmos.cone_vertices.buffer,
			gizmos.cone_vertices.allocation
		);
		gizmos.cone_vertices.buffer = VK_NULL_HANDLE;
		gizmos.cone_vertices.allocation = nullptr;
	}

	if (gizmos.sphere_mesh.vertex_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			allocator,
			gizmos.sphere_mesh.vertex_buffer,
			gizmos.sphere_mesh.vertex_allocation
		);

		gizmos.sphere_mesh.vertex_buffer = VK_NULL_HANDLE;
		gizmos.sphere_mesh.vertex_allocation = nullptr;
	}

	if (gizmos.sphere_mesh.index_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			allocator,
			gizmos.sphere_mesh.index_buffer,
			gizmos.sphere_mesh.index_allocation
		);

		gizmos.sphere_mesh.index_buffer = VK_NULL_HANDLE;
		gizmos.sphere_mesh.index_allocation = nullptr;
	}

    if (gizmos.markers.vertex_shader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(
			device,
			gizmos.markers.vertex_shader,
			nullptr
		);
		gizmos.markers.vertex_shader = VK_NULL_HANDLE;
	}

	if (gizmos.markers.fragment_shader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(
			device,
			gizmos.markers.fragment_shader,
			nullptr
		);
		gizmos.markers.fragment_shader = VK_NULL_HANDLE;
	}
}

void render_light_markers(
    const Resources& gizmos,
    VkCommandBuffer command_buffer,
    VkDescriptorSet scene_descriptor_set,
    VkDescriptorSet object_descriptor_set,
    std::size_t marker_count,
    std::size_t first_record,
    VkDeviceSize stride
) {
    if (marker_count == 0) {
        return;
    }

    vkCmdBindPipeline(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        gizmos.markers.pipeline
    );

    const VkDeviceSize vertex_offset = 0;

    vkCmdBindVertexBuffers(
        command_buffer,
        0,
        1,
        &gizmos.sphere_mesh.vertex_buffer,
        &vertex_offset
    );

    vkCmdBindIndexBuffer(
        command_buffer,
        gizmos.sphere_mesh.index_buffer,
        0,
        VK_INDEX_TYPE_UINT32
    );

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        gizmos.markers.pipeline_layout,
        0,
        1,
        &scene_descriptor_set,
        0,
        nullptr
    );

	for (std::size_t i = 0; i < marker_count; ++i) {
        const std::uint32_t dynamic_offset =
            static_cast<std::uint32_t>(
                stride * (first_record + i)
            );

        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            gizmos.markers.pipeline_layout,
            1,
            1,
            &object_descriptor_set,
            1,
            &dynamic_offset
        );

        vkCmdDrawIndexed(
            command_buffer,
            static_cast<std::uint32_t>(sphere::indices.size()),
            1,
            0,
            0,
            0
        );
    }
}

void render_spot_light_cones(
    const Resources& gizmos,
    VkCommandBuffer command_buffer,
    VkDescriptorSet scene_descriptor_set,
    VkDescriptorSet object_descriptor_set,
    std::size_t cone_count,
    std::size_t first_record,
    VkDeviceSize stride
) {
    if (cone_count == 0) {
        return;
    }

    vkCmdBindPipeline(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        gizmos.markers.cone_pipeline
    );

    const VkDeviceSize vertex_offset = 0;

    vkCmdBindVertexBuffers(
        command_buffer,
        0,
        1,
        &gizmos.cone_vertices.buffer,
        &vertex_offset
    );

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        gizmos.markers.pipeline_layout,
        0,
        1,
        &scene_descriptor_set,
        0,
        nullptr
    );

    for (std::size_t i = 0; i < cone_count; ++i) {
        const std::uint32_t dynamic_offset =
            static_cast<std::uint32_t>(
                stride * (first_record + i)
            );

        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            gizmos.markers.pipeline_layout,
            1,
            1,
            &object_descriptor_set,
            1,
            &dynamic_offset
        );

        vkCmdDraw(
            command_buffer,
            static_cast<std::uint32_t>(cone::vertices.size()),
            1,
            0,
            0
        );
    }
}

} // namespace light_gizmos