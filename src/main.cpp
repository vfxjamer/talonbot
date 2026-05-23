#include <GigaLearnCPP/Learner.h>

#include <RLGymCPP/Rewards/CommonRewards.h>
#include <RLGymCPP/Rewards/ZeroSumReward.h>
#include <RLGymCPP/TerminalConditions/NoTouchCondition.h>
#include <RLGymCPP/TerminalConditions/GoalScoreCondition.h>
#include <RLGymCPP/OBSBuilders/DefaultObsPadded.h>
#include <RLGymCPP/StateSetters/KickoffState.h>
#include <RLGymCPP/StateSetters/FuzzedKickoffState.h>
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/StateSetters/CombinedState.h>
#include <RLGymCPP/ActionParsers/DefaultAction.h>

#include "KickoffReward.h"
#include "DemoDodgeReward.h"
#include "PredictiveDefenseReward.h"
#include "DefenseRotationReward.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cstdint>

using namespace GGL;
using namespace RLGC;

namespace fs = std::filesystem;

static constexpr uint64_t PHASE_2_THRESHOLD = 30'000'000'000ULL;
static constexpr uint64_t PHASE_3_THRESHOLD = 100'000'000'000ULL;
static constexpr uint64_t PHASE_4_THRESHOLD = 220'000'000'000ULL;
static constexpr uint64_t PHASE_5_THRESHOLD = 340'000'000'000ULL;
static constexpr uint64_t MAX_STEPS = 400'000'000'000ULL;

static int g_detectedPhase = 1;

int DetectPhase(const std::string& checkpointFolder) {
	uint64_t maxSteps = 0;

	if (fs::exists(checkpointFolder)) {
		for (auto& entry : fs::directory_iterator(checkpointFolder)) {
			if (entry.is_directory()) {
				try {
					uint64_t steps = std::stoull(entry.path().filename().string());
					maxSteps = std::max(maxSteps, steps);
				} catch (...) {}
			}
		}
	}

	if (maxSteps < PHASE_2_THRESHOLD) return 1;
	if (maxSteps < PHASE_3_THRESHOLD) return 2;
	if (maxSteps < PHASE_4_THRESHOLD) return 3;
	if (maxSteps < PHASE_5_THRESHOLD) return 4;
	return 5;
}

static std::vector<WeightedReward> BuildRewards(int phase) {
	switch (phase) {
		case 1:
			return {
				{ new KickoffReward(), 5.0f },
				{ new VelocityPlayerToBallReward(), 2.0f },
				{ new FaceBallReward(), 1.0f },
				{ new TouchBallReward(), 4.0f },
				{ new GoalReward(-1.0f), 60.0f },
				{ new SaveBoostReward(), 0.3f },
				{ new ZeroSumReward(new StrongTouchReward(20, 100), 0.5f), 15.0f }
			};

		case 2:
			return {
				{ new KickoffReward(), 1.5f },
				{ new VelocityPlayerToBallReward(), 1.2f },
				{ new TouchBallReward(), 3.5f },
				{ new VelocityReward(), 0.12f },
				{ new SaveBoostReward(), 0.5f },
				{ new GoalReward(-1.0f), 100.0f },
				{ new ZeroSumReward(new StrongTouchReward(20, 100), 0.5f), 40.0f },
				{ new ZeroSumReward(new VelocityReward(), 0.5f, 0.5f), 6.0f },
				{ new DemoDodgeReward(), 2.5f },
				{ new DefenseRotationReward(), 0.2f },
				{ new PredictiveDefenseReward(), 0.3f }
			};

		case 3:
			return {
				{ new KickoffReward(), 1.0f },
				{ new VelocityPlayerToBallReward(), 1.0f },
				{ new TouchBallReward(), 3.0f },
				{ new AirReward(), 3.0f },
				{ new VelocityReward(), 0.1f },
				{ new SaveBoostReward(), 0.4f },
				{ new GoalReward(-1.0f), 130.0f },
				{ new ZeroSumReward(new StrongTouchReward(20, 100), 0.5f), 60.0f },
				{ new ZeroSumReward(new VelocityReward(), 0.5f), 10.0f },
				{ new DemoDodgeReward(), 1.5f },
				{ new DefenseRotationReward(), 0.6f },
				{ new PredictiveDefenseReward(), 0.8f }
			};

		case 4:
			return {
				{ new KickoffReward(), 0.5f },
				{ new VelocityPlayerToBallReward(), 0.75f },
				{ new FaceBallReward(), 0.3f },
				{ new TouchBallReward(), 2.5f },
				{ new AirReward(), 1.5f },
				{ new VelocityReward(), 0.1f },
				{ new SaveBoostReward(), 0.5f },
				{ new GoalReward(-1.0f), 150.0f },
				{ new ZeroSumReward(new StrongTouchReward(20, 100), 0.5f), 75.0f },
				{ new ZeroSumReward(new VelocityReward(), 0.5f), 12.0f },
				{ new DemoDodgeReward(), 1.2f },
				{ new PredictiveDefenseReward(), 2.5f },
				{ new DefenseRotationReward(), 2.0f }
			};

		case 5:
		default:
			return {
				{ new KickoffReward(), 0.5f },
				{ new VelocityPlayerToBallReward(), 0.75f },
				{ new FaceBallReward(), 0.3f },
				{ new TouchBallReward(), 2.5f },
				{ new AirReward(), 1.5f },
				{ new VelocityReward(), 0.1f },
				{ new SaveBoostReward(), 0.5f },
				{ new GoalReward(-1.0f), 170.0f },
				{ new ZeroSumReward(new StrongTouchReward(20, 100), 0.5f), 90.0f },
				{ new ZeroSumReward(new VelocityReward(), 0.5f), 12.0f },
				{ new DemoDodgeReward(), 1.2f },
				{ new PredictiveDefenseReward(), 2.5f },
				{ new DefenseRotationReward(), 2.0f }
			};
	}
}

static StateSetter* BuildStateSetter(int phase) {
	switch (phase) {
		case 1:
			return new KickoffState();

		case 2:
			return new CombinedState({
				{ new KickoffState(), 0.6f },
				{ new FuzzedKickoffState(), 0.4f }
			});

		case 3:
			return new CombinedState({
				{ new KickoffState(), 0.25f },
				{ new FuzzedKickoffState(), 0.30f },
				{ new RandomState(true, true, true), 0.45f }
			});

		case 4:
		case 5:
		default:
			return new CombinedState({
				{ new KickoffState(), 0.20f },
				{ new FuzzedKickoffState(), 0.25f },
				{ new RandomState(true, true, true), 0.55f }
			});
	}
}

static std::vector<TerminalCondition*> BuildTerminals(int phase) {
	std::vector<TerminalCondition*> terms = { new GoalScoreCondition() };

	switch (phase) {
		case 1: terms.push_back(new NoTouchCondition(8)); break;
		case 2: terms.push_back(new NoTouchCondition(12)); break;
		case 3: terms.push_back(new NoTouchCondition(25)); break;
		case 4: terms.push_back(new NoTouchCondition(35)); break;
		case 5: terms.push_back(new NoTouchCondition(45)); break;
	}

	return terms;
}

void StepCallback(Learner* learner, const std::vector<GameState>& states, Report& report) {
	static bool milestone30 = false;
	static bool milestone100 = false;
	static bool milestone220 = false;
	static bool milestone340 = false;

	uint64_t ts = learner->totalTimesteps;

	if (ts >= PHASE_2_THRESHOLD && !milestone30) {
		RG_LOG("=== PHASE 1 COMPLETE at " << ts << " steps. Beginning Phase 2 (Ground Mechanics) ===");
		milestone30 = true;
	}
	if (ts >= PHASE_3_THRESHOLD && !milestone100) {
		RG_LOG("=== PHASE 2 COMPLETE at " << ts << " steps. Beginning Phase 3 (Aerial Mechanics) ===");
		milestone100 = true;
	}
	if (ts >= PHASE_4_THRESHOLD && !milestone220) {
		RG_LOG("=== PHASE 3 COMPLETE at " << ts << " steps. Beginning Phase 4 (Offense & Defense) ===");
		milestone220 = true;
	}
	if (ts >= PHASE_5_THRESHOLD && !milestone340) {
		RG_LOG("=== PHASE 4 COMPLETE at " << ts << " steps. Beginning Phase 5 (Full Optimization) ===");
		milestone340 = true;
	}

	for (auto& state : states) {
		if (state.goalScored) {
			bool blueScored = state.ball.pos.y > 0;
			if (blueScored)
				report.AddAvg("Talon/GoalsScored", 1.0f);
			else
				report.AddAvg("Talon/GoalsConceded", 1.0f);
		}

		for (auto& player : state.players) {
			if (player.team != Team::BLUE)
				continue;

			if (player.ballTouchedStep) {
				report.AddAvg("Talon/BallTouches", 1.0f);
				if (state.ball.vel.Length() < 400.0f)
					report.AddAvg("Talon/KickoffWins", 1.0f);
			}

			if (player.eventState.demo)
				report.AddAvg("Talon/DemosLanded", 1.0f);
		}
	}
}

int main(int argc, char* argv[]) {
	std::srand((unsigned int)std::time(nullptr));

	fs::path projectRoot = fs::current_path();
	fs::path checkpointPath = projectRoot / "checkpoints";

	std::string collisionMeshesPath = "collision_meshes";
	if (argc > 1)
		collisionMeshesPath = argv[1];

	RocketSim::Init(collisionMeshesPath);

	int phase = DetectPhase(checkpointPath.string());
	g_detectedPhase = phase;

	RG_LOG(RG_DIVIDER);
	RG_LOG("  TALOS V1 — Rocket League 1v1 Training Bot");
	RG_LOG("  Detected Phase: " << phase);
	RG_LOG("  Checkpoint folder: " << checkpointPath.string());
	RG_LOG(RG_DIVIDER);

	LearnerConfig cfg = {};

	cfg.deviceType = LearnerDeviceType::GPU_CUDA;
	cfg.numGames = 40;
	cfg.tickSkip = 8;
	cfg.actionDelay = cfg.tickSkip - 1;
	cfg.checkpointFolder = checkpointPath.string();
	cfg.tsPerSave = 500'000'000;
	cfg.checkpointsToKeep = -1;
	cfg.sendMetrics = false;
	cfg.renderMode = false;
	cfg.randomSeed = -1;
	cfg.standardizeObs = true;
	cfg.standardizeReturns = true;
	cfg.addRewardsToMetrics = true;

	cfg.skillTracker.enabled = false;

	cfg.ppo.tsPerItr = 50000;
	cfg.ppo.batchSize = 50000;
	cfg.ppo.miniBatchSize = 10000;
	cfg.ppo.epochs = 2;
	cfg.ppo.policyLR = 3e-4f;
	cfg.ppo.criticLR = 3e-4f;
	cfg.ppo.clipRange = 0.2f;
	cfg.ppo.gaeLambda = 0.95f;

	cfg.ppo.sharedHead.layerSizes = { 256, 256 };
	cfg.ppo.sharedHead.activationType = ModelActivationType::LEAKY_RELU;
	cfg.ppo.sharedHead.optimType = ModelOptimType::ADAM;
	cfg.ppo.sharedHead.addLayerNorm = true;
	cfg.ppo.sharedHead.addOutputLayer = false;

	cfg.ppo.policy.layerSizes = { 256, 256 };
	cfg.ppo.policy.activationType = ModelActivationType::LEAKY_RELU;
	cfg.ppo.policy.optimType = ModelOptimType::ADAM;
	cfg.ppo.policy.addLayerNorm = true;

	cfg.ppo.critic.layerSizes = { 256, 256 };
	cfg.ppo.critic.activationType = ModelActivationType::LEAKY_RELU;
	cfg.ppo.critic.optimType = ModelOptimType::ADAM;
	cfg.ppo.critic.addLayerNorm = true;

	switch (phase) {
		case 1:
			cfg.ppo.gaeGamma = 0.990f;
			cfg.ppo.entropyScale = 0.05f;
			break;
		case 2:
			cfg.ppo.gaeGamma = 0.993f;
			cfg.ppo.entropyScale = 0.07f;
			break;
		case 3:
			cfg.ppo.gaeGamma = 0.995f;
			cfg.ppo.entropyScale = 0.10f;
			break;
		case 4:
			cfg.ppo.gaeGamma = 0.997f;
			cfg.ppo.entropyScale = 0.08f;
			break;
		case 5:
			cfg.ppo.gaeGamma = 0.998f;
			cfg.ppo.entropyScale = 0.04f;
			break;
	}

	auto envCreateFunc = [phase](int /*index*/) -> EnvCreateResult {
		EnvCreateResult result = {};

		auto arena = Arena::Create(GameMode::SOCCAR);
		arena->AddCar(Team::BLUE);
		arena->AddCar(Team::ORANGE);

		result.arena = arena;
		result.rewards = BuildRewards(phase);
		result.terminalConditions = BuildTerminals(phase);
		result.stateSetter = BuildStateSetter(phase);
		result.obsBuilder = new DefaultObsPadded(2);
		result.actionParser = new DefaultAction();

		return result;
	};

	Learner* learner = new Learner(envCreateFunc, cfg, StepCallback);

	RG_LOG("Loaded checkpoint. Starting training from Phase " << phase << "...");
	RG_LOG(RG_DIVIDER);

	learner->Start();

	delete learner;
	return EXIT_SUCCESS;
}
