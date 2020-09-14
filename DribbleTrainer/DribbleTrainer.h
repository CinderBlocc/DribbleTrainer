#pragma once
#pragma comment(lib, "PluginSDK.lib")
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "RenderingTools.h"
#include <chrono>

#define NOTIFIER_RESET            "DribbleReset"
#define NOTIFIER_LAUNCH           "DribbleLaunch"
#define NOTIFIER_REQUEST_MODE     "DribbleRequestModeToggle"
#define CVAR_ANGULAR_REDUCTION    "Dribble_AngularReduction"
#define CVAR_BALL_FLOOR_HEIGHT    "Dribble_BallFloorHeight"
#define CVAR_BALL_MAX_DISTANCE    "Dribble_BallMaxDistance"
#define CVAR_CATCH_PREPARATION    "Dribble_CatchPreparation"
#define CVAR_CATCH_SPEED          "Dribble_CatchSpeed"
#define CVAR_CATCH_ANGLE          "Dribble_CatchAngle"
#define CVAR_TOGGLE_DRIBBLE_MODE  "Dribble_ToggleDribbleMode"
#define CVAR_TOGGLE_FLICKS_MODE   "Dribble_ToggleFlicksMode"
#define CVAR_SHOW_SAFE_ZONE       "Dribble_ShowSafeZone"
#define CVAR_SHOW_FLOOR_HEIGHT    "Dribble_ShowFloorHeight"
#define CVAR_LOG_FLICK_SPEED      "Dribble_LogFlickSpeed"

class DribbleTrainer : public BakkesMod::Plugin::BakkesModPlugin
{
    RT::RenderingAssistant RA;

    std::shared_ptr<float> angularReduction;
    std::shared_ptr<float> floorThreshold;
    std::shared_ptr<float> maxFlickDistance;
    std::shared_ptr<float> preparationTime;

    std::shared_ptr<bool> bEnableDribbleMode;
    std::shared_ptr<bool> bEnableFlicksMode;
    std::shared_ptr<bool> bShowSafeZone;
    std::shared_ptr<bool> bShowFloorHeight;
    std::shared_ptr<bool> bLogFlickSpeed;

    //TEST JUNK
    std::shared_ptr<float> testLocX;
    std::shared_ptr<float> testLocY;
    std::shared_ptr<float> testLocZ;
    
    //Reset
    struct ResetData
    {
        float value;
        std::chrono::steady_clock::time_point captureTime;
    };
    std::vector<ResetData> ballResetFwdVals;
    std::vector<ResetData> ballResetRightVals;
    float ballResetPosFwd;
    float ballResetPosRight;
    float ballResetPosZ;
    Vector ballResetVelocity;

    float fwdDurationSeconds;
    float rightDurationSeconds;

    //Catch
    bool preparingToLaunch = false;
    int launchNum = 0;
    clock_t preparationStartTime;
    struct CatchData
    {
        Vector launchDirection;
        float launchMagnitude; //range 0-1
    };
    CatchData nextLaunch;

    //Acceleration
    Vector previousVelocity = {0,0,0};
    std::chrono::steady_clock::time_point previousTime;
    Vector carAcceleration = {0,0,0};
    float accelDotFwd = 0.f;
    float accelDotRight = 0.f;
    float highestDotFwd = 0.f;
    float highestDotRight = 0.f;
    float velocityForward = 0.f;
    float velocityRight = 0.f;

public:
    void onLoad() override;
    void onUnload() override;

    //Utility
    bool ShouldRun();
    void RequestToggle(std::vector<std::string> params);
    
    //Render and Tick
    void Render(CanvasWrapper canvas);
    void Tick();
    void DrawModesStrings(CanvasWrapper canvas);
    void DrawFloorHeight(CanvasWrapper canvas);
    void DrawSafeZone(CanvasWrapper canvas);
    void DrawLaunchTimer(CanvasWrapper canvas);

    //Reset
    void Reset();
    void GetResetValues();

    //Catch
    void PrepareToLaunch();
    void HoldBallInLaunchPosition();
    void Launch(int launchIndex);
    void GetNextLaunchDirection();
    Vector CalculateLaunchAngle(Vector start, Vector target, float speed);
};
