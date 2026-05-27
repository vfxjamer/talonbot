#include <GigaLearnCPP/Learner.h>

#include <RLGymCPP/Rewards/CommonRewards.h>
#include <RLGymCPP/Rewards/ZeroSumReward.h>
#include <RLGymCPP/TerminalConditions/NoTouchCondition.h>
#include <RLGymCPP/TerminalConditions/GoalScoreCondition.h>
#include <RLGymCPP/ObsBuilders/DefaultObsPadded.h>
#include <RLGymCPP/StateSetters/KickoffState.h>
#include <RLGymCPP/StateSetters/FuzzedKickoffState.h>
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/StateSetters/CombinedState.h>
#include <RLGymCPP/ActionParsers/DefaultAction.h>

#include "V4OutcomeRewards.h"
#include "V4MechanicsRewards.h"
#include "V4GameSenseRewards.h"
#include "V4PenaltyRewards.h"
#include "V4ComboRewards.h"

#include <cmath>
#include <string>
#include <vector>
#include <filesystem>

using namespace GGL;
using namespace RLGC;

static constexpr uint64_t PHASE_2_THRESHOLD = 30'000'000'000ULL;
static constexpr uint64_t PHASE_3_THRESHOLD = 100'000'000'000ULL;
static constexpr uint64_t PHASE_4_THRESHOLD = 220'000'000'000ULL;
static constexpr uint64_t PHASE_5_THRESHOLD = 340'000'000'000ULL;
static constexpr uint64_t MAX_STEPS = 400'000'000'000ULL;

int DetectPhase() {
    uint64_t maxSteps = 0;
    if (std::filesystem::exists("checkpoints")) {
        for (auto& entry : std::filesystem::directory_iterator("checkpoints")) {
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

struct PhaseConfig {
    float       gamma;
    float       entropy;
    const char* name;
    const char* description;
    int64_t     targetSteps;
};

static PhaseConfig PHASES[] = {
    { 0.0f, 0.0f, "INVALID", "INVALID", 0LL },

    { 0.990f, 0.05f,
      "Phase 1: Kickoffs",
      "Kickoff wins + ball direction + goals",
      30'000'000'000LL },

    { 0.993f, 0.07f,
      "Phase 2: Gameplay",
      "Scoring, saves, demos, mechanics, shadow defense",
      70'000'000'000LL },

    { 0.995f, 0.10f,
      "Phase 3: Mechanics",
      "Aerials, air dribbles, redirects, backboard reads",
      120'000'000'000LL },

    { 0.997f, 0.08f,
      "Phase 4: Game Sense",
      "Scaling demos, rotation, read chains",
      120'000'000'000LL },

    { 0.998f, 0.05f,
      "Phase 5: Optimization",
      "Sharpen everything, reduce mistakes, strategic demos",
      60'000'000'000LL },
};

std::vector<WeightedReward> GetPhase1Rewards() {
    return std::vector<WeightedReward>{
        WeightedReward(new V4GoalReward(80.0f, -60.0f),                             1.0f),
        WeightedReward(new SaveReward(),                                             30.0f),
        WeightedReward(new DemoReward(),                                             10.0f),
        WeightedReward(new OwnGoalReward(),                                          50.0f),

        WeightedReward(new KickoffWinReward(),                                       40.0f),
        WeightedReward(new DirectionalTouchReward(),                                 8.0f),

        WeightedReward(new WhiffPenalty(),                                           15.0f),
        WeightedReward(new DriveByWhiffPenalty(),                                    10.0f),
        WeightedReward(new DemoedPenalty(),                                          10.0f),
    };
}

std::vector<WeightedReward> GetPhase2Rewards() {
    return std::vector<WeightedReward>{
        WeightedReward(new V4GoalReward(100.0f, -100.0f),                           1.0f),
        WeightedReward(new SaveReward(),                                             50.0f),
        WeightedReward(new EpicSaveReward(),                                         45.0f),
        WeightedReward(new AssistReward(),                                           30.0f),
        WeightedReward(new DemoReward(),                                             10.0f),
        WeightedReward(new OwnGoalReward(),                                          100.0f),

        WeightedReward(new KickoffWinReward(),                                       25.0f),
        WeightedReward(new DirectionalTouchReward(),                                 8.0f),
        WeightedReward(new BadTouchReward(),                                         10.0f),
        WeightedReward(new AerialTouchReward(),                                      10.0f),

        WeightedReward(new ShadowDefenseReward(),                                    2.0f),
        WeightedReward(new BoostEfficiencyReward(),                                  1.0f),
        WeightedReward(new RotationTimingReward(),                                   8.0f),

        WeightedReward(new WhiffPenalty(),                                           20.0f),
        WeightedReward(new DriveByWhiffPenalty(),                                    15.0f),
        WeightedReward(new BallChasingPenalty(),                                     3.0f),
        WeightedReward(new DemoedPenalty(),                                          15.0f),
        WeightedReward(new BoostStarvationPenalty(),                                 1.0f),
    };
}

std::vector<WeightedReward> GetPhase3Rewards() {
    return std::vector<WeightedReward>{
        WeightedReward(new V4GoalReward(100.0f, -100.0f),                           1.0f),
        WeightedReward(new SaveReward(),                                             50.0f),
        WeightedReward(new EpicSaveReward(),                                         65.0f),
        WeightedReward(new AssistReward(),                                           50.0f),
        WeightedReward(new DemoReward(),                                             10.0f),
        WeightedReward(new OwnGoalReward(),                                          100.0f),

        WeightedReward(new KickoffWinReward(),                                       15.0f),
        WeightedReward(new DirectionalTouchReward(),                                 10.0f),
        WeightedReward(new BadTouchReward(),                                         12.0f),
        WeightedReward(new AerialTouchReward(),                                      20.0f),
        WeightedReward(new AirDribbleReward(),                                       15.0f),
        WeightedReward(new RedirectOnTargetReward(),                                 30.0f),

        WeightedReward(new ShadowDefenseReward(),                                    2.0f),
        WeightedReward(new BoostEfficiencyReward(),                                  2.0f),
        WeightedReward(new RotationTimingReward(),                                   8.0f),
        WeightedReward(new BackboardReadReward(),                                    20.0f),

        WeightedReward(new WhiffPenalty(),                                           25.0f),
        WeightedReward(new DriveByWhiffPenalty(),                                    20.0f),
        WeightedReward(new BallChasingPenalty(),                                     4.0f),
        WeightedReward(new DemoedPenalty(),                                          20.0f),
        WeightedReward(new BoostStarvationPenalty(),                                 2.0f),

        WeightedReward(new AerialGoalMultiplierReward(),                             50.0f),
        WeightedReward(new AirDribbleFlickGoalReward(),                              200.0f),
    };
}

std::vector<WeightedReward> GetPhase4Rewards() {
    return std::vector<WeightedReward>{
        WeightedReward(new V4GoalReward(100.0f, -100.0f),                           1.0f),
        WeightedReward(new SaveReward(),                                             50.0f),
        WeightedReward(new EpicSaveReward(),                                         65.0f),
        WeightedReward(new AssistReward(),                                           50.0f),
        WeightedReward(new DemoReward(),                                             20.0f),
        WeightedReward(new OwnGoalReward(),                                          100.0f),

        WeightedReward(new KickoffWinReward(),                                       10.0f),
        WeightedReward(new DirectionalTouchReward(),                                 8.0f),
        WeightedReward(new BadTouchReward(),                                         12.0f),
        WeightedReward(new AerialTouchReward(),                                      20.0f),
        WeightedReward(new AirDribbleReward(),                                       15.0f),
        WeightedReward(new RedirectOnTargetReward(),                                 30.0f),

        WeightedReward(new ShadowDefenseReward(),                                    5.0f),
        WeightedReward(new BoostEfficiencyReward(),                                  3.0f),
        WeightedReward(new RotationTimingReward(),                                   15.0f),
        WeightedReward(new BackboardReadReward(),                                    25.0f),

        WeightedReward(new WhiffPenalty(),                                           25.0f),
        WeightedReward(new DriveByWhiffPenalty(),                                    20.0f),
        WeightedReward(new BallChasingPenalty(),                                     5.0f),
        WeightedReward(new DemoedPenalty(),                                          20.0f),
        WeightedReward(new BoostStarvationPenalty(),                                 3.0f),

        WeightedReward(new AerialGoalMultiplierReward(),                             50.0f),
        WeightedReward(new AirDribbleFlickGoalReward(),                              200.0f),
        WeightedReward(new ReadChainReward(),                                        120.0f),
    };
}

std::vector<WeightedReward> GetPhase5Rewards() {
    return std::vector<WeightedReward>{
        WeightedReward(new V4GoalReward(100.0f, -100.0f),                           1.0f),
        WeightedReward(new SaveReward(),                                             50.0f),
        WeightedReward(new EpicSaveReward(),                                         65.0f),
        WeightedReward(new AssistReward(),                                           50.0f),
        WeightedReward(new DemoReward(),                                             30.0f),
        WeightedReward(new OwnGoalReward(),                                          100.0f),

        WeightedReward(new KickoffWinReward(),                                       10.0f),
        WeightedReward(new DirectionalTouchReward(),                                 5.0f),
        WeightedReward(new BadTouchReward(),                                         15.0f),
        WeightedReward(new AerialTouchReward(),                                      20.0f),
        WeightedReward(new AirDribbleReward(),                                       15.0f),
        WeightedReward(new RedirectOnTargetReward(),                                 30.0f),

        WeightedReward(new ShadowDefenseReward(),                                    5.0f),
        WeightedReward(new BoostEfficiencyReward(),                                  4.0f),
        WeightedReward(new RotationTimingReward(),                                   15.0f),
        WeightedReward(new BackboardReadReward(),                                    25.0f),

        WeightedReward(new WhiffPenalty(),                                           30.0f),
        WeightedReward(new DriveByWhiffPenalty(),                                    25.0f),
        WeightedReward(new BallChasingPenalty(),                                     5.0f),
        WeightedReward(new DemoedPenalty(),                                          20.0f),
        WeightedReward(new BoostStarvationPenalty(),                                 4.0f),

        WeightedReward(new AerialGoalMultiplierReward(),                             50.0f),
        WeightedReward(new AirDribbleFlickGoalReward(),                              250.0f),
        WeightedReward(new ReadChainReward(),                                        150.0f),
    };
}

std::vector<TerminalCondition*> GetPhaseTerminals(int phase) {
    std::vector<TerminalCondition*> conds;
    switch (phase) {
        case 1:  conds.push_back(new NoTouchCondition(10)); break;
        case 2:  conds.push_back(new NoTouchCondition(15)); break;
        case 3:  conds.push_back(new NoTouchCondition(20)); break;
        case 4:  conds.push_back(new NoTouchCondition(30)); break;
        case 5:  conds.push_back(new NoTouchCondition(40)); break;
        default: conds.push_back(new NoTouchCondition(15)); break;
    }
    conds.push_back(new GoalScoreCondition());
    return conds;
}

struct TrainingStats {
    int64_t totalSteps    = 0;
    int64_t phaseSteps    = 0;
    int     episodes      = 0;
    int     currentPhase  = 1;

    float avgGoalsFor     = 0.0f;
    float avgGoalsAgainst = 0.0f;
    float avgTouches      = 0.0f;
    float avgKickoffWins  = 0.0f;
    float avgDemos        = 0.0f;
    float avgSaves        = 0.0f;
};

static TrainingStats g_stats;

StateSetter* GetPhaseStateSetter(int phase) {
    switch (phase) {
        case 1:
            return new KickoffState();
        case 2: {
            std::vector<std::pair<StateSetter*, float>> s = {
                { new KickoffState(),       0.7f },
                { new FuzzedKickoffState(), 0.3f },
            };
            return new CombinedState(s);
        }
        case 3:
        case 4:
        case 5:
        default: {
            std::vector<std::pair<StateSetter*, float>> s = {
                { new KickoffState(),                0.3f },
                { new FuzzedKickoffState(),          0.3f },
                { new RandomState(true, true, true), 0.4f },
            };
            return new CombinedState(s);
        }
    }
}

EnvCreateResult EnvCreateFunc(int index) {
    int phase = g_stats.currentPhase;
    std::vector<WeightedReward> rewards;
    switch (phase) {
        case 1:  rewards = GetPhase1Rewards(); break;
        case 2:  rewards = GetPhase2Rewards(); break;
        case 3:  rewards = GetPhase3Rewards(); break;
        case 4:  rewards = GetPhase4Rewards(); break;
        case 5:  rewards = GetPhase5Rewards(); break;
        default: rewards = GetPhase1Rewards(); break;
    }

    auto terminalConditions = GetPhaseTerminals(phase);
    auto stateSetter        = GetPhaseStateSetter(phase);

    auto arena = Arena::Create(GameMode::SOCCAR);
    arena->AddCar(Team::BLUE,   CAR_CONFIG_OCTANE);
    arena->AddCar(Team::ORANGE, CAR_CONFIG_OCTANE);

    auto obsBuilder   = new DefaultObsPadded(2);
    auto actionParser = new DefaultAction();

    EnvCreateResult result    = {};
    result.actionParser       = actionParser;
    result.obsBuilder         = obsBuilder;
    result.stateSetter        = stateSetter;
    result.terminalConditions = terminalConditions;
    result.rewards            = rewards;
    result.arena              = arena;

    return result;
}

void StepCallback(Learner* learner, const std::vector<GameState>& states, Report& report) {
    g_stats.totalSteps += (int64_t)states.size();
    g_stats.phaseSteps += (int64_t)states.size();

    PhaseConfig& phase = PHASES[g_stats.currentPhase];
    if (g_stats.phaseSteps >= phase.targetSteps) {
        printf("\n");
        printf("============================================================\n");
        printf("  >>> Phase %d COMPLETE! <<<\n", g_stats.currentPhase);
        if (g_stats.currentPhase < 5) {
            printf("  Next restart will begin Phase %d\n", g_stats.currentPhase + 1);
        } else {
            printf("  All 5 phases complete. Bot fully trained.\n");
        }
        printf("============================================================\n\n");
        g_stats.phaseSteps = 0;
    }

    int goalsFor     = 0;
    int goalsAgainst = 0;
    int touches      = 0;
    int kickoffWins  = 0;
    int demos        = 0;
    int saves        = 0;

    for (auto& state : states) {
        if (state.goalScored) {
            if (state.ball.pos.y > 0) goalsFor++;
            else                       goalsAgainst++;
        }

        for (auto& player : state.players) {
            if (player.team == Team::BLUE) {
                if (player.ballTouchedStep) {
                    touches++;
                    float ballSpeed = state.ball.vel.Length();
                    if (ballSpeed < 400.0f)
                        kickoffWins++;
                }
                demos += player.eventState.demo ? 1 : 0;
                saves += player.eventState.save ? 1 : 0;
            }
        }
    }

    g_stats.episodes++;
    float n = (float)g_stats.episodes;
    g_stats.avgGoalsFor     = (g_stats.avgGoalsFor     * (n-1) + goalsFor)     / n;
    g_stats.avgGoalsAgainst = (g_stats.avgGoalsAgainst * (n-1) + goalsAgainst) / n;
    g_stats.avgTouches      = (g_stats.avgTouches      * (n-1) + touches)      / n;
    g_stats.avgKickoffWins  = (g_stats.avgKickoffWins  * (n-1) + kickoffWins)  / n;
    g_stats.avgDemos        = (g_stats.avgDemos        * (n-1) + demos)        / n;
    g_stats.avgSaves        = (g_stats.avgSaves        * (n-1) + saves)        / n;

    report.AddAvg("Bot/GoalsFor",     g_stats.avgGoalsFor);
    report.AddAvg("Bot/GoalsAgainst", g_stats.avgGoalsAgainst);
    report.AddAvg("Bot/Touches",      g_stats.avgTouches);
    report.AddAvg("Bot/KickoffWins",  g_stats.avgKickoffWins);
    report.AddAvg("Bot/Demos",        g_stats.avgDemos);
    report.AddAvg("Bot/Saves",        g_stats.avgSaves);
    report.AddAvg("Train/TotalSteps", (float)g_stats.totalSteps);
    report.AddAvg("Train/PhaseSteps", (float)g_stats.phaseSteps);
}

int main(int argc, char* argv[]) {
    std::string collisionMeshesPath = "collision_meshes";
    if (argc > 1)
        collisionMeshesPath = argv[1];
    RocketSim::Init(collisionMeshesPath);

    g_stats.currentPhase = DetectPhase();
    PhaseConfig& phase   = PHASES[g_stats.currentPhase];

    printf("\n");
    printf("============================================================\n");
    printf("  TALON V1 — V4 Reward Structure\n");
    printf("============================================================\n");
    printf("  Phase:       %s\n",       phase.name);
    printf("  Detected:    Phase %d\n", g_stats.currentPhase);
    printf("  Goal:        %s\n",       phase.description);
    printf("  Target:      %lld steps\n", phase.targetSteps);
    printf("  Gamma:       %.3f\n",     phase.gamma);
    printf("  Entropy:     %.2f\n",     phase.entropy);
    printf("  Network:     1024x4 LEAKY_RELU + LayerNorm\n");
    printf("  Mode:        1v1 only | Octane hitbox\n");
    printf("============================================================\n\n");

    LearnerConfig cfg = {};

    cfg.deviceType  = LearnerDeviceType::GPU_CUDA;
    cfg.tickSkip    = 8;
    cfg.actionDelay = cfg.tickSkip - 1;
    cfg.numGames    = 64;

    cfg.ppo.tsPerItr      = 50000;
    cfg.ppo.miniBatchSize = 10000;
    cfg.ppo.epochs        = 2;
    cfg.ppo.gaeGamma      = phase.gamma;
    cfg.ppo.gaeLambda     = 0.95f;
    cfg.ppo.entropyScale  = phase.entropy;
    cfg.ppo.policyLR      = 3e-4f;
    cfg.ppo.criticLR      = 3e-4f;

    cfg.ppo.sharedHead.layerSizes = { 1024, 1024, 1024, 1024 };
    cfg.ppo.policy.layerSizes     = { 1024, 1024, 1024, 1024 };
    cfg.ppo.critic.layerSizes     = { 1024, 1024, 1024, 1024 };

    cfg.ppo.sharedHead.activationType = ModelActivationType::LEAKY_RELU;
    cfg.ppo.policy.activationType     = ModelActivationType::LEAKY_RELU;
    cfg.ppo.critic.activationType     = ModelActivationType::LEAKY_RELU;

    cfg.ppo.sharedHead.addLayerNorm   = true;
    cfg.ppo.policy.addLayerNorm       = true;
    cfg.ppo.critic.addLayerNorm       = true;

    cfg.ppo.sharedHead.optimType      = ModelOptimType::ADAM;
    cfg.ppo.policy.optimType          = ModelOptimType::ADAM;
    cfg.ppo.critic.optimType          = ModelOptimType::ADAM;

    cfg.checkpointFolder = "checkpoints";
    cfg.tsPerSave        = 1000000;

    cfg.skillTracker.enabled = false;
    cfg.sendMetrics = false;

    bool renderMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--render") {
            renderMode = true;
            break;
        }
    }

    if (renderMode) {
        cfg.renderMode      = true;
        cfg.renderTimeScale = 1.0f;
    }

    cfg.randomSeed = 123;

    Learner* learner = new Learner(EnvCreateFunc, cfg, StepCallback);
    learner->Start();

    printf("\n");
    printf("============================================================\n");
    printf("  TRAINING COMPLETE\n");
    printf("============================================================\n");
    printf("  Total Steps:    %lld\n",    g_stats.totalSteps);
    printf("  Phase:          %d\n",      g_stats.currentPhase);
    printf("  Goals For:      %.2f/ep\n", g_stats.avgGoalsFor);
    printf("  Goals Against:  %.2f/ep\n", g_stats.avgGoalsAgainst);
    printf("  Touches:        %.2f/ep\n", g_stats.avgTouches);
    printf("  Kickoff Wins:   %.2f/ep\n", g_stats.avgKickoffWins);
    printf("  Demos:          %.2f/ep\n", g_stats.avgDemos);
    printf("  Saves:          %.2f/ep\n", g_stats.avgSaves);
    printf("============================================================\n\n");

    return EXIT_SUCCESS;
}
