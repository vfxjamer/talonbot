#pragma once
#include <RLGymCPP/Rewards/Reward.h>
#include <RLGymCPP/Math.h>
#include <RLGymCPP/CommonValues.h>

namespace RLGC {

class KickoffReward : public Reward {
public:
	virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
		uint64_t tickCount = state.lastTickCount;
		float ballSpeed = state.ball.vel.Length();

		if (tickCount >= 60 || ballSpeed >= 400.0f)
			return 0.0f;

		float distToBall = (player.pos - state.ball.pos).Length();
		if (distToBall > 800.0f)
			return 0.0f;

		float reward = 0.0f;

		float proximity = 1.0f - (distToBall / 800.0f);
		reward += 0.5f * proximity;

		Vec dirToBall = (state.ball.pos - player.pos).Normalized();
		float velTowardBall = RS_MAX(0.0f, dirToBall.Dot(player.vel)) / CommonValues::CAR_MAX_SPEED;
		reward += 0.3f * velTowardBall;

		Vec oppGoal = (player.team == Team::BLUE) ? CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;
		Vec ballDirToGoal = (oppGoal - state.ball.pos).Normalized();
		float ballSpeedTowardGoal = RS_MAX(0.0f, ballDirToGoal.Dot(state.ball.vel)) / CommonValues::BALL_MAX_SPEED;
		reward += 0.2f * ballSpeedTowardGoal;

		return reward;
	}
};

}
