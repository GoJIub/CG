#include "octahedron_mesh.hpp"

#include <cstddef>

namespace octahedron {

constexpr Vertex make_vertex(float x, float y, float z) {
    return Vertex{
        {x, y, z},
        {
            (x / octahedron_radius + 1.0f) * 0.5f,
            (y / octahedron_radius + 1.0f) * 0.5f,
            (z / octahedron_radius + 1.0f) * 0.5f
        }
    };
}

const std::array<Vertex, 6> vertices = {
    make_vertex(0.0f, octahedron_radius, 0.0f),
    make_vertex(octahedron_radius, 0.0f, 0.0f),
    make_vertex(0.0f, 0.0f, -octahedron_radius),
    make_vertex(-octahedron_radius, 0.0f, 0.0f),
    make_vertex(0.0f, 0.0f, octahedron_radius),
    make_vertex(0.0f, -octahedron_radius, 0.0f)
};

const std::array<uint16_t, 24> indices = {
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

} // namespace octahedron