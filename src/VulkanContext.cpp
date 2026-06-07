// VMA and its implementation must be in exactly one .cpp
#define VMA_IMPLEMENTATION
#include <SDL3/SDL_error.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "VulkanContext.hpp"
#include <fmt/core.h>
#include <fmt/format.h>
#include <stdexcept>

//=====Helpers==========================
#define VK_CHECK(expr)                                                         \
  do {                                                                         \
    VkResult _r = (expr);                                                      \
    if (_r != VK_SUCCESS)                                                      \
      throw std::runtime_error(fmt::format("Vulkan error {} at {}:{}",         \
                                           (int)_r, __FILE__, __LINE__));      \
  } while (0)

//===Public API=====================================
void VulkanContext::init(SDL_Window *win, int w, int h) {
  window = win;
  winWidth = w;
  winHeight = h;

  createInstance();
  createSurface();
  pickDeviceAndCreate();
  createAllocator();
  buildSwapchain();
  createSwapViews();
  createFrameData();

  fmt::print("[Vulkan] initialised - {}*{}\n", w, h);
}

void VulkanContext::cleanup() {
  vkDeviceWaitIdle(device);

  for (auto &f : frames) {
    vkDestroyCommandPool(device, f.cmdPool, nullptr);
    vkDestroySemaphore(device, f.imgReady, nullptr);
    vkDestroySemaphore(device, f.rendered, nullptr);
    vkDestroyFence(device, f.fence, nullptr);
  }

  destroySwapchain();
  vmaDestroyAllocator(allocator);

  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkb::destroy_debug_utils_messenger(instance, debugMessenger);
  vkDestroyInstance(instance, nullptr);
}

void VulkanContext::recreateSwapchain() {
  vkDeviceWaitIdle(device);
  destroySwapchain();
  buildSwapchain();
  createSwapViews();
  fmt::print("[Vulkan] swapchain recreated - {}x{}\n", swapExtent.width,
             swapExtent.height);
}

//===Image trnsation (synchronization2, core in VK 1.3)------
void VulkanContext::transitionImage(VkCommandBuffer cmd, VkImage image,
                                    VkImageLayout from,
                                    VkImageLayout to) const {
  VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  b.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
  b.oldLayout = from;
  b.newLayout = to;
  b.image = image;
  b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &b;
  vkCmdPipelineBarrier2(cmd, &dep);
}

//=====Private init steps===================
void VulkanContext::createInstance() {
  auto result =
      vkb::InstanceBuilder{}
          .set_app_name("SnakeVulkan")
          .set_engine_name("SnakeEngine")
          .request_validation_layers(true) // disable in Release id desired
          .use_default_debug_messenger()
          .require_api_version(1, 3, 0)
          .build();

  if (!result)
    throw std::runtime_error("vk-bootstrap: " + result.error().message());

  vkbInst = result.value();
  instance = vkbInst.instance;
  debugMessenger = vkbInst.debug_messenger;
}

void VulkanContext::createSurface() {
  if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface))
    throw std::runtime_error(std::string("SDL_Vulkan_Createface: ") +
                             SDL_GetError());
}

void VulkanContext::pickDeviceAndCreate() {
  // Require dynamic rendering + synchronization2 (both core in Vulkan 1.3)
  VkPhysicalDeviceVulkan13Features f13{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  f13.dynamicRendering = VK_TRUE;
  f13.synchronization2 = VK_TRUE;

  auto phys = vkb::PhysicalDeviceSelector{vkbInst}
                  .set_surface(surface)
                  .set_minimum_version(1, 3)
                  .set_required_features_13(f13)
                  .select();
  if (!phys)
    throw std::runtime_error("vk-bootstrap: " + phys.error().message());

  physDevice = phys.value().physical_device;
  fmt::print("[Vulkan] GPU: {}\n", phys.value().properties.deviceName);

  auto dev = vkb::DeviceBuilder{phys.value()}.build();

  if (!dev)
    throw std::runtime_error("vk-boostrap: " + dev.error().message());

  vkDev = dev.value();
  device = vkDev.device;

  graphicsQueue = vkDev.get_queue(vkb::QueueType::graphics).value();
  graphicsFamily = vkDev.get_queue_index(vkb::QueueType::graphics).value();
  presentQueue = vkDev.get_queue(vkb::QueueType::present).value();
  presentFamily = vkDev.get_queue_index(vkb::QueueType::present).value();
}

void VulkanContext::createAllocator() {
  VmaVulkanFunctions vf{};
  vf.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vf.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo ai{};
  ai.physicalDevice = physDevice;
  ai.device = device;
  ai.instance = instance;
  ai.vulkanApiVersion = VK_API_VERSION_1_3;
  ai.pVulkanFunctions = &vf;

  VK_CHECK(vmaCreateAllocator(&ai, &allocator));
}

void VulkanContext::buildSwapchain() {
  auto swap = vkb::SwapchainBuilder{vkDev, surface}
                  .use_default_format_selection()
                  .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // vsync
                  .set_desired_extent(winWidth, winHeight)
                  .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                  .build();
  if (!swap)
    throw std::runtime_error("vk-bootstrap: " + swap.error().message());

  vkbSwap = swap.value();
  swapchain = vkbSwap.swapchain;
  swapFormat = vkbSwap.image_format;
  swapExtent = vkbSwap.extent;
  swapImages = vkbSwap.get_images().value();
}
void VulkanContext::createSwapViews() {
  swapViews.resize(swapImages.size());
  for (size_t i{}; i < swapImages.size(); ++i) {
    VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image = swapImages[i];
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = swapFormat;
    ci.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};

    ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(device, &ci, nullptr, &swapViews[i]));
  }
}

void VulkanContext::createFrameData() {
  for (auto &f : frames) {
    VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = graphicsFamily;
    VK_CHECK(vkCreateCommandPool(device, &ci, nullptr, &f.cmdPool));

    VkCommandBufferAllocateInfo ai{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = f.cmdPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(device, &ai, &f.cmd));

    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_CHECK(vkCreateSemaphore(device, &si, nullptr, &f.imgReady));
    VK_CHECK(vkCreateSemaphore(device, &si, nullptr, &f.rendered));

    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(device, &fi, nullptr, &f.fence));
  }
}

void VulkanContext::destroySwapchain() {
  for (auto v : swapViews)
    vkDestroyImageView(device, v, nullptr);
  swapViews.clear();
  vkb::destroy_swapchain(vkbSwap);
  swapchain = VK_NULL_HANDLE;
}
