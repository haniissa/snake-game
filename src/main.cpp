#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <chrono>
#include <fmt/core.h>
#include <stdexcept>

#include "Game.hpp"
#include "Render.hpp"
#include "VulkanContext.hpp"

int main(int, char **) {
  //----SDL init------------
  if (!SDL_Init(SDL_INIT_VIDEO))
    throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());

  constexpr int W{660}, H{500};
  SDL_Window *win = SDL_CreateWindow("Snake - Vulkan", W, H,
                                     SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

  if (!win)
    throw std::runtime_error(std::string("SDL_CreateWindow: ") +
                             SDL_GetError());

  //----Vulkan + Renderer---------
  VulkanContext ctx;
  ctx.init(win, W, H);

  Renderer renderer;
  renderer.init(ctx);

  //----Game-------------------------
  Game game;

  fmt::print("\n"
             "  Controls:\n"
             "  WASD/ Arrow keys - steer\n"
             "  R                - restar\n"
             "  Escape           - quit\n\n");

  //--- Main loop-------------------------
  using Clock = std::chrono::high_resolution_clock;
  auto lastTime = Clock::now();
  float totalTime{0.0F};
  bool running{true};

  while (running) {
    auto now = Clock::now();
    float dt = std::chrono::duration<float>(now - lastTime).count();
    lastTime = now;
    // Clamp dt so a minimzed window doesn't spike
    dt = std::min(dt, 0.1F);
    totalTime += dt;

    //---Event ---------------------------------
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_EVENT_QUIT)
        running = false;

      if (ev.type == SDL_EVENT_WINDOW_RESIZED) {
        int nw, nh;
        SDL_GetWindowSize(win, &nw, &nh);
        ctx.winWidth = nw;
        ctx.winHeight = nh;
        ctx.recreateSwapchain();
      }
      if (ev.type == SDL_EVENT_KEY_DOWN) {
        auto sc = ev.key.scancode;
        switch (sc) {
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_UP:
          game.changeDirection(Direction::Up);
          break;
        case SDL_SCANCODE_S:
        case SDL_SCANCODE_DOWN:
          game.changeDirection(Direction::Down);
          break;
        case SDL_SCANCODE_A:
        case SDL_SCANCODE_LEFT:
          game.changeDirection(Direction::Left);
          break;
        case SDL_SCANCODE_D:
        case SDL_SCANCODE_RIGHT:
          game.changeDirection(Direction::Right);
          break;
        case SDL_SCANCODE_R:
          game.reset();
          break;
        case SDL_SCANCODE_ESCAPE:
          running = false;
          break;
        default:
          break;
        }
      }
    }
    //---Update ------------------
    game.update(dt);

    //-----Render------------
    if (renderer.beginFrame(totalTime)) {
      renderer.drawBackground();
      renderer.drawGame(game.state());
      renderer.endFrame();
    }
  }

  //----Cleanup----------------------------
  renderer.cleanup();
  ctx.cleanup();
  SDL_DestroyWindow(win);
  SDL_Quit();

  return 0;
}
