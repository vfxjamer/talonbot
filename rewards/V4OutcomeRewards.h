#pragma once
#include <RLGymCPP/Rewards/Reward.h>
#include <cmath>

using namespace RLGC;

class EpicSaveReward : public Reward {
public:
    static constexpr float EPIC_SAVE_DIST_THRESHOLD = 300.0f;

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (!player.eventState.save)
            return 0.0f;

        float distToGoalLine = std::abs(state.ball.pos.y) - CommonValues::BACK_WALL_Y;
        if (distToGoalLine > EPIC_SAVE_DIST_THRESHOLD)
            return 0.0f;

        float intensity = 1.0f - (distToGoalLine / EPIC_SAVE_DIST_THRESHOLD);
        return RS_CLAMP(intensity, 0.0f, 1.0f);
    }
};

class V4GoalReward : public Reward {
public:
    float scoredValue;
    float concededValue;

    V4GoalReward(float scored, float conceded) : scoredValue(scored), concededValue(conceded) {}

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (!state.goalScored)
            return 0.0f;
        bool scored = (player.team != RS_TEAM_FROM_Y(state.ball.pos.y));
        return scored ? scoredValue : concededValue;
    }
};

class OwnGoalReward : public Reward {
public:
    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (!state.goalScored)
            return 0.0f;
        if (player.team != RS_TEAM_FROM_Y(state.ball.pos.y))
            return 0.0f;
        if ((int)player.carId == state.lastTouchCarID)
            return -1.0f;
        return 0.0f;
    }
};
