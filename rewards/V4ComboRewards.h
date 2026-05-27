#pragma once
#include <RLGymCPP/Rewards/Reward.h>
#include <cmath>
#include <unordered_map>

using namespace RLGC;

class AerialGoalMultiplierReward : public Reward {
public:
    struct AerialState {
        bool wasAirborneBeforeGoal = false;
    };

    std::unordered_map<uint32_t, AerialState> playerStates;

    void Reset(const GameState& initialState) override {
        playerStates.clear();
    }

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        auto& as = playerStates[player.carId];

        if (!player.isOnGround) {
            as.wasAirborneBeforeGoal = true;
        }

        if (state.goalScored) {
            bool scored = (player.team != RS_TEAM_FROM_Y(state.ball.pos.y));
            if (scored && as.wasAirborneBeforeGoal) {
                as.wasAirborneBeforeGoal = false;
                return 0.5f;
            }
        }

        if (player.isOnGround && state.goalScored) {
            as.wasAirborneBeforeGoal = false;
        }

        if (player.isOnGround && !state.goalScored) {
            as.wasAirborneBeforeGoal = false;
        }

        return 0.0f;
    }
};

class AirDribbleFlickGoalReward : public Reward {
public:
    struct DribbleChainState {
        int aerialTouchCount = 0;
        bool endedWithFlick = false;
        bool goalAfterDribble = false;
        int framesSinceTouch = 0;
    };

    std::unordered_map<uint32_t, DribbleChainState> playerStates;

    static constexpr int GOAL_WINDOW = 120;

    void resetState(DribbleChainState& ds) {
        ds.aerialTouchCount = 0;
        ds.endedWithFlick = false;
        ds.goalAfterDribble = false;
        ds.framesSinceTouch = 0;
    }

    void Reset(const GameState& initialState) override {
        playerStates.clear();
    }

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        auto& ds = playerStates[player.carId];

        if (player.isOnGround) {
            if (ds.aerialTouchCount > 0 && ds.goalAfterDribble) {
                float reward = (ds.aerialTouchCount >= 2) ? 1.0f : 0.0f;
                resetState(ds);
                return reward;
            }
            resetState(ds);
            return 0.0f;
        }

        if (player.ballTouchedStep) {
            ds.aerialTouchCount++;
            ds.framesSinceTouch = 0;
            if (player.hasFlipped && ds.aerialTouchCount >= 2) {
                ds.endedWithFlick = true;
            }
        } else {
            ds.framesSinceTouch++;
        }

        if (ds.aerialTouchCount > 0 && ds.framesSinceTouch > GOAL_WINDOW) {
            resetState(ds);
        }

        if (state.goalScored) {
            bool scored = (player.team != RS_TEAM_FROM_Y(state.ball.pos.y));
            if (scored && ds.aerialTouchCount >= 2) {
                if (ds.framesSinceTouch < GOAL_WINDOW) {
                    ds.goalAfterDribble = true;
                }
            }
        }

        return 0.0f;
    }
};

class ReadChainReward : public Reward {
public:
    enum ChainEvent {
        NONE,
        BACKBOARD_HIT,
        REDIRECT,
        SHOT
    };

    struct ChainState {
        ChainEvent lastEvent = NONE;
        int framesSinceEvent = 0;
        bool chainComplete = false;
        int lastBackboardFrame = 0;
    };

    std::unordered_map<uint32_t, ChainState> playerStates;

    static constexpr int CHAIN_WINDOW = 360;
    static constexpr float BACKBOARD_DIST = 200.0f;
    static constexpr float BACKBOARD_Z_MAX = 600.0f;

    void Reset(const GameState& initialState) override {
        playerStates.clear();
    }

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        auto& cs = playerStates[player.carId];
        cs.framesSinceEvent++;

        bool backboardHit = false;
        if (state.prev) {
            float ballX = std::abs(state.ball.pos.x);
            float wallX = CommonValues::SIDE_WALL_X;

            if (ballX > wallX - BACKBOARD_DIST && state.ball.pos.z < BACKBOARD_Z_MAX &&
                std::abs(state.ball.vel.x) > 100.0f) {
                backboardHit = true;
            }
        }

        if (backboardHit && player.ballTouchedStep) {
            cs.lastEvent = BACKBOARD_HIT;
            cs.framesSinceEvent = 0;
            cs.lastBackboardFrame = cs.framesSinceEvent;
            return 0.0f;
        }

        if (cs.lastEvent == BACKBOARD_HIT && !player.isOnGround && player.ballTouchedStep &&
            cs.framesSinceEvent < CHAIN_WINDOW) {
            cs.lastEvent = REDIRECT;
            return 0.0f;
        }

        if (cs.lastEvent == REDIRECT && state.goalScored &&
            cs.framesSinceEvent < CHAIN_WINDOW &&
            player.ballTouchedStep) {
            bool scored = (player.team != RS_TEAM_FROM_Y(state.ball.pos.y));
            if (scored) {
                cs.lastEvent = SHOT;
                cs.chainComplete = true;
                return 1.0f;
            }
        }

        if (cs.framesSinceEvent > CHAIN_WINDOW) {
            cs.lastEvent = NONE;
            cs.chainComplete = false;
        }

        return 0.0f;
    }
};
