#pragma once

#include <cstdint>
#include <vector>

namespace image_loader {

struct ImageData {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;
};

bool load_rgba8(
    const char* path,
    ImageData& out_image
);

} // namespace image_loader