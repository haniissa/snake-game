#include <array>
#include <cstddef>
#include <cstdint>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fstream>
#include <glm/glm.hpp>
#include <ios>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

// Project headers AFTER
#include "Game.hpp"
#include "Render.hpp"
#include "Typs.hpp"
#include "VulkanContext.hpp"
#include "fmt/base.h"

// Margins: the game grid occupies 90% of the viewport on each axis
inline constexpr float GRID_MARGIN = 0.90F; // fraction of NDC half-range
// NDC runs [-1, 1] usable range = 2 * GRID_MARGIN = 1.00 units

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult _r = (x);                                                         \
    if (_r != VK_SUCCESS)                                                      \
      throw std::runtime_error(                                                \
          fmt::format("VKErr {} {}: {}", (int)_r, __FILE__, __LINE__));        \
  } while (0)

//---Lifecycle-------------------------------
void Renderer::init(VulkanContext &ctx) {
  ctx_ = &ctx;

  // Push-constant range covers the whole PushConstants struct
  VkPushConstantRange pcRange{};
  pcRange.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pcRange.size = sizeof(PushConstants);

  VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  li.pushConstantRangeCount = 1;
  li.pPushConstantRanges = &pcRange;
  VK_CHECK(vkCreatePipelineLayout(ctx.device, &li, nullptr, &layout_));

  alphaPipeline_ = createPipeline(false);
  additivePipeline_ = createPipeline(true);

  fmt::print("[Renderer] pipelines created\n");
}

void Renderer::cleanup() {
  if (!ctx_)
    return;
  vkDeviceWaitIdle(ctx_->device);
  vkDestroyPipeline(ctx_->device, alphaPipeline_, nullptr);
  vkDestroyPipeline(ctx_->device, additivePipeline_, nullptr);
  vkDestroyPipelineLayout(ctx_->device, layout_, nullptr);
}

//---- Frame ----------------------------------------
bool Renderer::beginFrame(float totalTime) {
  time_ = totalTime;
  auto &f = ctx_->frame();

  // Wait for the previous use of this frame's resources
  VK_CHECK(vkWaitForFences(ctx_->device, 1, &f.fence, VK_TRUE, UINT64_MAX));

  VkResult res =
      vkAcquireNextImageKHR(ctx_->device, ctx_->swapchain, UINT64_MAX,
                            f.imgReady, VK_NULL_HANDLE, &imgIndex_);

  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    ctx_->recreateSwapchain();
    alphaPipeline_ = createPipeline(false); // recreate pipelines too
    additivePipeline_ = createPipeline(true);
    return false;
  }
  if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
    throw std::runtime_error("vkAcquireNextImageKHR failed");

  VK_CHECK(vkResetFences(ctx_->device, 1, &f.fence));

  cmd_ = f.cmd;
  VK_CHECK(vkResetCommandBuffer(cmd_, 0));

  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VK_CHECK(vkBeginCommandBuffer(cmd_, &bi));

  // Transition swapchain image: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
  ctx_->transitionImage(cmd_, ctx_->swapImages[imgIndex_],
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // Begin dynamic rendering
  VkClearValue clear{};
  clear.color = {Colors::Background.r, Colors::Background.g,
                 Colors::Background.b, Colors::Background.a};

  VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  att.imageView = ctx_->swapViews[imgIndex_];
  att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  att.clearValue = clear;

  VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
  ri.renderArea = {{0, 0}, ctx_->swapExtent};
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &att;
  vkCmdBeginRendering(cmd_, &ri);

  // Set dynamic viewport + scissor
  VkViewport vp{
      0,    0,   (float)ctx_->swapExtent.width, (float)ctx_->swapExtent.height,
      0.0f, 1.0f};

  VkRect2D sc{{0, 0}, ctx_->swapExtent};
  vkCmdSetViewport(cmd_, 0, 1, &vp);
  vkCmdSetScissor(cmd_, 0, 1, &sc);

  inFrame = true;
  return true;
}
void Renderer::endFrame() {
  vkCmdEndRendering(cmd_);

  // Transition: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
  ctx_->transitionImage(cmd_, ctx_->swapImages[imgIndex_],
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  VK_CHECK(vkEndCommandBuffer(cmd_));

  auto &f = ctx_->frame();

  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.waitSemaphoreCount = 1;
  si.pWaitSemaphores = &f.imgReady;
  si.pWaitDstStageMask = &waitStage;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &f.cmd;
  si.signalSemaphoreCount = 1;
  si.pSignalSemaphores = &f.rendered;
  VK_CHECK(vkQueueSubmit(ctx_->graphicsQueue, 1, &si, f.fence));

  VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  pi.waitSemaphoreCount = 1;
  pi.pWaitSemaphores = &f.rendered;
  pi.swapchainCount = 1;
  pi.pSwapchains = &ctx_->swapchain;
  pi.pImageIndices = &imgIndex_;

  VkResult res = vkQueuePresentKHR(ctx_->presentQueue, &pi);
  if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
    ctx_->recreateSwapchain();

  ctx_->frameIndex = (ctx_->frameIndex + 1) % MAX_FRAMES;
  inFrame = false;
}

//------High-level game draw ---------------------------
void Renderer::drawBackground() {
  // Light grid lines so the playing field is visible
  bindAlpha();
  glm::vec2 sz = cellSizeNDC();
  for (int y = 0; y <= GRID; ++y) {
    for (int x{}; x <= GRID; ++x) {
      PushConstants pc{};
      pc.pos = cellNDC(x, y);
      pc.size = {sz.x * 0.04f, sz.y};
      pc.color = Colors::GridLine;
      pc.time = time_;
      pc.mode = (int)DrawMode::Solid;
      pushAndDraw(pc);
    }
  }
}

void Renderer::drawGame(const GameState &gs) {
  glm::vec2 sz = cellSizeNDC();
  float pad = 0.07F; // fraction of cell to inset each segment

  //----------1. Shadows ---------------------------
  bindAlpha();
  for (auto &seg : gs.snake) {
    PushConstants pc{};
    // Shadow: slightly offset down-right, a bit larger
    pc.pos = cellNDC(seg.x, seg.y) + sz * glm::vec2(0.18F, 0.22F);
    pc.size = sz * (1.0F + pad);
    pc.color = {0, 0, 0, 0.5F};
    pc.time = time_;
    pc.mode = (int)DrawMode::Shadow;
    pushAndDraw(pc);
  }
  // Shadow under food
  {
    PushConstants pc{};
    pc.pos = cellNDC(gs.food.x, gs.food.y) + sz * glm::vec2(0.20F, 0.25F);
    pc.size = sz * 1.1F;
    pc.color = {0, 0, 0, 0.4F};
    pc.time = time_;
    pc.mode = (int)DrawMode::Shadow;
    pushAndDraw(pc);
  }
  //---2. Food circle-------------------------------
  {
    PushConstants pc{};
    pc.pos = cellNDC(gs.food.x, gs.food.y) + sz * pad;
    pc.size = sz * (1.0F - pad * 2.0F);
    pc.color = Colors::Food;
    pc.time = time_;
    pc.mode = (int)DrawMode::Circle;
    pushAndDraw(pc);
  }

  //--- 3. Snake body (tail -> head so is drawn on top) ------
  int n = static_cast<int>(gs.snake.size());
  for (int i = n - 1; i >= 0; --i) {
    auto &seg = gs.snake[i];
    float t = (float)i / (float)(n - 1 + 1); // 0 = head, 1 = tail
    glm::vec4 col = mix(Colors::SnakeHead, Colors::SnakeTail, t);

    // Shrink segments slightly toward tail for a tapered look
    float scale = glm::mix(0.92F, 0.72F, t);

    PushConstants pc{};
    float halfDiff = (1.0F - scale) * 0.5f;
    pc.pos = cellNDC(seg.x, seg.y) + sz * halfDiff;
    pc.size = sz * scale;
    pc.color = col;
    pc.time = time_;
    pc.mode = (int)DrawMode::Solid;
    if (!gs.alive)
      pc.color = Colors::GameOver; // flash red on death
    pushAndDraw(pc);
  }

  //---- 4. Fire (additiver, drawn after solid gemoetry so it glows on top)
  if (gs.showFire()) {
    bindAdditive();
    float fi = gs.fireIntensity();

    // Expand fire quad over time as it dissipates
    float expand = 1.0F + (1.0F - fi) * 0.8F;
    glm::vec2 center =
        cellNDC(gs.lastFood.x, gs.lastFood.y) + sz * 0.5F; // center fo cell

    // Serval layered fire quads at slightly different sizes/offsets
    for (float layer : {1.0f, 1.4F, 1.9F}) {
      float lScale = expand * layer;
      glm::vec2 lSz = sz * lScale;

      PushConstants pc{};
      pc.pos = center - lSz * 0.5F;
      pc.size = lSz;
      pc.color = {1, 0.5F, 0, fi / layer};
      pc.time = time_;
      pc.mode = (int)DrawMode::Fire;
    }
    bindAlpha(); // restore for next call
  }
}

//----Internal helpers---------------------------
void Renderer::pushAndDraw(const PushConstants &pc) {
  vkCmdPushConstants(cmd_, layout_,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(PushConstants), &pc);
  vkCmdDraw(cmd_, 4, 1, 0, 0); // 4 vertices -> TRIANGLE_STRIP quad
}

void Renderer::bindAlpha() {
  vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, alphaPipeline_);
}

void Renderer::bindAdditive() {
  vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, additivePipeline_);
}

glm::vec2 Renderer::cellSizeNDC() const {
  // // 1. Calculate the aspect ratio of the current window/viewport
  // float aspect = (float)ctx_->swapExtent.width /
  // (float)ctx_->swapExtent.height;
  float sz = (2.0F * GRID_MARGIN) / (float)GRID;

  // // 2. adjust either x o y depending on whther the window is wider or taller

  // if (aspect > 1.0F) {
  //   // window is wider than it is tall (e.g. 800*450) -> scale x down
  //   return {sz / aspect, sz};
  // } else {
  //   return {sz, sz * aspect};
  // }
  return {sz, sz};
}

glm::vec2 Renderer::cellNDC(int x, int y) const {
  // float aspect = (float)ctx_->swapExtent.width /
  // (float)ctx_->swapExtent.height; float sz = (2.0F * GRID_MARGIN) /
  // (float)GRID; if (aspect > 1.0F) {
  //   // Center the grid horizontally and scale x positions
  //   float marginX = GRID_MARGIN / aspect;
  //   float offX = -marginX + x * (sz / aspect);
  //   float offY = -GRID_MARGIN + y * sz;
  //   return {offX, offY};
  // } else {
  //   // center the grid vertically and scale Y positions
  //   float marginY = GRID_MARGIN * aspect;
  //   float offX = -GRID_MARGIN + x * sz;
  //   float offY = -marginY + y * (sz * aspect);
  //   return {offX, offY};
  // }
  float sz = (2.0f * GRID_MARGIN) / (float)GRID;
  float offX = -GRID_MARGIN + x * sz;
  float offY = -GRID_MARGIN + y * sz;
  return {offX, offY};
}

//-----Pipline factory---------------------------------
VkShaderModule Renderer::loadSPV(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    throw std::runtime_error("Cannot open shader: " + path);
  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<char> buf(size);
  file.read(buf.data(), (std::streamsize)size);

  VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  ci.codeSize = size;
  ci.pCode = reinterpret_cast<const uint32_t *>(buf.data());
  VkShaderModule mod;
  VK_CHECK(vkCreateShaderModule(ctx_->device, &ci, nullptr, &mod));
  return mod;
}

VkPipeline Renderer::createPipeline(bool additive) {
  auto vert = loadSPV("shaders/quad.vert.spv");
  auto frag = loadSPV("shaders/quad.frag.spv");

  std::array<VkPipelineShaderStageCreateInfo, 2> stagesP{};
  stagesP[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stagesP[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stagesP[0].module = vert;
  stagesP[0].pName = "main";
  stagesP[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stagesP[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stagesP[1].module = frag;
  stagesP[1].pName = "main";

  // No vertex buffers - vertices are generated in the shader
  VkPipelineVertexInputStateCreateInfo vi{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

  VkPipelineInputAssemblyStateCreateInfo ia{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

  VkPipelineViewportStateCreateInfo vs{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vs.viewportCount = 1;
  vs.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rs{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rs.lineWidth = 1.0F;

  VkPipelineMultisampleStateCreateInfo ms{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // Blend state
  VkPipelineColorBlendAttachmentState blend{};
  blend.blendEnable = VK_TRUE;
  blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  if (additive) {
    // Fire: src*srcAlpha + dst*1 -> HDR glow
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
  } else {
    // Standard over-compositing
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
  }

  VkPipelineColorBlendStateCreateInfo cbs{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cbs.attachmentCount = 1;
  cbs.pAttachments = &blend;

  std::array<VkDynamicState, 2> dyn{VK_DYNAMIC_STATE_VIEWPORT,
                                    VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo ds{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  ds.dynamicStateCount = (uint32_t)dyn.size();
  ds.pDynamicStates = dyn.data();

  // Dynamic rendering (no render pass object needed)
  VkPipelineRenderingCreateInfo dri{
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  dri.colorAttachmentCount = 1;
  dri.pColorAttachmentFormats = &ctx_->swapFormat;

  VkGraphicsPipelineCreateInfo pi{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pi.pNext = &dri;
  pi.stageCount = (uint32_t)stagesP.size();
  pi.pStages = stagesP.data();
  pi.pVertexInputState = &vi;
  pi.pInputAssemblyState = &ia;
  pi.pViewportState = &vs;
  pi.pRasterizationState = &rs;
  pi.pMultisampleState = &ms;
  pi.pColorBlendState = &cbs;
  pi.pDynamicState = &ds;
  pi.layout = layout_;

  VkPipeline pipeline;
  VK_CHECK(vkCreateGraphicsPipelines(ctx_->device, VK_NULL_HANDLE, 1, &pi,
                                     nullptr, &pipeline));

  vkDestroyShaderModule(ctx_->device, vert, nullptr);
  vkDestroyShaderModule(ctx_->device, frag, nullptr);

  return pipeline;
}
