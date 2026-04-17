# Snake — Vulkan

A Snake game using **SDL3 + Vulkan 1.3 + CMake**, featuring:

- **Fire shader** (FBM noise, additive blending) bursts at the food position when the snake eats
- **Soft drop shadows** under every snake segment and the food
- **Smooth snake taper** — head is bright lime, tail fades to dark green
- **Pulsing food circle** with specular highlight and glow ring
- Zero vertex buffers — geometry is generated directly in the vertex shader

---

## Requirements

| Tool | Min version |
|------|------------|
| CMake | 3.20 |
| C++ compiler | C++17 (GCC 11, Clang 14, MSVC 2022) |
| Vulkan SDK | 1.3.x — includes `glslc` |
| GPU driver | Vulkan 1.3 with `dynamicRendering` + `synchronization2` |

Install the Vulkan SDK from <https://vulkan.lunarg.com/> and make sure
`glslc` is on your `PATH` (or set `VULKAN_SDK` env variable).

All C++ dependencies (**SDL3, fmt, vk-bootstrap, VulkanMemoryAllocator, glm**)
are fetched automatically by CMake via `FetchContent` — no manual installs.

---

## Build

```bash
git clone <this-repo>
cd snake-vulkan

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

./build/SnakeVulkan          # Linux / macOS
build\Release\SnakeVulkan.exe  # Windows
```

### Debug build (validation layers enabled)
```bash
cmake -B build-dbg -DCMAKE_BUILD_TYPE=Debug
cmake --build build-dbg
./build-dbg/SnakeVulkan
```

---

## Controls

| Key | Action |
|-----|--------|
| `W` / `↑` | Up |
| `S` / `↓` | Down |
| `A` / `←` | Left |
| `D` / `→` | Right |
| `R` | Restart |
| `Esc` | Quit |

---

## Project layout

```
snake-vulkan/
├── CMakeLists.txt
├── shaders/
│   ├── quad.vert       GLSL vertex  — generates a quad from vertex index
│   └── quad.frag       GLSL fragment — 4 modes: solid / fire / shadow / circle
└── src/
    ├── Types.hpp       PushConstants, DrawMode, palette constants
    ├── Game.hpp / .cpp Pure game logic (no Vulkan)
    ├── VulkanContext.hpp / .cpp
    │                   Instance · physical device · logical device · swapchain
    │                   (vk-bootstrap) · VMA allocator · frame sync objects
    ├── Renderer.hpp / .cpp
    │                   Two pipelines (alpha-blend / additive) · draw calls ·
    │                   NDC coordinate helpers · dynamic rendering (VK 1.3)
    └── main.cpp        SDL3 window · event loop · game + renderer glue
```

---

## Shader modes (push constants)

The single `quad.frag` shader handles all draw modes via the `mode` field
in the push-constant block:

| `mode` | Effect | Pipeline |
|--------|--------|----------|
| `0` Solid | Rounded rectangle with SDF rounding | Alpha-blend |
| `1` Fire  | FBM noise, height mask, animated upward | Additive |
| `2` Shadow | Soft elliptical gradient | Alpha-blend |
| `3` Circle | SDF circle with pulsing glow + specular | Alpha-blend |

### Fire technical notes
The fire uses 5-octave fractional Brownian motion (FBM) animated by
`time` in the push constants. A height mask (`1 - uv.y`) makes the base
hot and the top transparent, and a sine-based horizontal sway makes
each flame unique. The additive pipeline means fire colour *adds* to
whatever is behind it — darker backgrounds produce more vivid glow.

---

## Extending

- **Score HUD** — render a bitmap-font atlas into a texture and sample it in a new shader mode
- **Particles** — emit small additive quads from the snake head each step
- **Compute shader** — move fire FBM to a compute pass and sample a texture in the fragment shader (better for high frame-rates)
- **Sound** — SDL_mixer 3 can be added alongside SDL3
