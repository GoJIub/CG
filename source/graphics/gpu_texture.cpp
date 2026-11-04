#include "gpu_texture.hpp"
#include "gpu_buffer.hpp"

#include <exception>
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

    const VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage =
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
    }
}

void record_upload_rgba8(
    VkCommandBuffer command_buffer,
    VkBuffer staging_buffer,
    VkImage image,
    std::uint32_t width,
    std::uint32_t height
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
            .levelCount = 1,
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

    VkImageMemoryBarrier to_shader = to_transfer;
    to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &to_shader
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
    const void* pixels
) {
    if (image == VK_NULL_HANDLE ||
        pixels == nullptr ||
        width == 0 ||
        height == 0) {
        std::cerr << "Invalid texture upload arguments\n";
        return false;
    }

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_allocation = nullptr;
    VkCommandPool command_pool = VK_NULL_HANDLE;

    const auto cleanup = [&]() {
        if (command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, command_pool, nullptr);
        }

        if (staging_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(
                allocator,
                staging_buffer,
                staging_allocation
            );
        }
    };

    const VkDeviceSize byte_size =
        static_cast<VkDeviceSize>(width) * height * 4;

    if (!gpu_buffer::create_buffer(
        allocator,
        byte_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        pixels,
        staging_buffer,
        staging_allocation
    )) {
        return false;
    }

    const VkCommandPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = queue_family_index
    };

    VkResult result = vkCreateCommandPool(
        device,
        &pool_info,
        nullptr,
        &command_pool
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create upload command pool: "
                  << result << '\n';
        cleanup();
        return false;
    }

    const VkCommandBufferAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    result = vkAllocateCommandBuffers(
        device,
        &allocate_info,
        &command_buffer
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to allocate upload command buffer: "
                  << result << '\n';
        cleanup();
        return false;
    }

    const VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    result = vkBeginCommandBuffer(command_buffer, &begin_info);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to begin upload command buffer: "
                  << result << '\n';
        cleanup();
        return false;
    }

    record_upload_rgba8(
        command_buffer,
        staging_buffer,
        image,
        width,
        height
    );

    result = vkEndCommandBuffer(command_buffer);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to end upload command buffer: "
                  << result << '\n';
        cleanup();
        return false;
    }

    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer
    };

    result = vkQueueSubmit(
        queue,
        1,
        &submit_info,
        VK_NULL_HANDLE
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to submit texture upload: "
                  << result << '\n';
        cleanup();
        return false;
    }

    result = vkQueueWaitIdle(queue);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to wait for texture upload: "
                  << result << '\n';

        std::terminate();
    }

    cleanup();
    return true;
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
            .levelCount = 1,
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
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
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