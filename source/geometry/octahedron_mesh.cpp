#include "octahedron_mesh.hpp"

#include <glm/vec3.hpp>
#include <glm/geometric.hpp>

#include <cstddef>

namespace octahedron {

constexpr Vertex make_vertex(float x, float y, float z) {
    return Vertex{
        {x, y, z},
        {
            (x / octahedron_radius + 1.0f) * 0.5f,
            (y / octahedron_radius + 1.0f) * 0.5f,
            (z / octahedron_radius + 1.0f) * 0.5f
        },
		{0.0f, 0.0f, 0.0f}
    };
}

const std::array<Vertex, 6> source_vertices = {
    make_vertex(0.0f, octahedron_radius, 0.0f),
    make_vertex(octahedron_radius, 0.0f, 0.0f),
    make_vertex(0.0f, 0.0f, -octahedron_radius),
    make_vertex(-octahedron_radius, 0.0f, 0.0f),
    make_vertex(0.0f, 0.0f, octahedron_radius),
    make_vertex(0.0f, -octahedron_radius, 0.0f)
};

const std::array<uint16_t, 24> source_indices = {
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

const std::array<VkVertexInputAttributeDescription, 3> vertex_attribute_descriptions = {
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
	},
	VkVertexInputAttributeDescription{
		.location = 2,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, normal)
	}
};


void set_normal(Vertex& vertex, const glm::vec3& normal) {
	for (int i = 0; i < 3; ++i) {
		vertex.normal[i] = normal[i];
	}
}

glm::vec3 position_of(const Vertex& vertex) {
    return {
        vertex.position[0],
        vertex.position[1],
        vertex.position[2]
    };
}

std::array<Vertex, 24> make_flat_vertices() {
    std::array<Vertex, 24> result{};

    for (size_t i = 0; i < source_indices.size(); i += 3) {
        auto a = source_vertices[source_indices[i]];
		auto b = source_vertices[source_indices[i + 1]];
		auto c = source_vertices[source_indices[i + 2]];

		const glm::vec3 edge_ab = position_of(b) - position_of(a);
		const glm::vec3 edge_ac = position_of(c) - position_of(a);

		const glm::vec3 normal =
			glm::normalize(glm::cross(edge_ab, edge_ac));

		set_normal(a, normal);
		set_normal(b, normal);
		set_normal(c, normal);

		result[i]     = a;
		result[i + 1] = b;
		result[i + 2] = c;
    }

    return result;
}

const std::array<Vertex, 24> vertices = make_flat_vertices();

std::array<uint16_t, 24> make_flat_indices() {
	std::array<uint16_t, 24> result{};

	for (size_t i = 0; i < source_indices.size(); ++i) {
		result[i] = static_cast<uint16_t>(i);
	}

	return result;
}

const std::array<uint16_t, 24> indices = make_flat_indices();

} // namespace octahedron