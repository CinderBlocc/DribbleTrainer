#pragma once
#pragma comment(lib, "BakkesMod.lib")
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "../_UP-TO-DATE-HELPER-FILES/RenderingTools.h"
#include <chrono>

class DribblesAndFlicks : public BakkesMod::Plugin::BakkesModPlugin
{
private:
    std::shared_ptr<float> angularReduction;
    std::shared_ptr<float> floorThreshold;
    std::shared_ptr<float> maxFlickDistance;
    std::shared_ptr<float> preparationTime;

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
    void Render(CanvasWrapper canvas);
    
    //Tick
    void Tick();

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
