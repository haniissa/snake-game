#pragma once
#include <glm/glm.hpp>
#include <deque>
#include <vector>
#include "Typs.hpp"


//----- Constants ---------------
inline constexpr int GRID  = 20;           // cells per axis
inline constexpr float MOVE_BASE = 0.18F;  //seconds / step at start
inline constexpr float MOVE_MIN  = 0.06F;  // fastest possible
inline constexpr float FIRE_DURATION = 1.20F; // fire effect length (s)

//----Game state -------------
struct GameState {
    std::deque<glm::ivec2> snake;  // front = head
    glm::ivec2             food;
    glm::ivec2             lastFood; // where fire erupts

    Direction dir;
    Direction nextDir;

    bool alive = true;
    bool started = false;
    int score = 0;
    float moveTime = 0.0F;
    float moveSpeed = MOVE_BASE;
    float fireTime = 0.0F; // countdown after eating


    //Return true when fire effect should be drawn
    bool showFire() const {return fireTime > 0.0F;}
    //Inensity 0-1 for fire (fades out)
    float fireIntensity() const {return glm::clamp(fireTime / FIRE_DURATION, 0.0f, 1.0F);}
};

//===Controller ===============
class Game {
    public:
        Game();

        void reset();
        void update(float dt);
        void changeDirection(Direction d);

        const GameState& state() const {return state_;}
    private:
        GameState state_;

        void step();   // advance snake one cell
        void placeFood();
        bool headHitsBody() const;
        bool outOfBounds(glm::ivec2 pos) const;
};
