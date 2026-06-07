#pragma once
#include <vulkan/vulkan.h>


 //Vulkan Memeory Allocator
 #include<vk_mem_alloc.h>
 //
//vk-bootstrap wraps instance/device/swapchain boilerplate
#include <VkBootstrap.h>
 //
 #include <SDL3/SDL.h>
 #include <SDL3/SDL_vulkan.h>
 #include <vector>
 #include <string>
 #include <stdexcept>
#include <vulkan/vulkan_core.h>

 inline constexpr int MAX_FRAMES{2};

 //--Per-frame sync objects--------------
 struct FrameData {
     VkCommandPool cmdPool = VK_NULL_HANDLE;
     VkCommandBuffer cmd = VK_NULL_HANDLE;
     VkSemaphore   imgReady = VK_NULL_HANDLE; // IMAGE ACQUIRED
     VkSemaphore  rendered = VK_NULL_HANDLE; // RENDERING FINISHED
     VkFence     fence  = VK_NULL_HANDLE;
 };

 //---Core Vulkan objects
 struct VulkanContext {
     //Raw Vulkan handles
     VkInstance                instance = VK_NULL_HANDLE;
     VkDebugUtilsMessengerEXT  debugMessenger = VK_NULL_HANDLE;
     VkPhysicalDevice          physDevice = VK_NULL_HANDLE;
     VkDevice                  device = VK_NULL_HANDLE;
     VkSurfaceKHR              surface = VK_NULL_HANDLE;

     VkQueue                   graphicsQueue = VK_NULL_HANDLE;
     uint32_t                  graphicsFamily = 0;
     VkQueue                   presentQueue   = VK_NULL_HANDLE;
     uint32_t                  presentFamily  = 0;

     //Swapchain
     VkSwapchainKHR            swapchain = VK_NULL_HANDLE;
     VkFormat                  swapFormat{};
     VkExtent2D                swapExtent{};
     std::vector<VkImage>      swapImages;
     std::vector<VkImageView>  swapViews;

     //VMA allocator
     VmaAllocator allocator = VK_NULL_HANDLE;

     //Frame data (double-buffered)
     FrameData frames[MAX_FRAMES];
     uint32_t  frameIndex = 0;

     //SDL window reference
     SDL_Window* window{nullptr};
     int winWidth = 0;
     int winHeight = 0;

     //-----Lifescyle--------------------
     void init(SDL_Window* win, int w, int h);
     void cleanup();
     void recreateSwapchain();

     FrameData& frame(){return frames[frameIndex];}

     //Image layout transition using synchronization2
     void transitionImage(VkCommandBuffer cmd, VkImage image,
         VkImageLayout from, VkImageLayout to) const;

     //Immediate submit helper (useful for one-off uploads)
     void immediateSubmit(auto&& fn){
         VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr};

         ai.commandPool  = frames[0].cmdPool;
         ai.level        = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
         ai.commandBufferCount = 1;
         VkCommandBuffer cmd;
         vkAllocateCommandBuffers(device, &ai, &cmd);

         VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
         bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
         vkBeginCommandBuffer(cmd, &bi);
         fn(cmd);
         vkEndCommandBuffer(cmd);

         VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
         si.commandBufferCount = 1;
         si.pCommandBuffers = &cmd;
         vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
         vkQueueWaitIdle(graphicsQueue);
         vkFreeCommandBuffers(device, frames[0].cmdPool, 1, &cmd);
     }
     private:
        vkb::Instance   vkbInst;
        vkb::Device     vkDev;
        vkb::Swapchain  vkbSwap;

        void createInstance();
        void createSurface();
        void pickDeviceAndCreate();
        void createAllocator();
        void buildSwapchain();
        void createSwapViews();
        void createFrameData();
        void destroySwapchain();

 };
