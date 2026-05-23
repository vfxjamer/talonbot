#pragma once
#include <RLGymCPP/Rewards/Reward.h>
#include <RLGymCPP/Math.h>
#include <RLGymCPP/CommonValues.h>

namespace RLGC {

class DefenseRotationReward : public Reward {
public:
	virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
		Vec ownGoal = (player.team == Team::BLUE) ? CommonValues::BLUE_GOAL_BACK : CommonValues::ORANGE_GOAL_BACK;

		bool ballInDefHalf = (player.team == Team::BLUE) ? (state.ball.pos.y < 0) : (state.ball.pos.y > 0);
		if (!ballInDefHalf)
			return 0.0f;

		float distToGoal = (player.pos - ownGoal).Length();
		float ballDistToGoal = (state.ball.pos - ownGoal).Length();

		if (distToGoal > ballDistToGoal * 0.6f)
			return -0.5f * (distToGoal / (ballDistToGoal + 1.0f));

		Vec dirToGoal = (ownGoal - player.pos).Normalized();
		float velTowardGoal = RS_MAX(0.0f, dirToGoal.Dot(player.vel)) / CommonValues::CAR_MAX_SPEED;
		float threatLevel = 1.0f - RS_MIN(1.0f, ballDistToGoal / 4000.0f);

		return 0.5f * velTowardGoal * threatLevel;
	}
};

}
