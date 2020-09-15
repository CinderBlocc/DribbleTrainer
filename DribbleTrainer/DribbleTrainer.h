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
    
    //Reset
    struct ResetData
    {
        Vector location;
        std::chrono::steady_clock::time_point captureTime;
    };

    Vector ballResetVelocity;
    Vector ballResetLocation;

    bool IsBallHidden = false;

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
    void DrawFloorHeight(CanvasWrapper canvas, CarWrapper car);
    void DrawSafeZone(CanvasWrapper canvas, CameraWrapper camera, CarWrapper car, BallWrapper ball);
    void DrawLineUnderBall(CanvasWrapper canvas, CameraWrapper camera, CarWrapper car, BallWrapper ball);
    void DrawLaunchTimer(CanvasWrapper canvas, CameraWrapper camera, BallWrapper ball);

    //Reset
    void Reset();
    void GetResetValues(BallWrapper ball, CarWrapper car);
    Vector GetAcceleration(CarWrapper car);
    void TrimResetDataBuffer(std::vector<ResetData>& Buffer, float MaxBufferTime);
    Vector GetResetDataBufferAverage(const std::vector<ResetData>& Values);

    //Catch
    void PrepareToLaunch();
    void HoldBallInLaunchPosition(BallWrapper ball, CarWrapper car);
    Vector GetSafeHoldPosition(Vector InLocation);
    void Launch(int launchIndex);
    void GetNextLaunchDirection();
    Vector CalculateLaunchAngle(Vector start, Vector target, float speed);
};
