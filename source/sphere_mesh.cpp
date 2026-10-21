#include "sphere_mesh.hpp"

#include <cmath>
#include <glm/gtc/constants.hpp>

namespace sphere {
namespace {

std::array<Vertex, vertex_count> make_vertices() {
    std::array<Vertex, vertex_count> result{};

    for (std::uint32_t latitude = 0;
         latitude <= latitude_segments;
         ++latitude) {

        const float phi =
            glm::pi<float>() *
            static_cast<float>(latitude) /
            static_cast<float>(latitude_segments);

        const float sin_phi = std::sin(phi);
        const float cos_phi = std::cos(phi);

        for (std::uint32_t longitude = 0;
             longitude <= longitude_segments;
             ++longitude) {

            const float theta =
                glm::two_pi<float>() *
                static_cast<float>(longitude) /
                static_cast<float>(longitude_segments);

            const std::size_t index =
                latitude * (longitude_segments + 1) + longitude;

            result[index] = Vertex{{
                sin_phi * std::cos(theta),
                cos_phi,
                sin_phi * std::sin(theta)
            }};
        }
    }

    return result;
}

std::array<std::uint32_t, index_count> make_indices() {
    std::array<std::uint32_t, index_count> result{};
    std::size_t offset = 0;

    constexpr std::uint32_t row_stride =
        longitude_segments + 1;

    for (std::uint32_t latitude = 0;
         latitude < latitude_segments;
         ++latitude) {

        for (std::uint32_t longitude = 0;
             longitude < longitude_segments;
             ++longitude) {

            const std::uint32_t a =
                latitude * row_stride + longitude;

            const std::uint32_t b = a + row_stride;

            if (latitude != 0) {
                result[offset++] = a;
                result[offset++] = a + 1;
                result[offset++] = b;
            }

            if (latitude != latitude_segments - 1) {
                result[offset++] = a + 1;
                result[offset++] = b + 1;
                result[offset++] = b;
            }
        }
    }

    return result;
}

} // namespace

const std::array<Vertex, vertex_count> vertices =
    make_vertices();

const std::array<std::uint32_t, index_count> indices =
    make_indices();

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

} // namespace sphere