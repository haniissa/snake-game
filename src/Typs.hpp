#pragma once
#include <glm/glm.hpp>
#include <cstdint>

//---------Draw mode (matches shader) ================
enum class DrawMode : int {
    Solid = 0,
    Fire = 1,
    Shadow = 2,
    Circle = 3,
};

//----Vulkan push-constant block (<128 bytes guaranteed)-----
// Layout must exactly mtch the GLSL `PC` block (std430 scalars).
struct PushConstants {
    glm::vec2 pos;      // NDC top-left corner
    glm::vec2 size;     // NDC width * height
    glm::vec4 color;    // RGBA
    float time;         // seconds
    int mode;           // DrawMode cast to int
    float _pad[2];      // keep vec4 alignment tidy
};

static_assert(sizeof(PushConstants) == 48, "PushConstants size mismatch");
static_assert(sizeof(PushConstants) <= 128, "Exceeds guarnateed push-constant budget");

//---- Game direction---------
enum class Direction {Up, Down, Left, Right};

//--- Palette (linear sRGB)---------------
namespace Colors {
    inline constexpr glm::vec4 Background{0.05F, 0.06F, 0.10F, 1.0F};
    inline constexpr glm::vec4 GridLine  {0.10F, 0.11F, 0.17F, 1.0F};
    inline constexpr glm::vec4 SnakeHead {0.45F, 1.00F, 0.45F, 1.0F};
    inline constexpr glm::vec4 SnakeBody {0.20F, 0.75F, 0.25F, 1.0F};
    inline constexpr glm::vec4 SnakeTail {0.10F, 0.45F, 0.12F, 1.0F};
    inline constexpr glm::vec4 Food      {0.95F, 0.20F, 0.10F, 1.0F};
    inline constexpr glm::vec4 GameOver  {0.90F, 0.15F, 0.15F, 1.0F};
}
