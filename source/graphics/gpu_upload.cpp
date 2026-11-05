#include "gpu_upload.hpp"
#include "gpu_buffer.hpp"

#include <exception>
#include <iostream>
#include <limits>

namespace gpu_upload {

bool upload_staged(
    VkDevice device,
    VmaAllocator allocator,
    VkQueue queue,
    std::uint32_t queue_family_index,
    VkDeviceSize size,
    const void* source_data,
    const RecordCommands& record_commands
) {
    if (size == 0 ||
        source_data == nullptr ||
        !record_commands) {
        std::cerr << "Invalid staged upload arguments\n";
        return false;
    }

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_allocation = nullptr;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkFence upload_fence = VK_NULL_HANDLE;

    const auto cleanup = [&]() {
        if (upload_fence != VK_NULL_HANDLE) {
            vkDestroyFence(device, upload_fence, nullptr);
        }

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

    if (!gpu_buffer::create_buffer(
        allocator,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        source_data,
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

    record_commands(command_buffer, staging_buffer);

    result = vkEndCommandBuffer(command_buffer);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to end upload command buffer: "
                  << result << '\n';
        cleanup();
        return false;
    }

    const VkFenceCreateInfo fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0
    };

    result = vkCreateFence(
        device,
        &fence_info,
        nullptr,
        &upload_fence
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create upload fence: "
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
        upload_fence
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to submit upload: "
                  << result << '\n';
        cleanup();
        return false;
    }

    result = vkWaitForFences(
        device,
        1,
        &upload_fence,
        VK_TRUE,
        std::numeric_limits<std::uint64_t>::max()
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to wait for upload: "
                  << result << '\n';

        std::terminate();
    }

    cleanup();
    return true;
}

bool create_geometry_buffer(
    VkDevice device,
    VmaAllocator allocator,
    VkQueue queue,
    std::uint32_t queue_family_index,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    const void* source_data,
    VkBuffer& out_buffer,
    VmaAllocation& out_allocation
) {
    if (size == 0 ||
        source_data == nullptr ||
        out_buffer != VK_NULL_HANDLE ||
        out_allocation != nullptr) {
        std::cerr << "Invalid geometry buffer arguments\n";
        return false;
    }

    VkBuffer new_buffer = VK_NULL_HANDLE;
    VmaAllocation new_allocation = nullptr;

    if (!gpu_buffer::create_device_buffer(
        allocator,
        size,
        usage,
        new_buffer,
        new_allocation
    )) {
        return false;
    }

    const bool uploaded = upload_staged(
        device,
        allocator,
        queue,
        queue_family_index,
        size,
        source_data,
        [new_buffer, size](
            VkCommandBuffer command_buffer,
            VkBuffer staging_buffer
        ) {
            gpu_buffer::record_geometry_upload(
                command_buffer,
                staging_buffer,
                new_buffer,
                size
            );
        }
    );

    if (!uploaded) {
        vmaDestroyBuffer(
            allocator,
            new_buffer,
            new_allocation
        );
        return false;
    }

    out_buffer = new_buffer;
    out_allocation = new_allocation;
    return true;
}

} // namespace gpu_upload
