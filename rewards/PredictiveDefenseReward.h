#pragma once
#include <RLGymCPP/Rewards/Reward.h>
#include <RLGymCPP/Math.h>
#include <RLGymCPP/CommonValues.h>
#include <cmath>

namespace RLGC {

class PredictiveDefenseReward : public Reward {
public:
	virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
		Vec ownGoal = (player.team == Team::BLUE) ? CommonValues::BLUE_GOAL_BACK : CommonValues::ORANGE_GOAL_BACK;

		bool ballInDefHalf = (player.team == Team::BLUE) ? (state.ball.pos.y < 0) : (state.ball.pos.y > 0);
		if (!ballInDefHalf)
			return 0.0f;

		Vec ballToGoal = (ownGoal - state.ball.pos).Normalized();
		float ballToGoalDist = (ownGoal - state.ball.pos).Length();

		Vec botToBall = player.pos - state.ball.pos;
		float projection = botToBall.Dot(ballToGoal);
		float frac = projection / ballToGoalDist;

		if (frac < 0.0f || frac > 1.0f)
			return 0.0f;

		float optimalFrac = 0.7f;
		float diff = (frac - optimalFrac) / 0.3f;
		float alignment = std::exp(-diff * diff);

		Vec closestPoint = state.ball.pos + ballToGoal * projection;
		float distToLine = (player.pos - closestPoint).Length();
		float lineAlign = 1.0f - RS_MIN(1.0f, distToLine / 500.0f);

		return alignment * lineAlign;
	}
};

}
