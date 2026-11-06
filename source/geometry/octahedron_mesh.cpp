#include "octahedron_mesh.hpp"

#include <glm/vec3.hpp>
#include <glm/geometric.hpp>

#include <cstddef>
#include <numbers>

namespace octahedron {

constexpr Vertex make_vertex(float x, float y, float z) {
    return Vertex{
        {x, y, z},
        {0.0f, 0.0f},
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
	constexpr float triangle_uv_height =
		std::numbers::sqrt3_v<float> * 0.5f;

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

		a.uv[0] = 0.0f;
		a.uv[1] = 0.0f;

		b.uv[0] = 1.0f;
		b.uv[1] = 0.0f;

		c.uv[0] = 0.5f;
		c.uv[1] = triangle_uv_height;

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