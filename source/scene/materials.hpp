#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace scene {

enum class MaterialId : std::uint32_t {
    ObsidianGold,
    LightStone,
    Checkerboard,
    Count
};

inline constexpr std::size_t material_count =
    static_cast<std::size_t>(MaterialId::Count);

struct MaterialDescription {
    const char* name;
    const char* albedo_path;
    const char* specular_path = nullptr;
    const char* emissive_path = nullptr;
};

inline constexpr auto material_descriptions =
    std::to_array<MaterialDescription>({
        {
            .name = "Obsidian gold",
            .albedo_path = "textures/obsidian_gold/albedo.png",
            .specular_path = "textures/obsidian_gold/specular.png",
            .emissive_path = "textures/obsidian_gold/emissive.png"
        },
        {
            .name = "Light stone",
            .albedo_path = "textures/light_stone/albedo.png"
        },
        {
            .name = "Checkerboard",
            .albedo_path = "textures/checkerboard/albedo.png"
        }
    });

static_assert(
    material_descriptions.size() == material_count
);

} // namespace scene