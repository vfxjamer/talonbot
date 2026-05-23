#pragma once
#include <RLGymCPP/Rewards/Reward.h>
#include <RLGymCPP/Math.h>

namespace RLGC {

class DemoDodgeReward : public Reward {
private:
	uint64_t lastDemoedTick = 0;
	bool dodgeRewardedThisStep = false;

public:
	virtual void Reset(const GameState& initialState) override {
		lastDemoedTick = 0;
		dodgeRewardedThisStep = false;
	}

	virtual void PreStep(const GameState& state) override {
		dodgeRewardedThisStep = false;
	}

	virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
		float reward = 0.0f;

		if (player.eventState.demo)
			reward += 1.0f;

		if (player.eventState.demoed && (state.lastTickCount - lastDemoedTick > 30)) {
			reward -= 0.5f;
			lastDemoedTick = state.lastTickCount;
		}

		if (!dodgeRewardedThisStep && player.isFlipping) {
			for (auto& other : state.players) {
				if (other.carId == player.carId)
					continue;

				float distToOpp = (other.pos - player.pos).Length();
				if (distToOpp < 600.0f && other.vel.Length() > 1000.0f) {
					Vec dirToBot = (player.pos - other.pos).Normalized();
					if (other.rotMat.forward.Dot(dirToBot) > 0.848f) {
						reward += 0.4f;
						dodgeRewardedThisStep = true;
						break;
					}
				}
			}
		}

		return reward;
	}
};

}
