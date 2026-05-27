#pragma once
#include <RLGymCPP/Rewards/Reward.h>
#include <cmath>
#include <unordered_map>

using namespace RLGC;

class WhiffPenalty : public Reward {
public:
    static constexpr float WHIFF_DIST = 250.0f;
    static constexpr int WHIFF_FRAMES = 10;

    struct PlayerWhiffState {
        bool jumpedOrFlipped = false;
        int framesSinceJump = 0;
        float ballDistAtJump = 0.0f;
        bool whiffed = false;
    };

    std::unordered_map<uint32_t, PlayerWhiffState> playerStates;

    void Reset(const GameState& initialState) override {
        playerStates.clear();
    }

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        auto& ws = playerStates[player.carId];

        bool justJumped = (player.isJumping || player.isFlipping) &&
                          (!player.prev || (!player.prev->isJumping && !player.prev->isFlipping));
        if (justJumped) {
            ws.jumpedOrFlipped = true;
            ws.framesSinceJump = 0;
            ws.ballDistAtJump = (player.pos - state.ball.pos).Length();
            ws.whiffed = true;
        }

        if (ws.jumpedOrFlipped) {
            ws.framesSinceJump++;

            if (player.ballTouchedStep) {
                ws.jumpedOrFlipped = false;
                ws.whiffed = false;
                return 0.0f;
            }

            if (ws.framesSinceJump > WHIFF_FRAMES && ws.ballDistAtJump < WHIFF_DIST && ws.whiffed) {
                ws.jumpedOrFlipped = false;
                ws.whiffed = false;
                return -1.0f;
            }

            if (ws.framesSinceJump > WHIFF_FRAMES * 3) {
                ws.jumpedOrFlipped = false;
            }
        }

        return 0.0f;
    }
};

class DriveByWhiffPenalty : public Reward {
public:
    static constexpr float MIN_SPEED = 80.0f;
    static constexpr float BALL_DIST = 150.0f;

    struct PlayerDBState {
        bool wasClose = false;
        int closeFrames = 0;
        static constexpr int REQUIRED_FRAMES = 8;
    };

    std::unordered_map<uint32_t, PlayerDBState> playerStates;

    void Reset(const GameState& initialState) override {
        playerStates.clear();
    }

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        auto& ds = playerStates[player.carId];

        float speed = player.vel.Length();
        float distToBall = (player.pos - state.ball.pos).Length();

        if (speed > MIN_SPEED && distToBall < BALL_DIST && !player.ballTouchedStep) {
            ds.closeFrames++;
            ds.wasClose = true;
        } else {
            if (ds.wasClose && ds.closeFrames >= PlayerDBState::REQUIRED_FRAMES && !player.ballTouchedStep) {
                ds.wasClose = false;
                ds.closeFrames = 0;
                return -1.0f;
            }
            ds.closeFrames = 0;
            ds.wasClose = false;
        }

        return 0.0f;
    }
};

class BallChasingPenalty : public Reward {
public:
    static constexpr float CHASE_FRAMES = 180.0f;
    static constexpr float MIN_SPEED = 200.0f;

    struct ChaseState {
        int consecutiveTowardBall = 0;
    };

    std::unordered_map<uint32_t, ChaseState> playerStates;

    void Reset(const GameState& initialState) override {
        playerStates.clear();
    }

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        auto& cs = playerStates[player.carId];

        float speed = player.vel.Length();
        if (speed < MIN_SPEED) {
            cs.consecutiveTowardBall = 0;
            return 0.0f;
        }

        Vec toBall = (state.ball.pos - player.pos).Normalized();
        float velTowardBall = toBall.Dot(player.vel);

        if (velTowardBall > speed * 0.5f) {
            cs.consecutiveTowardBall++;
        } else {
            cs.consecutiveTowardBall = 0;
            return 0.0f;
        }

        float ballDistToOwnGoal = (player.team == Team::BLUE) ?
            std::abs(state.ball.pos.y - (-CommonValues::BACK_WALL_Y)) :
            std::abs(state.ball.pos.y - CommonValues::BACK_WALL_Y);

        if (ballDistToOwnGoal < 1500.0f) {
            cs.consecutiveTowardBall = 0;
            return 0.0f;
        }

        if (cs.consecutiveTowardBall >= CHASE_FRAMES) {
            float severity = RS_MIN(1.0f, (cs.consecutiveTowardBall - CHASE_FRAMES) / 120.0f);
            return -severity;
        }

        return 0.0f;
    }
};

class BoostStarvationPenalty : public Reward {
public:
    static constexpr float STARVATION_RADIUS = 800.0f;

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (player.boost > 5.0f)
            return 0.0f;

        for (int i = 0; i < CommonValues::BOOST_LOCATIONS_AMOUNT; i++) {
            if (!state.boostPads[i]) continue;

            Vec padPos = CommonValues::BOOST_LOCATIONS[i];
            float dist = (player.pos - padPos).Length();
            if (dist < STARVATION_RADIUS) {
                float severity = 1.0f - (dist / STARVATION_RADIUS);
                return -severity;
            }
        }

        return 0.0f;
    }
};
