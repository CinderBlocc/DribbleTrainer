#include "DribblesAndFlicks.h"
#include "bakkesmod\wrappers\includes.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>
#include <ctime>
#include <cstdlib>

//using namespace std;
using namespace std::chrono;

BAKKESMOD_PLUGIN(DribblesAndFlicks, "Freeplay training for dribbles, flicks, and catches", "1.0", PLUGINTYPE_FREEPLAY)

/*
TO-DO

    - Add catch practice
        - Add velocity prediction
        - LEFT OFF HERE: https://www.youtube.com/watch?v=RhUdv0jjfcE&list=PLqwfRVlgGdFAZeT_o-ASucXoFsajXMirw&index=12

    - Align safe zone circle's rotation to car so it doesnt spin while turning
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

std::string catchSpeedName = "Dribble_CatchSpeed",
            catchAngleName = "Dribble_CatchAngle",
            toggleDribbleModeName = "Dribble_ToggleDribbleMode",
            toggleFlicksModeName = "Dribble_ToggleFlicksMode",
            toggleSafeZoneName = "Dribble_ShowSafeZone",
            toggleFloorHeightName = "Dribble_ShowFloorHeight",
            toggleLogFlickSpeedName = "Dribble_LogFlickSpeed";
void DribblesAndFlicks::onLoad()
{
    srand(clock());

    //Notifiers
    cvarManager->registerNotifier("DribbleReset", [this](std::vector<std::string> params){Reset();}, "Resets ball to dribbling position", PERMISSION_ALL);
    cvarManager->registerNotifier("DribbleLaunch", [this](std::vector<std::string> params){GetNextLaunchDirection();}, "Launch the ball toward your car for catch practice", PERMISSION_ALL);
    cvarManager->registerNotifier("DribbleRequestModeToggle", [this](std::vector<std::string> params){RequestToggle(params);}, "Launch the ball toward your car for catch practice", PERMISSION_ALL);
    
    //Sliders
    angularReduction = std::make_shared<float>(0.f);
    floorThreshold = std::make_shared<float>(0.f);
    maxFlickDistance = std::make_shared<float>(0.f);
    preparationTime = std::make_shared<float>(0.f);
    cvarManager->registerCvar("Dribble_AngularReduction", "0.5", "How much the angular velocity should be reduced on reset", true, true, 0, true, 1).bindTo(angularReduction);
    cvarManager->registerCvar("Dribble_BallFloorHeight", "2", "How close the ball can get to the floor before resetting", true, true, 0, true, 100000).bindTo(floorThreshold);
    cvarManager->registerCvar("Dribble_BallMaxDistance", "1250", "Max distance the ball can move before resetting to car", true, true, 300, true, 100000).bindTo(maxFlickDistance);
    cvarManager->registerCvar("Dribble_CatchPreparation", "2", "Preparation time before launching", true, true, 0, true, 20).bindTo(preparationTime);
    cvarManager->registerCvar(catchSpeedName, "(1500, 3500)", "Launch speed randomization range", true, true, 0, true, 5000);
    cvarManager->registerCvar(catchAngleName, "(15, 75)", "Launch angle randomization range", true, true, 10, true, 90);
    
    //Bools
    cvarManager->registerCvar(toggleDribbleModeName, "0", "Reset the ball if it falls below floor height");
    cvarManager->registerCvar(toggleFlicksModeName, "0", "Reset the ball if it goes farther than max flick distance");
    cvarManager->registerCvar(toggleSafeZoneName, "1", "Show where the ball should be to keep it balanced");
    cvarManager->registerCvar(toggleFloorHeightName, "0", "Show where the reset threshold is for dribbling");
    cvarManager->registerCvar(toggleLogFlickSpeedName, "1", "Save flick speed to bakkesmod.log so you can see them later");

    //Event hooks
    gameWrapper->HookEvent("Function Engine.GameViewportClient.Tick", std::bind(&DribblesAndFlicks::Tick, this));
    gameWrapper->RegisterDrawable(bind(&DribblesAndFlicks::Render, this, std::placeholders::_1));


    //TEST JUNK
    testLocX = std::make_shared<float>(0.f);
    cvarManager->registerCvar("Dribble_TestX", "250", "Test location X", true, true, 0, true, 10000).bindTo(testLocX);
    testLocY = std::make_shared<float>(0.f);
    cvarManager->registerCvar("Dribble_TestY", "0", "Test location Y", true, true, 0, true, 10000).bindTo(testLocY);
    testLocZ = std::make_shared<float>(0.f);
    cvarManager->registerCvar("Dribble_TestZ", "100", "Test location Z", true, true, 0, true, 10000).bindTo(testLocZ);
}
void DribblesAndFlicks::onUnload() {}

//Utility
bool DribblesAndFlicks::ShouldRun()
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
void DribblesAndFlicks::RequestToggle(std::vector<std::string> params)
{
    if(params.size() != 2 || !gameWrapper->IsInFreeplay())
        return;

    std::string newval = "0";
    if(params.at(1) == "dribble")
    {
        if(!cvarManager->getCvar(toggleDribbleModeName).getBoolValue())
            newval = "1";
        cvarManager->executeCommand(toggleDribbleModeName + " " + newval);
    }
    if(params.at(1) == "flick")
    {
        if(!cvarManager->getCvar(toggleFlicksModeName).getBoolValue())
            newval = "1";
        cvarManager->executeCommand(toggleFlicksModeName + " " + newval);
    }
}
void DribblesAndFlicks::Render(CanvasWrapper canvas)
{
    if(!gameWrapper->IsInFreeplay())
        return;

    CameraWrapper camera = gameWrapper->GetCamera();

    //Nullchecks
    bool haveCamera = !camera.IsNull();
    bool haveCar = !gameWrapper->GetLocalCar().IsNull();
    bool haveServer = !gameWrapper->GetGameEventAsServer().IsNull();
    bool haveBall = false;
    if(haveServer)
    {
        if(!gameWrapper->GetGameEventAsServer().GetBall().IsNull())
            haveBall = true;
    }

    if(!haveCamera)
    {
        return;
    }


    //Create frustum - NEED TO CHECK IF haveCamera IS TRUE BEFORE USING FRUSTUM
    RA.frustum = RT::Frustum(canvas, camera);
    //RT::Frustum frustum;
    //if(haveCamera)
    //{
    //    CameraWrapper camera = gameWrapper->GetCamera();
    //    frustum = RT::CreateFrustum(canvas, camera);
    //}

    //Show which modes are enabled
    bool dribbles = cvarManager->getCvar(toggleDribbleModeName).getBoolValue();
    bool flicks = cvarManager->getCvar(toggleFlicksModeName).getBoolValue();
    if(dribbles || flicks)
    {
        std::string dribblemode = "OFF", flickmode = "OFF", catchmode = "OFF";
        if(dribbles)
            dribblemode = "ON";
        if(flicks)
            flickmode = "ON";

        int width = canvas.GetSize().X;
        int height = canvas.GetSize().Y;

        int midline = width/2;

        canvas.SetColor(255,255,255,255);
        canvas.SetPosition(Vector2{midline-90,height-30});
        canvas.DrawString("Dribble: " + dribblemode);

        canvas.SetPosition(Vector2{midline+30,height-30});
        canvas.DrawString("Flick: " + flickmode);
    }

    //Show floor height
    if(haveCamera && haveCar && cvarManager->getCvar(toggleFloorHeightName).getBoolValue())
    {
        CarWrapper car = gameWrapper->GetLocalCar();
        Vector drawLocation = car.GetLocation();
        drawLocation.Z = *floorThreshold;

        RT::Circle floorHeightCircle;
        floorHeightCircle.orientation = RT::RotatorToQuat(Rotator{0,0,0});
        floorHeightCircle.steps = 24;

        int circles = 4;
        for(int i=0; i<circles; i++)
        {
            canvas.SetColor(150,150,255,255);
            floorHeightCircle.lineThickness = 3;
            floorHeightCircle.location = drawLocation;
            floorHeightCircle.radius = 100 - 8 * i;
            DrawCircle(canvas, frustum, floorHeightCircle);

            canvas.SetColor(150,150,255,(char)(255/((float)(i+1)/2)));
            floorHeightCircle.lineThickness = 3/(float)(i+1);
            floorHeightCircle.radius = 100;
            float heightSegs = *floorThreshold / (circles-1);
            Vector loc = floorHeightCircle.location;
            loc.Z = drawLocation.Z - heightSegs * i;
            floorHeightCircle.location = loc;
            if(i != 0)
                DrawCircle(canvas, frustum, floorHeightCircle);
        }
    }

    //Show balance safe zone
    if(haveCamera && haveBall && haveCar && cvarManager->getCvar(toggleSafeZoneName).getBoolValue())
    {
        CameraWrapper camera = gameWrapper->GetCamera();
        ServerWrapper server = gameWrapper->GetGameEventAsServer();
        BallWrapper ball = server.GetBall();
        CarWrapper car = gameWrapper->GetLocalCar();
        
        Vector carLocation = car.GetLocation();
        Vector ballLocation = ball.GetLocation();
        Vector cameraLocation = camera.GetLocation();

        float ballDistanceMagnitude = (Vector{ballLocation.X, ballLocation.Y, 0} - Vector{carLocation.X, carLocation.Y, 0}).magnitude();
        if(ballDistanceMagnitude < 300 && carLocation.Z <= ballLocation.Z)
        {
            Quat carQuat = RT::RotatorToQuat(car.GetRotation());
            RT::Matrix3 carMat = RT::QuatToMatrix(carQuat);

            //Crosshair
            RT::SetColor(canvas, "green");
            canvas.DrawLine(canvas.Project((carMat.forward * 5) + carLocation), canvas.Project((carMat.forward * 50) + carLocation), 2);//forward
            canvas.DrawLine(canvas.Project((carMat.right * 5) + carLocation), canvas.Project((carMat.right * 50) + carLocation), 2);//right
            canvas.DrawLine(canvas.Project((carMat.forward * -5) + carLocation), canvas.Project((carMat.forward * -50) + carLocation), 2);//back
            canvas.DrawLine(canvas.Project((carMat.right * -5) + carLocation), canvas.Project((carMat.right * -50) + carLocation), 2);//left

            //Good range
            Vector safeZoneOffset = {0,0,0};
            safeZoneOffset = safeZoneOffset + (carMat.forward * ballResetPosFwd);
            safeZoneOffset = safeZoneOffset + (carMat.right * ballResetPosRight);

            RT::Circle goodRangeCircle;
            goodRangeCircle.radius = 20;
            goodRangeCircle.location = carLocation + safeZoneOffset;
            goodRangeCircle.lineThickness = 2;
            DrawCircle(canvas, frustum, goodRangeCircle);

            //Car stable circle
            RT::SetColor(canvas, "green", 100);
            goodRangeCircle.location = carLocation;
            goodRangeCircle.lineThickness = 3;
            goodRangeCircle.orientation = carQuat;
            DrawCircle(canvas, frustum, goodRangeCircle);

            //Draw ball's location relative to car
            Vector ballDot = {ballLocation.X, ballLocation.Y, carLocation.Z};
            if(IsInFrustum(frustum, ballDot, 5.f))
            {
                //Draw crosshair under ball
                RT::Matrix3 ZRotMatrix;
                ZRotMatrix = RT::SingleAxisAlignment(ZRotMatrix, carMat.forward, RT::AXIS_FORWARD, 1);
                Quat ZRot = MatrixToQuat(ZRotMatrix);
                ZRot = RT::NormalizeQuat(ZRot);

                float lineLength = 20;
                Vector2 line1Start = canvas.Project(RT::RotateVectorWithQuat(Vector{-lineLength,0,0}, ZRot) + ballDot);
                Vector2 line1End = canvas.Project(RT::RotateVectorWithQuat(Vector{lineLength,0,0}, ZRot) + ballDot);
                Vector2 line2Start = canvas.Project(RT::RotateVectorWithQuat(Vector{0,-lineLength,0}, ZRot) + ballDot);
                Vector2 line2End = canvas.Project(RT::RotateVectorWithQuat(Vector{0,lineLength,0}, ZRot) + ballDot);
                RT::SetColor(canvas, "white");
                canvas.DrawLine(line1Start, line1End);
                canvas.DrawLine(line2Start, line2End);

                RT::Circle ballLocationCircle;
                ballLocationCircle.radius = 4;
                ballLocationCircle.location = ballDot;
                ballLocationCircle.steps = 8;
                DrawCircle(canvas, frustum, ballLocationCircle);

                //Calculate where the line should terminate just below the ball
                Vector ballBottom = {ballLocation.X, ballLocation.Y, ballLocation.Z - ball.GetRadius()};
                if(ballBottom.Z > ballDot.Z)
                {
                    if(IsInFrustum(frustum, ballBottom, 0.f))
                    {
                        float ballRadius = ball.GetRadius();
                        bool isBallBottomObscured = RT::IsObscuredByObject(ballBottom, cameraLocation, RT::Sphere{ballLocation, ballRadius-.25f});
                        bool isBallDotObscured = RT::IsObscuredByObject(ballDot, cameraLocation, RT::Sphere{ballLocation, ballRadius});
                        if(!isBallDotObscured)
                        {
                            if(isBallBottomObscured)
                            {
                                //Get tangent point along ball sphere to create the line for the line-plane intersection
                                Vector cameraToBallCenter = ballLocation - cameraLocation;
                                Vector cameraToBallBottom = ballBottom - cameraLocation;
                                Vector projection = RT::VectorProjection(cameraToBallCenter, cameraToBallBottom) + cameraLocation;
                                Vector projectionToBall = projection - ballLocation;
                                projectionToBall.normalize();
                                Vector tangentVert = (projectionToBall * (ballRadius-2)) + ballLocation;
                                RT::Line line = RT::GetLineFromPoints(cameraLocation, tangentVert);

                                //Create a plane at ball's location with normal parallel to ground
                                Vector planeVec1 = cameraLocation;
                                Vector planeVec2 = ballLocation;
                                planeVec1.Z = planeVec2.Z;
                                Vector planeNormal = planeVec1 - planeVec2;
                                RT::Plane plane = RT::GetPlaneFromLocationAndDirection(ballLocation, planeNormal);

                                //Calculate intersection point on plane to assign new Z position to ballBottom
                                Vector newBallBottom = RT::LinePlaneIntersectionPoint(line, plane);
                                ballBottom.Z = newBallBottom.Z;
                            }
                            canvas.DrawLine(canvas.Project(ballBottom), canvas.Project(ballDot));
                        }
                    }
                }
            }
        }
    }

    //Show launch countdown circle around ball
    if(haveCamera && haveBall && preparingToLaunch)
    {
        CameraWrapper camera = gameWrapper->GetCamera();
        ServerWrapper server = gameWrapper->GetGameEventAsServer();
        BallWrapper ball = server.GetBall();

        float piePercentage = 1 - (clock() - preparationStartTime)/(*preparationTime * CLOCKS_PER_SEC);
        RT::Matrix3 directionMatrix = RT::LookAt(ball.GetLocation(), camera.GetLocation(), RT::AXIS_UP, M_PI * -piePercentage + M_PI);

        RT::Circle circleAroundBall;
        circleAroundBall.radius = ball.GetRadius();
        circleAroundBall.location = ball.GetLocation();
        circleAroundBall.orientation = RT::MatrixToQuat(directionMatrix);
        circleAroundBall.lineThickness = 4;
        circleAroundBall.piePercentage = piePercentage;
        
        int minSteps = 8;
        int maxSteps = 40;
        float distancePerc = GetVisualDistance(canvas, frustum, camera, ball.GetLocation());
        int calcSteps = (int)(maxSteps * distancePerc);
        if(calcSteps < minSteps)
            calcSteps = minSteps;
        circleAroundBall.steps = calcSteps;

        LinearColor launchColor = RT::GetPercentageColor(1 - nextLaunch.launchMagnitude);
        RT::SetColor(canvas, launchColor);
        RT::DrawCircle(canvas, frustum, circleAroundBall);
    }




    
    //MATH HELP
    std::vector<std::string> debugString;
    
    /*
    if(abs(accelDotFwd) > highestDotFwd)
        highestDotFwd = abs(accelDotFwd);
    if(abs(accelDotRight) > highestDotRight)
        highestDotRight = abs(accelDotRight);
    debugString.push_back("Car Angular Velocity: " + vector_to_string(car.GetAngularVelocity()));
    debugString.push_back("Car Acceleration: " + vector_to_string(carAcceleration));
    debugString.push_back("Accel Dot Fwd: " + std::to_string(accelDotFwd));
    debugString.push_back("Accel Dot Right: " + std::to_string(accelDotRight));
    debugString.push_back("Highest Dot FWD: " + std::to_string(highestDotFwd));
    debugString.push_back("Highest Dot RIGHT: " +std::to_string(highestDotRight));
    debugString.push_back("Car Velocity Forward: " +std::to_string(velocityForward));
    debugString.push_back("Car Velocity Right: " +std::to_string(velocityRight));
    */
    
    /*
    debugString.push_back("Fwd reset vals: " + to_string(ballResetFwdVals.size()));
    debugString.push_back("Fwd buffer time: " + to_string(fwdDurationSeconds));
    debugString.push_back("Right reset vals: " + to_string(ballResetRightVals.size()));
    debugString.push_back("Right buffer time: " + to_string(rightDurationSeconds));
    */

    RT::DrawDebugStrings(canvas, debugString, true);
}

//Reset
void DribblesAndFlicks::Reset()
{
    if(!ShouldRun()) return;
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    BallWrapper ball = server.GetBall();
    CarWrapper car = gameWrapper->GetLocalCar();
    if(abs(car.GetLocation().Y) >= 5120) return;//dont move ball to car if car is in goal
    
    Quat carQuat = RT::RotatorToQuat(car.GetRotation());
    RT::Matrix3 carMat = RT::QuatToMatrix(carQuat);
    
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
void DribblesAndFlicks::GetResetValues()
{
    if(!ShouldRun()) return;
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    BallWrapper ball = server.GetBall();
    CarWrapper car = gameWrapper->GetLocalCar();

    Quat carQuat = RT::RotatorToQuat(car.GetRotation());
    RT::Matrix3 carMat = RT::QuatToMatrix(carQuat);

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
            duration<double> fwdValsDuration = duration_cast<duration<double>>(lastTime - firstTime);
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
            duration<double> rightValsDuration = duration_cast<duration<double>>(lastTime - firstTime);
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
    for(int i=0; i<ballResetFwdVals.size(); i++)
        fwdAverage += ballResetFwdVals[i].value;
    for(int i=0; i<ballResetRightVals.size(); i++)
        rightAverage += ballResetRightVals[i].value;

    fwdAverage /= ballResetFwdVals.size();
    rightAverage /= ballResetRightVals.size();

    ballResetPosFwd = fwdAverage;
    ballResetPosRight = rightAverage;
    ballResetPosZ = spawnOffset.Z;
    ballResetVelocity = velocityAdjust;
}

//Tick
void DribblesAndFlicks::Tick()
{
    if(!ShouldRun()) return;
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    BallWrapper ball = server.GetBall();
    CarWrapper car = gameWrapper->GetLocalCar();

    //GET RESET VALUES
    Vector speedChange = car.GetVelocity() - previousVelocity;
    duration<double> timeChange = duration_cast<duration<double>>(steady_clock::now() - previousTime);
    carAcceleration = (speedChange * 0.036) * (1 / timeChange.count());
    
    Quat carQuat = RT::RotatorToQuat(car.GetRotation());
    RT::Matrix3 carMat = RT::QuatToMatrix(carQuat);
    accelDotFwd = Vector::dot(carMat.forward, carAcceleration);//range -150 to 150
    accelDotRight = Vector::dot(carMat.right, carAcceleration);//range -250 to 250
    velocityForward = Vector::dot(car.GetVelocity(),carMat.forward);
    velocityRight = Vector::dot(car.GetVelocity(),carMat.right);

    previousVelocity = car.GetVelocity();
    previousTime = steady_clock::now();

    GetResetValues();

    //MODES
    //Dribble Reset
    if(cvarManager->getCvar(toggleDribbleModeName).getBoolValue())
    {
        float ballHeight = ball.GetLocation().Z - (ball.GetRadius() + *floorThreshold);
        if(ballHeight <= 0)
            Reset();
    }

    //Flick Reset
    if(cvarManager->getCvar(toggleFlicksModeName).getBoolValue())
    {
        Vector ballToCar = ball.GetLocation() - car.GetLocation();
        float flickDistance = ballToCar.magnitude();
        if(flickDistance > *maxFlickDistance)
        {
            float ballSpeed = ball.GetVelocity().magnitude();
            ballSpeed *= 0.036;//cms to kph
            if(abs(ball.GetLocation().Y) < 5120 && abs(car.GetLocation().Y) < 5120)
            {
                gameWrapper->LogToChatbox(std::to_string((int)ballSpeed) + " KPH", "Flick Speed");
                if(cvarManager->getCvar(toggleLogFlickSpeedName).getBoolValue())
                    cvarManager->log("Flick Speed: " + std::to_string((int)ballSpeed) + " KPH");
            }
            Reset();
        }
    }

    //Catch
    if(preparingToLaunch)
        HoldBallInLaunchPosition();
}

//Catch
void DribblesAndFlicks::PrepareToLaunch()
{
    preparingToLaunch = true;
    preparationStartTime = clock();
    launchNum++;
    gameWrapper->SetTimeout(std::bind(&DribblesAndFlicks::Launch, this, launchNum), *preparationTime);
}
void DribblesAndFlicks::GetNextLaunchDirection()
{
    if(!ShouldRun()) return;

    CarWrapper car = gameWrapper->GetLocalCar();
    if(car.IsNull()) return;

    RT::Matrix3 launchDirectionMatrix;

    //Rotate around UP axis
    float UpRotAmount = ((float)rand()/RAND_MAX) * (2.f * M_PI);
    Quat upRotQuat = RT::AngleAxisRotation(UpRotAmount, launchDirectionMatrix.up);
    launchDirectionMatrix = RT::RotateMatrixWithQuat(launchDirectionMatrix, upRotQuat);

    //Rotate around right axis
    float launchAngle = cvarManager->getCvar(catchAngleName).getFloatValue();
    float RightRotAmount = launchAngle * -(M_PI/180);
    Quat rightRotQuat = RT::AngleAxisRotation(RightRotAmount, launchDirectionMatrix.right);
    launchDirectionMatrix = RT::RotateMatrixWithQuat(launchDirectionMatrix, rightRotQuat);
    
    Vector spawnDirection = launchDirectionMatrix.forward;
    spawnDirection.normalize();

    nextLaunch.launchDirection = spawnDirection;

    float launchSpeed = cvarManager->getCvar(catchSpeedName).getFloatValue();
    nextLaunch.launchMagnitude = launchSpeed / 5000;

    PrepareToLaunch();
}
void DribblesAndFlicks::HoldBallInLaunchPosition()
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
void DribblesAndFlicks::Launch(int launchIndex)
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
Vector DribblesAndFlicks::CalculateLaunchAngle(Vector start, Vector target, float v)
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
        tanTheta += sqrt((v*v*v*v) - g * ((g*x*x) + (2*y*v*v)));
    else
        tanTheta -= sqrt((v*v*v*v) - g * ((g*x*x) + (2*y*v*v)));
    tanTheta /= g*x;


    return atan(tanTheta);
}
