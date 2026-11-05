#include "floor_mesh.hpp"

namespace floor_mesh {

namespace {

constexpr float half_extent = 1.0f;
constexpr float texture_repeats = 4.0f;

} // namespace

const std::array<geometry::MeshVertex, 4> vertices{
    geometry::MeshVertex{
        {-half_extent, 0.0f, -half_extent},
        {0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    },
    geometry::MeshVertex{
        {-half_extent, 0.0f, half_extent},
        {0.0f, texture_repeats},
        {0.0f, 1.0f, 0.0f}
    },
    geometry::MeshVertex{
        {half_extent, 0.0f, half_extent},
        {texture_repeats, texture_repeats},
        {0.0f, 1.0f, 0.0f}
    },
    geometry::MeshVertex{
        {half_extent, 0.0f, -half_extent},
        {texture_repeats, 0.0f},
        {0.0f, 1.0f, 0.0f}
    }
};

const std::array<std::uint16_t, 6> indices{
    0, 1, 2,
    0, 2, 3
};

} // namespace floor_mesh