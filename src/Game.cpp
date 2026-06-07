#include "Game.hpp"
#include "Typs.hpp"
#include <algorithm>
#include <fmt/core.h>
#include <random>

///===============RNG===============
static std::mt19937 rng{static_cast<std::mt19937>(std::random_device{}())};

static int randInt(int lo, int hi) { // [lo, hi],
  return std::uniform_int_distribution<int>{lo, hi}(rng);
}

//==========Game ====================
Game::Game() { reset(); }

void Game::reset() {
  state_ = GameState{};

  int cx = GRID / 2, cy = GRID / 2;
  state_.snake.push_back({cx, cy});
  state_.snake.push_back({cx - 1, cy});
  state_.snake.push_back({cx - 2, cy});

  state_.dir = Direction::Right;
  state_.nextDir = Direction::Right;
  state_.alive = true;
  state_.started = false;
  state_.score = 0;
  state_.moveSpeed = MOVE_BASE;

  placeFood();
  fmt::print("[Game] reset - WASD/arrows to move, R to restart\n");
}

void Game::update(float dt) {
  if (!state_.alive)
    return;

  // Fire countdown
  if (state_.fireTime > 0.0F)
    state_.fireTime = std::max(0.0F, state_.fireTime - dt);

  if (!state_.started)
    return;

  state_.moveTime += dt;
  if (state_.moveTime >= state_.moveSpeed) {
    state_.moveTime -= state_.moveSpeed;
    step();
  }
}

void Game::changeDirection(Direction d) {
  // Prevent 180-degree reversal
  auto &cur = state_.dir;
  if (d == Direction::Up && cur == Direction::Down)
    return;
  if (d == Direction::Down && cur == Direction::Up)
    return;
  if (d == Direction::Left && cur == Direction::Right)
    return;
  if (d == Direction::Right && cur == Direction::Left)
    return;

  state_.nextDir = d;
  state_.started = true;
}

//---Step ---------------------------
void Game::step() {
  state_.dir = state_.nextDir;

  glm::ivec2 head = state_.snake.front();
  switch (state_.dir) {
  case Direction::Up:
    head.y -= 1;
    break;
  case Direction::Down:
    head.y += 1;
    break;
  case Direction::Left:
    head.x -= 1;
    break;
  case Direction::Right:
    head.x += 1;
    break;
  }
  // 1. Push the new head onto the snake first so it is visual draw at the
  // border
  state_.snake.push_front(head);

  // 2. Now check if it went out of bounds
  //  Collision: wall
  if (outOfBounds(head)) {
    state_.alive = false;
    fmt::print("[Game] game over - wall - score: {}\n", state_.score);
    return;
  }

  // Collision: self (check before pushing new head)
  if (headHitsBody()) {
    state_.alive = false;
    fmt::print("[Game] game over - self - score: {}\n", state_.score);
    return;
  }

  // Eat food?
  if (head == state_.food) {
    state_.score++;
    state_.lastFood = state_.food;
    state_.fireTime = FIRE_DURATION;
    // speed up slightly
    state_.moveSpeed = std::max(MOVE_MIN, MOVE_BASE - state_.score * 0.005F);
    placeFood();
    fmt::print("[Game] score: {} speed: {:.3f}s\n", state_.score,
               state_.moveSpeed);
    // Dont pop tail - snake grows
  } else {
    state_.snake.pop_back();
  }
}

//==============Helper ==========
void Game::placeFood() {
  // Collect empty cells
  std::vector<glm::ivec2> free;
  free.reserve(GRID * GRID);
  for (int y{}; y < GRID; ++y) {
    for (int x{}; x < GRID; ++x) {
      glm::ivec2 p{x, y};
      bool occupied{false};
      for (auto &s : state_.snake)
        if (s == p) {
          occupied = true;
          break;
        }
      if (!occupied)
        free.push_back(p);
    }
  }
  if (free.empty())
    return;
  state_.food = free[randInt(0, static_cast<int>(free.size()) - 1)];
}

bool Game::headHitsBody() const {
  auto &s = state_.snake;
  for (size_t i{1}; i < s.size(); ++i)
    if (s[0] == s[i])
      return true;
  return false;
}

bool Game::outOfBounds(glm::ivec2 p) const {
  return p.x < 0 || p.x >= GRID || p.y < 0 || p.y >= GRID;
}
