#pragma once
#include <RLGymCPP/Rewards/Reward.h>
#include <cmath>
#include <unordered_map>

using namespace RLGC;

class KickoffWinReward : public Reward {
public:
    static constexpr float BALL_SPEED_THRESHOLD = 400.0f;
    static constexpr float DIST_THRESHOLD = 800.0f;

    void Reset(const GameState& initialState) override {}

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (state.ball.updateCounter > 60)
            return 0.0f;

        float ballSpeed = state.ball.vel.Length();
        if (ballSpeed > BALL_SPEED_THRESHOLD)
            return 0.0f;

        float dist = (player.pos - state.ball.pos).Length();
        if (dist > DIST_THRESHOLD)
            return 0.0f;

        float proximity = 1.0f / (1.0f + dist * 0.002f);

        Vec toBall = (state.ball.pos - player.pos).Normalized();
        float velTowardBall = std::max(0.0f, toBall.Dot(player.vel)) / 2300.0f;

        Vec targetGoal = (player.team == Team::BLUE) ? CommonValues::ORANGE_GOAL_CENTER : CommonValues::BLUE_GOAL_CENTER;
        Vec ballToGoal = (targetGoal - state.ball.pos).Normalized();
        float ballGoalDir = std::max(0.0f, ballToGoal.Dot(state.ball.vel)) / 6000.0f;

        return proximity * 0.5f + velTowardBall * 0.3f + ballGoalDir * 0.2f;
    }
};

class DirectionalTouchReward : public Reward {
public:
    static constexpr float DIR_DOT_THRESHOLD = 0.5f;

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (!player.ballTouchedStep)
            return 0.0f;

        Vec targetGoal = (player.team == Team::BLUE) ? CommonValues::ORANGE_GOAL_CENTER : CommonValues::BLUE_GOAL_CENTER;
        Vec ballToGoal = (targetGoal - state.ball.pos).Normalized();
        float dot = ballToGoal.Dot(state.ball.vel) / CommonValues::BALL_MAX_SPEED;

        if (dot < DIR_DOT_THRESHOLD)
            return 0.0f;

        return RS_CLAMP(dot, 0.0f, 1.0f);
    }
};

class BadTouchReward : public Reward {
public:
    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (!player.ballTouchedStep)
            return 0.0f;

        Vec ownGoal = (player.team == Team::BLUE) ? CommonValues::BLUE_GOAL_CENTER : CommonValues::ORANGE_GOAL_CENTER;
        Vec ballToOwnGoal = (ownGoal - state.ball.pos).Normalized();
        float dot = ballToOwnGoal.Dot(state.ball.vel) / CommonValues::BALL_MAX_SPEED;

        if (dot > 0.0f)
            return 0.0f;

        return RS_CLAMP(dot, -1.0f, 0.0f);
    }
};

class AerialTouchReward : public Reward {
public:
    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (!player.ballTouchedStep || player.isOnGround)
            return 0.0f;

        return RS_MIN(1.0f, state.ball.pos.z / 1500.0f);
    }
};

class AirDribbleReward : public Reward {
public:
    struct DribbleState {
        int aerialTouchCount = 0;
        bool wasInAir = false;
    };

    std::unordered_map<uint32_t, DribbleState> playerStates;

    void Reset(const GameState& initialState) override {
        playerStates.clear();
    }

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        auto& ds = playerStates[player.carId];

        if (player.isOnGround) {
            ds.aerialTouchCount = 0;
            ds.wasInAir = false;
            return 0.0f;
        }

        if (!ds.wasInAir && !player.isOnGround) {
            ds.wasInAir = true;
        }

        if (!player.ballTouchedStep || !ds.wasInAir)
            return 0.0f;

        ds.aerialTouchCount++;
        if (ds.aerialTouchCount <= 1)
            return 0.0f;

        return 1.0f;
    }
};

class RedirectOnTargetReward : public Reward {
public:
    static constexpr float MIN_BALL_SPEED = 80.0f;
    static constexpr float MAX_ANGLE_DEG = 45.0f;
    static constexpr float MAX_ANGLE_COS = 0.707f;

    float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (!player.ballTouchedStep || player.isOnGround)
            return 0.0f;

        float ballSpeed = state.ball.vel.Length();
        if (ballSpeed < MIN_BALL_SPEED)
            return 0.0f;

        Vec targetGoal = (player.team == Team::BLUE) ? CommonValues::ORANGE_GOAL_CENTER : CommonValues::BLUE_GOAL_CENTER;
        Vec ballToGoal = (targetGoal - state.ball.pos).Normalized();
        float angleCos = ballToGoal.Dot(state.ball.vel / ballSpeed);

        if (angleCos < MAX_ANGLE_COS)
            return 0.0f;

        float speedFactor = RS_MIN(1.0f, ballSpeed / 2000.0f);
        return angleCos * speedFactor;
    }
};
