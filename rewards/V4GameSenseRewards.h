#pragma once
#include <RLGymCPP/Rewards/Reward.h>
#include <cmath>
#include <unordered_map>

using namespace RLGC;

class ShadowDefenseReward : public Reward {
public:
    static constexpr float MIN_VEL_TOWARD_BALL = 50.0f;
    static constexpr float POSSESSION_DIST = 300.0f;

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        Vec ownGoal = (player.team == Team::BLUE) ? CommonValues::BLUE_GOAL_CENTER : CommonValues::ORANGE_GOAL_CENTER;
        float playerDistToOwnGoal = (player.pos - ownGoal).Length();

        for (auto& other : state.players) {
            if (other.team == player.team) continue;

            float oppDistToBall = (other.pos - state.ball.pos).Length();
            if (oppDistToBall > POSSESSION_DIST) continue;

            float ballDistToOwnGoal = (state.ball.pos - ownGoal).Length();
            if (playerDistToOwnGoal >= ballDistToOwnGoal) continue;

            float velTowardBall = std::max(0.0f, (state.ball.pos - player.pos).Normalized().Dot(player.vel));
            if (velTowardBall < MIN_VEL_TOWARD_BALL) continue;

            return RS_MIN(1.0f, velTowardBall / 2300.0f);
        }

        return 0.0f;
    }
};

class BoostEfficiencyReward : public Reward {
public:
    static constexpr int FRAMES_WINDOW = 1200;
    static constexpr float PICKUP_RADIUS = 800.0f;
    static constexpr float BOOST_USE_THRESHOLD = 1.0f;

    struct PickupEvent {
        int frame;
        Vec pos;
    };

    struct PlayerBoostState {
        std::vector<PickupEvent> pickups;
        float prevBoost = 0.0f;
        int frame = 0;
        bool rewardedThisSequence = false;
    };

    std::unordered_map<uint32_t, PlayerBoostState> playerStates;
    static inline int globalFrame = 0;

    void Reset(const GameState& initialState) override {
        playerStates.clear();
        globalFrame = 0;
    }

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        auto& bs = playerStates[player.carId];
        bs.frame = globalFrame++;

        if (player.boost > bs.prevBoost + 5.0f) {
            bs.pickups.push_back({ bs.frame, player.pos });
            bs.rewardedThisSequence = false;
        }

        float boostUsed = bs.prevBoost - player.boost;
        if (boostUsed > BOOST_USE_THRESHOLD && !bs.rewardedThisSequence) {
            for (auto it = bs.pickups.begin(); it != bs.pickups.end(); ) {
                if (bs.frame - it->frame > FRAMES_WINDOW) {
                    it = bs.pickups.erase(it);
                    continue;
                }

                float distToPickup = (player.pos - it->pos).Length();
                if (distToPickup <= PICKUP_RADIUS) {
                    bs.rewardedThisSequence = true;
                    bs.pickups.clear();
                    bs.prevBoost = player.boost;
                    return 1.0f;
                }
                ++it;
            }
        }

        bs.prevBoost = player.boost;
        return 0.0f;
    }
};

class RotationTimingReward : public Reward {
public:
    struct RotationState {
        bool wasInDefensiveHalf = true;
        bool ballWasInDefHalf = true;
        bool rewardedThisTransition = false;
    };

    std::unordered_map<uint32_t, RotationState> playerStates;

    void Reset(const GameState& initialState) override {
        playerStates.clear();
    }

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        auto& rs = playerStates[player.carId];

        bool ballInDefHalf = (player.team == Team::BLUE) ? (state.ball.pos.y < 0) : (state.ball.pos.y > 0);
        bool playerInDefHalf = (player.team == Team::BLUE) ? (player.pos.y < 0) : (player.pos.y > 0);

        if (rs.ballWasInDefHalf && !ballInDefHalf && !rs.rewardedThisTransition) {
            Vec ownGoal = (player.team == Team::BLUE) ? CommonValues::BLUE_GOAL_CENTER : CommonValues::ORANGE_GOAL_CENTER;
            Vec toGoal = (ownGoal - player.pos).Normalized();
            Vec ballToGoalDir = (ownGoal - state.ball.pos).Normalized();
            float alignment = toGoal.Dot(ballToGoalDir);

            if (alignment > 0.5f) {
                rs.rewardedThisTransition = true;
                return alignment;
            }
        }

        if (ballInDefHalf) {
            rs.rewardedThisTransition = false;
        }

        rs.ballWasInDefHalf = ballInDefHalf;
        rs.wasInDefensiveHalf = playerInDefHalf;
        return 0.0f;
    }
};

class BackboardReadReward : public Reward {
public:
    static constexpr float BACKBOARD_DIST = 200.0f;
    static constexpr float BACKBOARD_Z_MAX = 600.0f;

    struct BackboardState {
        bool ballHitBackboard = false;
        int framesSinceHit = 0;
        static constexpr int READ_WINDOW = 60;
    };

    std::unordered_map<uint32_t, BackboardState> playerStates;

    void Reset(const GameState& initialState) override {
        playerStates.clear();
    }

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        auto& bs = playerStates[player.carId];

        if (state.prev) {
            float prevBallY = state.prev->ball.pos.y;
            float curBallY = state.ball.pos.y;
            float ballX = std::abs(state.ball.pos.x);
            float ballZ = state.ball.pos.z;
            float wallX = CommonValues::SIDE_WALL_X;

            if (ballX > wallX - BACKBOARD_DIST && ballZ < BACKBOARD_Z_MAX &&
                std::abs(state.ball.vel.x) > 200.0f) {
                bs.ballHitBackboard = true;
                bs.framesSinceHit = 0;
            }
        }

        if (bs.ballHitBackboard) {
            bs.framesSinceHit++;
            if (bs.framesSinceHit > BackboardState::READ_WINDOW) {
                bs.ballHitBackboard = false;
                return 0.0f;
            }

            if (player.ballTouchedStep) {
                bs.ballHitBackboard = false;
                return 1.0f;
            }
        }

        return 0.0f;
    }
};
