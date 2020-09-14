#include "DribbleTrainer.h"
#include "bakkesmod\wrappers\includes.h"
#include <time.h>
#include <ctime>
#include <cstdlib>

//using namespace std;
using namespace std::chrono;

BAKKESMOD_PLUGIN(DribbleTrainer, "Freeplay training for dribbles, flicks, and catches", "1.0", PLUGINTYPE_FREEPLAY)

/*
    - Break up rendering function into multiple functions
*/



/*
TO-DO

    - Add catch practice
        - Add velocity prediction
        - LEFT OFF HERE: https://www.youtube.com/watch?v=RhUdv0jjfcE&list=PLqwfRVlgGdFAZeT_o-ASucXoFsajXMirw&index=12
        - Make sure during catch preparation time the ball doesn't go out of bounds in the corners
*/
/*
    - Make safe zone dynamic
        - "Machine learn" where zone should be to keep the ball balanced on the car
            - While the ball is within a certain distance of the car, gather all the variables.
                It's pointless to get that data when it isn't within touching range of the car at that frame.
            - VARIABLES
                - DECIDING FACTORS
                    - Car Accel Fwd
                    - Car Accel Right
                    - Car Vel Fwd
                    - Car Vel Right
                    - Car Angular Vel
                    - For angled surfaces - EXPLANATION BELOW
                        - CarUp dot with Z axis
                        - CarRight dot with Z axis
                        - CarFwd dot with Z axis
                - USEFUL FOR GAINING DATA
                    - Ball location relative to Car Fwd
                    - Ball location relative to Car Right
                    - Ball location Z
                    - Ball Vel relative to Car Fwd
                    - Ball Vel relative to Car Right
            - RESULT
                - (Vector) Ball spawn position relative to car forward
                - (Vector) Ball spawn position relative to car right
                - (float) Ball spawn position Z
                - (Vector) Ball spawn velocity
        - Only write values if ball has been in a stable relative position to the car
            - If the average dot of the ballLoc->carLoc vector with the 3 local car axes stays within a certain range then its safe to store those values
        - ANGLED SURFACES: Vector's dot with Z will determine how much of the position should be subtracted
            - i.e. When sitting stationary on an angled surface with the left side of the car lower than the right, the ball will need to sit on the upper right side of the car
            - When turning left around that same angle surface, the ball needs to be on the left side of the car due to angular velocity
            - Combine the two to subtract the right stationary value from the left turn, and you have the resulting position
*/

void DribbleTrainer::onLoad()
{
    srand(clock());

    //Notifiers
    cvarManager->registerNotifier(NOTIFIER_RESET,        [this](std::vector<std::string> params){Reset();}, "Resets ball to dribbling position", PERMISSION_ALL);
    cvarManager->registerNotifier(NOTIFIER_LAUNCH,       [this](std::vector<std::string> params){GetNextLaunchDirection();}, "Launch the ball toward your car for catch practice", PERMISSION_ALL);
    cvarManager->registerNotifier(NOTIFIER_REQUEST_MODE, [this](std::vector<std::string> params){RequestToggle(params);}, "Launch the ball toward your car for catch practice", PERMISSION_ALL);
    
    //Sliders
    angularReduction = std::make_shared<float>(0.f);
    floorThreshold   = std::make_shared<float>(0.f);
    maxFlickDistance = std::make_shared<float>(0.f);
    preparationTime  = std::make_shared<float>(0.f);
    cvarManager->registerCvar(CVAR_ANGULAR_REDUCTION, "0.5",          "How much the angular velocity should be reduced on reset", true, true, 0,   true, 1).bindTo(angularReduction);
    cvarManager->registerCvar(CVAR_BALL_FLOOR_HEIGHT, "2",            "How close the ball can get to the floor before resetting", true, true, 0,   true, 100000).bindTo(floorThreshold);
    cvarManager->registerCvar(CVAR_BALL_MAX_DISTANCE, "1250",         "Max distance the ball can move before resetting to car",   true, true, 300, true, 100000).bindTo(maxFlickDistance);
    cvarManager->registerCvar(CVAR_CATCH_PREPARATION, "2",            "Preparation time before launching", true, true, 0,  true, 20).bindTo(preparationTime);
    cvarManager->registerCvar(CVAR_CATCH_SPEED,       "(1500, 3500)", "Launch speed randomization range",  true, true, 0,  true, 5000);
    cvarManager->registerCvar(CVAR_CATCH_ANGLE,       "(15, 75)",     "Launch angle randomization range",  true, true, 10, true, 90);
    
    //Bools
    bEnableDribbleMode = std::make_shared<bool>(false);
    bEnableFlicksMode  = std::make_shared<bool>(false);
    bShowSafeZone      = std::make_shared<bool>(false);
    bShowFloorHeight   = std::make_shared<bool>(false);
    bLogFlickSpeed     = std::make_shared<bool>(false);
    cvarManager->registerCvar(CVAR_TOGGLE_DRIBBLE_MODE, "0", "Reset the ball if it falls below floor height").bindTo(bEnableDribbleMode);
    cvarManager->registerCvar(CVAR_TOGGLE_FLICKS_MODE,  "0", "Reset the ball if it goes farther than max flick distance").bindTo(bEnableFlicksMode);
    cvarManager->registerCvar(CVAR_SHOW_SAFE_ZONE,      "1", "Show where the ball should be to keep it balanced").bindTo(bShowSafeZone);
    cvarManager->registerCvar(CVAR_SHOW_FLOOR_HEIGHT,   "0", "Show where the reset threshold is for dribbling").bindTo(bShowFloorHeight);
    cvarManager->registerCvar(CVAR_LOG_FLICK_SPEED,     "1", "Save flick speed to bakkesmod.log so you can see them later").bindTo(bLogFlickSpeed);

    //Event hooks
    //gameWrapper->HookEvent("Function Engine.GameViewportClient.Tick", std::bind(&DribbleTrainer::Tick, this));
    gameWrapper->RegisterDrawable(bind(&DribbleTrainer::Render, this, std::placeholders::_1));


    //TEST JUNK
    testLocX = std::make_shared<float>(0.f);
    testLocY = std::make_shared<float>(0.f);
    testLocZ = std::make_shared<float>(0.f);
    cvarManager->registerCvar("Dribble_TestX", "250", "Test location X", true, true, 0, true, 10000).bindTo(testLocX);
    cvarManager->registerCvar("Dribble_TestY", "0",   "Test location Y", true, true, 0, true, 10000).bindTo(testLocY);
    cvarManager->registerCvar("Dribble_TestZ", "100", "Test location Z", true, true, 0, true, 10000).bindTo(testLocZ);
}
void DribbleTrainer::onUnload() {}

//Utility
bool DribbleTrainer::ShouldRun()
{
    if(!gameWrapper->IsInFreeplay())
        return false;

    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    if(server.IsNull())
        return false;

    BallWrapper ball = server.GetBall();
    CarWrapper car = gameWrapper->GetLocalCar();
    if(ball.IsNull() || car.IsNull())
        return false;

    return true;
}
void DribbleTrainer::RequestToggle(std::vector<std::string> params)
{
    if(params.size() != 2 || !gameWrapper->IsInFreeplay())
        return;

    bool newval = false;
    if(params.at(1) == "dribble")
    {
        if(!(*bEnableDribbleMode))
            newval = true;
        cvarManager->getCvar(CVAR_TOGGLE_DRIBBLE_MODE).setValue(newval);
    }
    if(params.at(1) == "flick")
    {
        if(!(*bEnableFlicksMode))
            newval = true;
        cvarManager->getCvar(CVAR_TOGGLE_FLICKS_MODE).setValue(newval);
    }
}

//Reset
void DribbleTrainer::Reset()
{
    if(!ShouldRun()) return;
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    BallWrapper ball = server.GetBall();
    CarWrapper car = gameWrapper->GetLocalCar();
    if(abs(car.GetLocation().Y) >= 5120) return;//dont move ball to car if car is in goal
    
    Quat carQuat = RotatorToQuat(car.GetRotation());
    RT::Matrix3 carMat = RT::Matrix3(carQuat);
    
    Vector ballAngular = ball.GetAngularVelocity();
    ballAngular = ballAngular - ballAngular * *angularReduction;

    //ball.SetLocation(carLocation + spawnOffset + forwardOffset);
    //ball.SetVelocity(carVelocity + velocityAdjust);
    Vector positionOffset = Vector{0,0,0} + (carMat.forward * ballResetPosFwd) + (carMat.right * ballResetPosRight);
    positionOffset.Z = ballResetPosZ;
    ball.SetLocation(car.GetLocation() + positionOffset);
    ball.SetVelocity(car.GetVelocity() + ballResetVelocity);
    ball.SetAngularVelocity(ballAngular, false);
}

void DribbleTrainer::GetResetValues()
{
    if(!ShouldRun()) return;
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    BallWrapper ball = server.GetBall();
    CarWrapper car = gameWrapper->GetLocalCar();

    Quat carQuat = RotatorToQuat(car.GetRotation());
    RT::Matrix3 carMat = RT::Matrix3(carQuat);

    Vector carVelocity = car.GetVelocity();
    Vector carAngular = car.GetAngularVelocity();

    float speedPerc = carVelocity.magnitude() / 2300;
    float angularPerc = abs(carAngular.Z) / 5.5f;
    Vector rightOffset = carMat.right * (300 * angularPerc * speedPerc);
    Vector forwardOffset = carMat.forward * accelDotFwd * 4 * speedPerc;
    Vector spawnOffset = {0,0,150};
    Vector velocityAdjust = {0,0,0};
    
    //ANGULAR VELOCITY
    if(car.IsOnGround())
    {
        if(carAngular.Z > 0.f)
        {
            //right turn
            spawnOffset = spawnOffset + rightOffset;
            spawnOffset.Z *= (1 - angularPerc);
            forwardOffset = forwardOffset - forwardOffset * angularPerc;
            velocityAdjust = velocityAdjust - (carMat.right * velocityRight);//carRight velocity is negative from 0 to -215 without powerslide
        }
        else if(carAngular.Z < 0.f)
        {
            //left turn
            spawnOffset = spawnOffset - rightOffset;
            spawnOffset.Z *= (1 - angularPerc);
            forwardOffset = forwardOffset - forwardOffset * angularPerc;
            velocityAdjust = velocityAdjust - (carMat.right * velocityRight);//carRight velocity is positive from 0 to 215 without powerslide
        }
    }

    forwardOffset = forwardOffset - forwardOffset * speedPerc;
    if(spawnOffset.Z < ball.GetRadius())
        spawnOffset.Z = ball.GetRadius();

    //Get values from data - take average over time
    Vector finalOffset = spawnOffset + forwardOffset;
    
    ResetData fwdVal;
    fwdVal.captureTime = steady_clock::now();
    fwdVal.value = Vector::dot(finalOffset, carMat.forward);
    ballResetFwdVals.push_back(fwdVal);
    
    ResetData rightVal;
    rightVal.captureTime = steady_clock::now();
    rightVal.value = Vector::dot(finalOffset, carMat.right);
    ballResetRightVals.push_back(rightVal);

    bool fwdValsAreOK = false;
    bool rightValsAreOK = false;

    float valueBufferTime = .5f;//Average over .25 seconds
    while(!fwdValsAreOK || !rightValsAreOK)
    {
        //Erase values from vector if they are older than the buffer time
        if(!fwdValsAreOK)
        {
            steady_clock::time_point lastTime = ballResetFwdVals[ballResetFwdVals.size()-1].captureTime;
            steady_clock::time_point firstTime = ballResetFwdVals[0].captureTime;
            duration<float> fwdValsDuration = duration_cast<duration<float>>(lastTime - firstTime);
            fwdDurationSeconds = fwdValsDuration.count();
            if(fwdDurationSeconds > valueBufferTime)
                ballResetFwdVals.erase(ballResetFwdVals.begin());
            else
                fwdValsAreOK = true;
        }
        if(!rightValsAreOK)
        {
            steady_clock::time_point lastTime = ballResetRightVals[ballResetRightVals.size()-1].captureTime;
            steady_clock::time_point firstTime = ballResetRightVals[0].captureTime;
            duration<float> rightValsDuration = duration_cast<duration<float>>(lastTime - firstTime);
            rightDurationSeconds = rightValsDuration.count();
            if(rightDurationSeconds > valueBufferTime)
                ballResetRightVals.erase(ballResetRightVals.begin());
            else
                rightValsAreOK = true;
        }
    }

    //Get average of values
    float fwdAverage = 0;
    float rightAverage = 0;
    for(const auto& ballResetFwdVal : ballResetFwdVals)
        fwdAverage += ballResetFwdVal.value;
    for(const auto& ballResetRightVal : ballResetRightVals)
        rightAverage += ballResetRightVal.value;

    fwdAverage /= ballResetFwdVals.size();
    rightAverage /= ballResetRightVals.size();

    ballResetPosFwd = fwdAverage;
    ballResetPosRight = rightAverage;
    ballResetPosZ = spawnOffset.Z;
    ballResetVelocity = velocityAdjust;
}

//Tick
void DribbleTrainer::Tick()
{
    //Called inside the render function

    if(!ShouldRun()) { return; }
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    BallWrapper ball = server.GetBall();
    CarWrapper car = gameWrapper->GetLocalCar();

    //GET RESET VALUES
    Vector speedChange = car.GetVelocity() - previousVelocity;
    duration<float> timeChange = duration_cast<duration<float>>(steady_clock::now() - previousTime);
    carAcceleration = (speedChange * 0.036f) * (1 / timeChange.count());
    
    Quat carQuat = RotatorToQuat(car.GetRotation());
    RT::Matrix3 carMat = RT::Matrix3(carQuat);
    accelDotFwd = Vector::dot(carMat.forward, carAcceleration);//range -150 to 150
    accelDotRight = Vector::dot(carMat.right, carAcceleration);//range -250 to 250
    velocityForward = Vector::dot(car.GetVelocity(),carMat.forward);
    velocityRight = Vector::dot(car.GetVelocity(),carMat.right);

    previousVelocity = car.GetVelocity();
    previousTime = steady_clock::now();

    GetResetValues();

    //MODES
    //Dribble Reset
    if(*bEnableDribbleMode)
    {
        float ballHeight = ball.GetLocation().Z - (ball.GetRadius() + *floorThreshold);
        if(ballHeight <= 0)
            Reset();
    }

    //Flick Reset
    if(*bEnableFlicksMode)
    {
        Vector ballToCar = ball.GetLocation() - car.GetLocation();
        float flickDistance = ballToCar.magnitude();
        if(flickDistance > *maxFlickDistance)
        {
            float ballSpeed = ball.GetVelocity().magnitude();
            ballSpeed *= 0.036f;//cms to kph
            if(abs(ball.GetLocation().Y) < 5120 && abs(car.GetLocation().Y) < 5120)
            {
                if(*bLogFlickSpeed)
                {
                    gameWrapper->LogToChatbox(std::to_string((int)ballSpeed) + " KPH", "Flick Speed");
                    cvarManager->log("Flick Speed: " + std::to_string((int)ballSpeed) + " KPH");
                }
            }
            Reset();
        }
    }

    //Catch
    if(preparingToLaunch)
        HoldBallInLaunchPosition();
}

//Catch
void DribbleTrainer::PrepareToLaunch()
{
    preparingToLaunch = true;
    preparationStartTime = clock();
    launchNum++;
    gameWrapper->SetTimeout(std::bind(&DribbleTrainer::Launch, this, launchNum), *preparationTime);
}
void DribbleTrainer::GetNextLaunchDirection()
{
    if(!ShouldRun()) return;

    CarWrapper car = gameWrapper->GetLocalCar();
    if(car.IsNull()) return;

    RT::Matrix3 launchDirectionMatrix;

    //Rotate around UP axis
    float UpRotAmount = ((float)rand()/RAND_MAX) * (2.f * CONST_PI_F);
    Quat upRotQuat = RT::AngleAxisRotation(UpRotAmount, launchDirectionMatrix.up);
    launchDirectionMatrix.RotateWithQuat(upRotQuat);// = RT::RotateMatrixWithQuat(launchDirectionMatrix, upRotQuat);

    //Rotate around right axis
    float launchAngle = cvarManager->getCvar(CVAR_CATCH_ANGLE).getFloatValue();
    float RightRotAmount = launchAngle * -(CONST_PI_F / 180);
    Quat rightRotQuat = RT::AngleAxisRotation(RightRotAmount, launchDirectionMatrix.right);
    launchDirectionMatrix.RotateWithQuat(rightRotQuat);// = RT::RotateMatrixWithQuat(launchDirectionMatrix, rightRotQuat);
    
    Vector spawnDirection = launchDirectionMatrix.forward;
    spawnDirection.normalize();

    nextLaunch.launchDirection = spawnDirection;

    float launchSpeed = cvarManager->getCvar(CVAR_CATCH_SPEED).getFloatValue();
    nextLaunch.launchMagnitude = launchSpeed / 5000;

    PrepareToLaunch();
}
void DribbleTrainer::HoldBallInLaunchPosition()
{
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    BallWrapper ball = server.GetBall();
    CarWrapper car = gameWrapper->GetLocalCar();

    Vector spawnLocation = car.GetLocation() + nextLaunch.launchDirection * (*maxFlickDistance - 150.f);
    if(spawnLocation.X < -4000)
        spawnLocation.X = -4000;
    if(spawnLocation.X > 4000)
        spawnLocation.X = 4000;
    if(spawnLocation.Y < -5000)
        spawnLocation.Y = -5000;
    if(spawnLocation.Y > 5000)
        spawnLocation.Y = 5000;
    if(spawnLocation.Z < 100)
        spawnLocation.Z = 100;
    if(spawnLocation.Z > 1900)
        spawnLocation.Z = 1900;

    ball.SetVelocity(Vector{0,0,0});
    ball.SetLocation(spawnLocation);
}
void DribbleTrainer::Launch(int launchIndex)
{
    //Launch the ball at roughly the speed of the car
    //Use redirectplugin code for this
    //https://github.com/bakkesmodorg/BakkesMod2-Plugins/blob/master/RedirectPlugin/redirectplugin.cpp

    //Calculate launch velocity to make it catchable
    //BALL VELOCITY TO MEET CAR

    /*
    velMultiplied = playerVelocity * Vector(predictMultiplierX, predictMultiplierY, 1);
    offsetVec = offsetVec + velMultiplied;
    Vector shotData = training.GenerateShot(ballPosition, playerPosition + offsetVec, ballSpeed);
    if (onGround)
        shotData.Z = 0;
    training.GetBall().SetVelocity(shotData);
    */

    /*
    Vector ServerWrapper::GenerateShot(Vector startPos, Vector destination, float speed)
    {
        //speed = range from 0 to 2000
        speed = 2000 - speed;
        Vector shotVelocity = Vector(0);

        Vector destination = car.GetLocation();
        Vector startPos = ball.GetLocation();
        Vector desiredLocation = destination - startPos;
        float size = sqrt(desiredLocation.X * desiredLocation.X + desiredLocation.Y * desiredLocation.Y); //2d vector size
        Vector desiredLocationNormalized = desiredLocation;
        desiredLocationNormalized.normalize();

        Vector centerLocation = startPos + (desiredLocationNormalized * ((size / 2.f)* (1500.f / speed)));
        Rotator desiredRotation = VectorToRotator(desiredLocation);

        Vector vec = { 0, 0, speed };
        Rotator rot = { desiredRotation.Pitch, desiredRotation.Yaw, desiredRotation.Roll };

        Vector aaa = RotateVectorWithQuat(vec, RotatorToQuat(rot));
        centerLocation = centerLocation + aaa;
        Vector directionToCenterUp = centerLocation - startPos;
        Vector directionToCenterUpNorm = directionToCenterUp;
        directionToCenterUpNorm.normalize();
        Vector finalCalc = directionToCenterUpNorm * (directionToCenterUp.magnitude() * 0.680f);

        return Vector(shotVelocity);
    }
    */

    if(launchIndex != launchNum) return;

    preparingToLaunch = false;

    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    BallWrapper ball = server.GetBall();
    CarWrapper car = gameWrapper->GetLocalCar();

    //this is what needs to be calculated with prediction plugin code
    Vector launchDirection = car.GetLocation() - ball.GetLocation();
    launchDirection.normalize();
    ball.SetVelocity(launchDirection * (5000 * nextLaunch.launchMagnitude) + car.GetVelocity());

    /*int randOffsetScale = 50;
    float randX = rand() % randOffsetScale;
    float randY = rand() % randOffsetScale;
    Vector randOffset = {randX, randY, 0};
    Vector launchAngle = CalculateLaunchAngle(ball.GetLocation(), car.GetLocation() + randOffset, 5000 * nextLaunch.launchMagnitude);*/
}
Vector DribbleTrainer::CalculateLaunchAngle(Vector start, Vector target, float v)
{
    //Convert 3D trajectory into 2D equation, just solving for angle to reach X coordinate
    //https://en.wikipedia.org/wiki/Projectile_motion --> Angle theta required to hit coordinate (x,y)
    //y == car.Z - ball.Z (puts ball Z at 0)

    float y = -start.Z + 80;//target the top of the car
    target.Z = start.Z = 0;
    float x = (target - start).magnitude();

    float tanTheta;
    float g = -9.8f;

    bool addIt = true;
    tanTheta = v*v;
    if(addIt)
        tanTheta += sqrtf((v*v*v*v) - g * ((g*x*x) + (2*y*v*v)));
    else
        tanTheta -= sqrtf((v*v*v*v) - g * ((g*x*x) + (2*y*v*v)));
    tanTheta /= g*x;


    return atanf(tanTheta);
}
