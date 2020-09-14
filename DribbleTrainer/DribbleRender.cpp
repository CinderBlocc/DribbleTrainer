#include "DribbleTrainer.h"

void DribbleTrainer::Render(CanvasWrapper canvas)
{
    if(!ShouldRun()) { return; }

    //Call tick function
    Tick();

    //Create frustum
    CameraWrapper camera = gameWrapper->GetCamera();
    if(camera.IsNull()) { return; }
    RA.frustum = RT::Frustum(canvas, camera);

    //Draw text showing which modes are active
    if(*bEnableDribbleMode || *bEnableFlicksMode)
    {
        DrawModesStrings(canvas);
    }

    //Draw the floor reset threshold for dribble mode
    if(*bShowFloorHeight)
    {
        DrawFloorHeight(canvas);
    }

    //Show balance safe zone
    if(*bShowSafeZone)
    {
        DrawSafeZone(canvas);
    }

    //Show launch countdown circle around ball
    if(preparingToLaunch)
    {
        DrawLaunchTimer(canvas);
    }

    //MATH HELP
    //
    //std::vector<std::string> debugString;
    //
    //if(abs(accelDotFwd) > highestDotFwd)
    //    highestDotFwd = abs(accelDotFwd);
    //if(abs(accelDotRight) > highestDotRight)
    //    highestDotRight = abs(accelDotRight);
    //debugString.push_back("Car Angular Velocity: " + vector_to_string(car.GetAngularVelocity()));
    //debugString.push_back("Car Acceleration: " + vector_to_string(carAcceleration));
    //debugString.push_back("Accel Dot Fwd: " + std::to_string(accelDotFwd));
    //debugString.push_back("Accel Dot Right: " + std::to_string(accelDotRight));
    //debugString.push_back("Highest Dot FWD: " + std::to_string(highestDotFwd));
    //debugString.push_back("Highest Dot RIGHT: " +std::to_string(highestDotRight));
    //debugString.push_back("Car Velocity Forward: " +std::to_string(velocityForward));
    //debugString.push_back("Car Velocity Right: " +std::to_string(velocityRight));
    //
    //debugString.push_back("Fwd reset vals: " + to_string(ballResetFwdVals.size()));
    //debugString.push_back("Fwd buffer time: " + to_string(fwdDurationSeconds));
    //debugString.push_back("Right reset vals: " + to_string(ballResetRightVals.size()));
    //debugString.push_back("Right buffer time: " + to_string(rightDurationSeconds));
    //
    //RT::DrawDebugStrings(canvas, debugString, true);
}

void DribbleTrainer::DrawModesStrings(CanvasWrapper canvas)
{
    std::string dribblemode = *bEnableDribbleMode ? "ON" : "OFF";
    std::string flickmode   = *bEnableFlicksMode  ? "ON" : "OFF";

    Vector2 screen = canvas.GetSize();
    int midline = screen.X / 2;

    canvas.SetColor(LinearColor{255,255,255,255});

    canvas.SetPosition(Vector2{midline, screen.Y} + Vector2{-90, -30});
    canvas.DrawString("Dribble: " + dribblemode);
    canvas.SetPosition(Vector2{midline, screen.Y} + Vector2{30, -30});
    canvas.DrawString("Flick: " + flickmode);
}

void DribbleTrainer::DrawFloorHeight(CanvasWrapper canvas)
{
    CarWrapper car = gameWrapper->GetLocalCar();
    if(car.IsNull()) { return; }

    Vector drawLocation = car.GetLocation();
    drawLocation.Z = *floorThreshold;

    RT::Circle floorHeightCircle;
    floorHeightCircle.steps = 24;

    canvas.SetColor(LinearColor{150,150,255,255});

    constexpr int circles = 4;
    for(int i = 0; i < circles; ++i)
    {
        //Draw concentric circles at floor height
        floorHeightCircle.lineThickness = 3;
        floorHeightCircle.location = drawLocation;
        floorHeightCircle.radius = 100.f - 8.f * i;
        floorHeightCircle.Draw(canvas, RA.frustum);

        //Draw additional circles vertically to display height difference
        float opacity = 255.f / ((i + 1) / 2.f);
        canvas.SetColor(LinearColor{150, 150, 255, opacity});
        floorHeightCircle.lineThickness = 3.f / (i + 1);
        floorHeightCircle.radius = 100;
        float heightSegs = *floorThreshold / (circles - 1);
        Vector loc = floorHeightCircle.location;
        loc.Z = drawLocation.Z - heightSegs * i;
        floorHeightCircle.location = loc;
        if(i != 0)
            floorHeightCircle.Draw(canvas, RA.frustum);
    }
}

void DribbleTrainer::DrawSafeZone(CanvasWrapper canvas)
{
    CameraWrapper camera = gameWrapper->GetCamera();
    if(camera.IsNull()) { return; }
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    if(server.IsNull()) { return; }
    BallWrapper ball = server.GetBall();
    if(ball.IsNull()) { return; }
    CarWrapper car = gameWrapper->GetLocalCar();
    if(car.IsNull()) { return; }
        
    Vector carLocation = car.GetLocation();
    Vector ballLocation = ball.GetLocation();
    Vector cameraLocation = camera.GetLocation();

    float ballDistanceMagnitude = (Vector{ballLocation.X, ballLocation.Y, 0} - Vector{carLocation.X, carLocation.Y, 0}).magnitude();
    
    //This would kill the safe zone rendering if the ball is too far away or below the car
    //if(ballDistanceMagnitude >= 300 || carLocation.Z > ballLocation.Z) { return; }

    RT::Matrix3 carMat(car.GetRotation());

    //Crosshair
    canvas.SetColor(LinearColor{0,255,0,255});
    canvas.DrawLine(canvas.Project((carMat.forward *  5) + carLocation), canvas.Project((carMat.forward *  50) + carLocation), 2);//forward
    canvas.DrawLine(canvas.Project((carMat.right   *  5) + carLocation), canvas.Project((carMat.right   *  50) + carLocation), 2);//right
    canvas.DrawLine(canvas.Project((carMat.forward * -5) + carLocation), canvas.Project((carMat.forward * -50) + carLocation), 2);//back
    canvas.DrawLine(canvas.Project((carMat.right   * -5) + carLocation), canvas.Project((carMat.right   * -50) + carLocation), 2);//left

    //Draw where the ball will spawn when it resets
    Vector safeZoneOffset = {0,0,0};
    safeZoneOffset = safeZoneOffset + (carMat.forward * ballResetPosFwd);
    safeZoneOffset = safeZoneOffset + (carMat.right * ballResetPosRight);
    safeZoneOffset.Z = ballResetPosZ;

    canvas.SetColor(LinearColor{0,100,255,255});
    RT::DrawVector(canvas, ballResetVelocity, carLocation + safeZoneOffset);
    canvas.SetPosition(canvas.Project(carLocation + safeZoneOffset) - Vector2{20, 20});
    canvas.DrawString(std::to_string(ballResetVelocity.magnitude()));

    RT::Sphere ballResetSphere = RT::Sphere(carLocation + safeZoneOffset, Quat(), ball.GetRadius());
    canvas.SetColor(LinearColor{0,255,0,50});
    ballResetSphere.Draw(canvas, RA.frustum, camera.GetLocation(), 64);

    //Car stable circle
    canvas.SetColor(LinearColor{0,255,0,100});
    RT::Circle goodRangeCircle;
    goodRangeCircle.radius = 20;
    goodRangeCircle.location = carLocation;
    goodRangeCircle.lineThickness = 3;
    goodRangeCircle.orientation = carMat.ToQuat();
    goodRangeCircle.Draw(canvas, RA.frustum);

    //Draw ball's location relative to car
    Vector ballDot = {ballLocation.X, ballLocation.Y, carLocation.Z};
    if(!RA.frustum.IsInFrustum(ballDot, 5.f)) { return; }

    //Draw crosshair under ball
    RT::Matrix3 ZRotMatrix;
    ZRotMatrix = RT::SingleAxisAlignment(ZRotMatrix, carMat.forward, LookAtAxis::AXIS_FORWARD, 1);
    Quat ZRot = ZRotMatrix.ToQuat();
    ZRot.normalize();

    float lineLength = 20;
    Vector2 line1Start = canvas.Project(RotateVectorWithQuat(Vector{-lineLength, 0, 0}, ZRot) + ballDot);
    Vector2 line1End   = canvas.Project(RotateVectorWithQuat(Vector{ lineLength, 0, 0}, ZRot) + ballDot);
    Vector2 line2Start = canvas.Project(RotateVectorWithQuat(Vector{0, -lineLength, 0}, ZRot) + ballDot);
    Vector2 line2End   = canvas.Project(RotateVectorWithQuat(Vector{0,  lineLength, 0}, ZRot) + ballDot);
    canvas.SetColor(LinearColor{255,255,255,255});
    canvas.DrawLine(line1Start, line1End);
    canvas.DrawLine(line2Start, line2End);

    RT::Circle ballLocationCircle;
    ballLocationCircle.radius = 4;
    ballLocationCircle.location = ballDot;
    ballLocationCircle.steps = 8;
    ballLocationCircle.Draw(canvas, RA.frustum);//DrawCircle(canvas, frustum, ballLocationCircle);

    //Calculate where the line should terminate just below the ball
    Vector ballBottom = {ballLocation.X, ballLocation.Y, ballLocation.Z - ball.GetRadius()};
    if(ballBottom.Z <= ballDot.Z || !RA.frustum.IsInFrustum(ballBottom, 0.f)) { return; }

    float ballRadius = ball.GetRadius();
    RT::Sphere ballSphere = RT::Sphere(ballLocation, ballRadius);
    RT::Line ballBottomToCamera(ballBottom, cameraLocation);
    RT::Line ballDotToCamera(ballDot, cameraLocation);
    bool isBallBottomObscured = ballSphere.IsOccludingLine(ballBottomToCamera);//RT::IsObscuredByObject(ballBottom, cameraLocation, RT::Sphere{ballLocation, ballRadius-.25f});
    bool isBallDotObscured = ballSphere.IsOccludingLine(ballDotToCamera);//RT::IsObscuredByObject(ballDot, cameraLocation, RT::Sphere{ballLocation, ballRadius});
    if(isBallDotObscured) { return; }

    if(isBallBottomObscured)
    {
        //Get tangent point along ball sphere to create the line for the line-plane intersection
        Vector cameraToBallCenter = ballLocation - cameraLocation;
        Vector cameraToBallBottom = ballBottom - cameraLocation;
        Vector projection = RT::VectorProjection(cameraToBallCenter, cameraToBallBottom) + cameraLocation;
        Vector projectionToBall = projection - ballLocation;
        projectionToBall.normalize();
        Vector tangentVert = (projectionToBall * (ballRadius-2)) + ballLocation;
        RT::Line line = RT::Line(cameraLocation, tangentVert);

        //Create a plane at ball's location with normal parallel to ground
        Vector planeVec1 = cameraLocation;
        Vector planeVec2 = ballLocation;
        planeVec1.Z = planeVec2.Z;
        Vector planeNormal = planeVec1 - planeVec2;
        RT::Plane plane = /*RT::GetPlaneFromLocationAndDirection*/RT::Plane(planeNormal, ballLocation);

        //Calculate intersection point on plane to assign new Z position to ballBottom
        Vector newBallBottom = plane.LinePlaneIntersectionPoint(line);//RT::LinePlaneIntersectionPoint(line, plane);
        ballBottom.Z = newBallBottom.Z;
    }

    canvas.DrawLine(canvas.Project(ballBottom), canvas.Project(ballDot));
}

void DribbleTrainer::DrawLaunchTimer(CanvasWrapper canvas)
{
    CameraWrapper camera = gameWrapper->GetCamera();
    if(camera.IsNull()) { return; }
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    if(server.IsNull()) { return; }
    BallWrapper ball = server.GetBall();
    if(ball.IsNull()) { return; }

    //Orient the circle so that it points at the camera. Rotate circle as timer counts down to keep it centered
    float piePercentage = 1 - (clock() - preparationStartTime) / (*preparationTime * CLOCKS_PER_SEC);
    RT::Matrix3 directionMatrix = RT::LookAt(ball.GetLocation(), camera.GetLocation(), LookAtAxis::AXIS_UP, CONST_PI_F * -piePercentage + CONST_PI_F);
    
    //Determine the number of steps the circle should have to maintain visual fidelity
    constexpr int minSteps = 8;
    constexpr int maxSteps = 40;
    float distancePerc = RT::GetVisualDistance(canvas, RA.frustum, camera, ball.GetLocation());
    int calcSteps = static_cast<int>(maxSteps * distancePerc);
    
    //Create the circle
    RT::Circle circleAroundBall(ball.GetLocation(), directionMatrix.ToQuat(), ball.GetRadius());
    circleAroundBall.lineThickness = 4;
    circleAroundBall.piePercentage = piePercentage;
    circleAroundBall.steps = max(calcSteps, minSteps);

    //Draw the circle. Change color based on how fast the ball will be launched
    canvas.SetColor(RT::GetPercentageColor(1 - nextLaunch.launchMagnitude));
    circleAroundBall.Draw(canvas, RA.frustum);
}
