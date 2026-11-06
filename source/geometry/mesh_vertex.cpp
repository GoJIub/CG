#include "mesh_vertex.hpp"

#include <cstddef>

namespace geometry {

const VkVertexInputBindingDescription mesh_vertex_binding_description{
	.binding = 0,
	.stride = sizeof(MeshVertex),
	.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
};

const std::array<VkVertexInputAttributeDescription, 3> mesh_vertex_attribute_descriptions = {
	VkVertexInputAttributeDescription{
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(MeshVertex, position)
	},
	VkVertexInputAttributeDescription{
		.location = 1,
		.binding = 0,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(MeshVertex, uv)
	},
	VkVertexInputAttributeDescription{
		.location = 2,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(MeshVertex, normal)
	}
};

} // namespace geometry