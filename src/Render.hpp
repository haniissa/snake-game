#pragma once
#include "Game.hpp"
#include "Typs.hpp"
#include "VulkanContext.hpp"
#include <cstdint>
#include <string>


//------Renderer-------------------------------
// Manage two pipelines and exposes high-level draw calls.
// Alpha-blend pipeline:  solid quads + shadows + food circle
// Additive pipline    : fire glow (HDR-style, no darkening)
class Renderer {
public:
  void init(VulkanContext &ctx);
  void cleanup();

  // Return false when the frame must be skipped (swapchain out of date).
  bool beginFrame(float totalTime);
  void endFrame();

  //----Draw calls (only valid between beginFrame/endFrame)
  void drawBackground();
  void drawGame(const GameState &gs);

private:
  VulkanContext *ctx_{nullptr};

  VkPipelineLayout layout_ = VK_NULL_HANDLE;

  VkPipeline alphaPipeline_ = VK_NULL_HANDLE;    // mode 0,2, 3
  VkPipeline additivePipeline_ = VK_NULL_HANDLE; // mode 1 (fire)

  uint32_t imgIndex_ = 0;
  VkCommandBuffer cmd_ = VK_NULL_HANDLE;
  float time_ = 0.0F;
  bool inFrame = false;

  //----Internal draw helpers--------------------
  void pushAndDraw(const PushConstants &pc);
  void bindAlpha();
  void bindAdditive();

  //-----NDC coordinate helpers-----------------
  // Conversts a grid cell to NDC top-left corner and size
  glm::vec2 cellNDC(int x, int y) const;
  glm::vec2 cellSizeNDC() const;

  //-----Pipeline factory----------------------
  VkPipeline createPipeline(bool additive);
  VkShaderModule loadSPV(const std::string &path);
};
