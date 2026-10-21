#include "cone_mesh.hpp"

#include <cmath>
#include <glm/gtc/constants.hpp>

namespace cone {
namespace {

std::array<Vertex, vertex_count> make_vertices() {
    std::array<Vertex, vertex_count> result{};
    std::size_t offset = 0;

    for (std::uint32_t i = 0; i < circle_segments; ++i) {
        const float angle_a =
            glm::two_pi<float>() *
            static_cast<float>(i) /
            static_cast<float>(circle_segments);

        const float angle_b =
            glm::two_pi<float>() *
            static_cast<float>(i + 1) /
            static_cast<float>(circle_segments);

        result[offset++] = Vertex{{
            std::cos(angle_a), std::sin(angle_a), 1.0f
        }};

        result[offset++] = Vertex{{
            std::cos(angle_b), std::sin(angle_b), 1.0f
        }};
    }

    for (std::uint32_t i = 0; i < side_line_count; ++i) {
        const float angle =
            glm::two_pi<float>() *
            static_cast<float>(i) /
            static_cast<float>(side_line_count);

        result[offset++] = Vertex{{0.0f, 0.0f, 0.0f}};

        result[offset++] = Vertex{{
            std::cos(angle), std::sin(angle), 1.0f
        }};
    }

    result[offset++] = Vertex{{0.0f, 0.0f, 0.0f}};
    result[offset++] = Vertex{{0.0f, 0.0f, 1.0f}};

    return result;
}

} // namespace

const std::array<Vertex, vertex_count> vertices =
    make_vertices();

const VkVertexInputBindingDescription vertex_binding_description{
    .binding = 0,
    .stride = sizeof(Vertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
};

const std::array<VkVertexInputAttributeDescription, 1>
vertex_attribute_descriptions = {
    VkVertexInputAttributeDescription{
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, position)
    }
};

} // namespace cone