#include "gpu_texture.hpp"
#include "gpu_upload.hpp"

#include <algorithm>
#include <bit>
#include <iostream>

namespace gpu_texture {

bool create_image(
    VmaAllocator allocator,
    std::uint32_t width,
    std::uint32_t height,
    VkFormat format,
    Resources& texture
) {
    if (width == 0 || height == 0) {
        std::cerr << "Texture dimensions must be nonzero\n";
        return false;
    }

    if (texture.image != VK_NULL_HANDLE ||
        texture.allocation != nullptr ||
        texture.view != VK_NULL_HANDLE ||
        texture.sampler != VK_NULL_HANDLE) {
        std::cerr << "Texture resources are already initialized\n";
        return false;
    }

    const std::uint32_t mip_levels =
        static_cast<std::uint32_t>(
            std::bit_width(std::max(width, height))
        );

    const VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = mip_levels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage =
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    const VmaAllocationCreateInfo allocation_info{
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    VkImage new_image = VK_NULL_HANDLE;
    VmaAllocation new_allocation = nullptr;

    const VkResult result = vmaCreateImage(
        allocator,
        &image_info,
        &allocation_info,
        &new_image,
        &new_allocation,
        nullptr
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create texture image: "
                  << result << '\n';
        return false;
    }

    texture.image = new_image;
    texture.allocation = new_allocation;
    texture.mip_levels = mip_levels;

    return true;
}

void destroy(
    VkDevice device,
    VmaAllocator allocator,
    Resources& texture
) {
    if (texture.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, texture.sampler, nullptr);
        texture.sampler = VK_NULL_HANDLE;
    }

    if (texture.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, texture.view, nullptr);
        texture.view = VK_NULL_HANDLE;
    }

    if (texture.image != VK_NULL_HANDLE) {
        vmaDestroyImage(
            allocator,
            texture.image,
            texture.allocation
        );

        texture.image = VK_NULL_HANDLE;
        texture.allocation = nullptr;
        texture.mip_levels = 1;
    }
}

void record_upload_rgba8(
    VkCommandBuffer command_buffer,
    VkBuffer staging_buffer,
    VkImage image,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t mip_levels
) {
    const VkImageMemoryBarrier to_transfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = mip_levels,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &to_transfer
    );

    const VkBufferImageCopy copy_region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };

    vkCmdCopyBufferToImage(
        command_buffer,
        staging_buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copy_region
    );

    std::int32_t mip_width =
        static_cast<std::int32_t>(width);

    std::int32_t mip_height =
        static_cast<std::int32_t>(height);

    VkImageMemoryBarrier barrier = to_transfer;
    barrier.subresourceRange.levelCount = 1;

    for (std::uint32_t level = 1;
         level < mip_levels;
         ++level) {
        barrier.subresourceRange.baseMipLevel = level - 1;

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        const std::int32_t next_width =
            std::max(mip_width / 2, 1);

        const std::int32_t next_height =
            std::max(mip_height / 2, 1);

        const VkImageBlit blit_region{
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = level - 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .srcOffsets = {
                {0, 0, 0},
                {mip_width, mip_height, 1}
            },
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = level,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .dstOffsets = {
                {0, 0, 0},
                {next_width, next_height, 1}
            }
        };

        vkCmdBlitImage(
            command_buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blit_region,
            VK_FILTER_LINEAR
        );

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        mip_width = next_width;
        mip_height = next_height;
    }

    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

bool upload_rgba8(
    VkDevice device,
    VmaAllocator allocator,
    VkQueue queue,
    std::uint32_t queue_family_index,
    VkImage image,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t mip_levels,
    const void* pixels
) {
    if (image == VK_NULL_HANDLE ||
        pixels == nullptr ||
        width == 0 ||
        height == 0 ||
        mip_levels == 0) {
        std::cerr << "Invalid texture upload arguments\n";
        return false;
    }

    const VkDeviceSize byte_size =
        static_cast<VkDeviceSize>(width) * height * 4;

    return gpu_upload::upload_staged(
        device,
        allocator,
        queue,
        queue_family_index,
        byte_size,
        pixels,
        [image, width, height, mip_levels](
            VkCommandBuffer command_buffer,
            VkBuffer staging_buffer
        ) {
            record_upload_rgba8(
                command_buffer,
                staging_buffer,
                image,
                width,
                height,
                mip_levels
            );
        }
    );
}

bool create_view(
    VkDevice device,
    VkFormat format,
    Resources& texture
) {
    if (texture.image == VK_NULL_HANDLE ||
        texture.view != VK_NULL_HANDLE) {
        std::cerr << "Invalid texture state for creating view\n";
        return false;
    }

    const VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = texture.mip_levels,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkImageView new_view = VK_NULL_HANDLE;

    const VkResult result = vkCreateImageView(
        device,
        &view_info,
        nullptr,
        &new_view
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create texture image view: "
                  << result << '\n';
        return false;
    }

    texture.view = new_view;
    return true;
}

bool create_sampler(
    VkDevice device,
    float anisotropy,
    Resources& texture
) {
    if (texture.sampler != VK_NULL_HANDLE) {
        std::cerr << "Texture sampler is already initialized\n";
        return false;
    }

    const VkSamplerCreateInfo sampler_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable =
            anisotropy > 1.0f ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = anisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = static_cast<float>(texture.mip_levels - 1),
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };

    VkSampler new_sampler = VK_NULL_HANDLE;

    const VkResult result = vkCreateSampler(
        device,
        &sampler_info,
        nullptr,
        &new_sampler
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create texture sampler: "
                  << result << '\n';
        return false;
    }

    texture.sampler = new_sampler;
    return true;
}

} // namespace gpu_texture