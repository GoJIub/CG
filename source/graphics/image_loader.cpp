#include "image_loader.hpp"

#include <cstddef>
#include <iostream>
#include <memory>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace image_loader {

bool load_rgba8(
    const char* path,
    ImageData& out_image
) {
    int width = 0;
    int height = 0;

    using LoadedPixels = std::unique_ptr<
        stbi_uc,
        decltype(&stbi_image_free)
    >;

    LoadedPixels pixels(
        stbi_load(
            path,
            &width,
            &height,
            nullptr,
            STBI_rgb_alpha
        ),
        &stbi_image_free
    );

    if (!pixels) {
        const char* reason = stbi_failure_reason();

        std::cerr
            << "Failed to load image: " << path
            << " (" << (reason ? reason : "unknown error") << ")\n";

        return false;
    }

    if (width <= 0 || height <= 0) {
        std::cerr << "Invalid image dimensions: " << path << '\n';
        return false;
    }

    constexpr std::size_t channel_count = 4;

    const std::size_t byte_count =
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height) *
        channel_count;

    ImageData image;
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);

    image.pixels.assign(
        pixels.get(),
        pixels.get() + byte_count
    );

    out_image = std::move(image);
    return true;
}

} // namespace image_loader