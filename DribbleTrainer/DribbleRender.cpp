#include "DribbleTrainer.h"

void DribbleTrainer::Render(CanvasWrapper canvas)
{
    if(!ShouldRun()) { return; }

    //Call tick function
    Tick();

    //Nullcheck everything
    CameraWrapper camera = gameWrapper->GetCamera();
    if(camera.IsNull()) { return; }
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    if(server.IsNull()) { return; }
    BallWrapper ball = server.GetBall();
    if(ball.IsNull()) { return; }
    CarWrapper car = gameWrapper->GetLocalCar();
    if(car.IsNull()) { return; }

    //Create frustum
    RA.frustum = RT::Frustum(canvas, camera);

    //Draw text showing which modes are active
    if(*bEnableDribbleMode || *bEnableFlicksMode)
    {
        DrawModesStrings(canvas);
    }

    //Draw the floor reset threshold for dribble mode
    if(*bShowFloorHeight)
    {
        DrawFloorHeight(canvas, car);
    }

    //Show balance safe zone
    if(*bShowSafeZone)
    {
        //Kills the safe zone rendering if the ball is too far away or below the car
        float ballDistanceMagnitude = (Vector{ball.GetLocation().X, ball.GetLocation().Y, 0} - Vector{car.GetLocation().X, car.GetLocation().Y, 0}).magnitude();
        if(ballDistanceMagnitude < 300 && car.GetLocation().Z <= ball.GetLocation().Z)
        {
            DrawSafeZone(canvas, camera, car, ball);
            DrawLineUnderBall(canvas, camera, car, ball);
        }
    }

    //Show launch countdown circle around ball
    if(preparingToLaunch)
    {
        DrawLaunchTimer(canvas, camera, ball);
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

void DribbleTrainer::DrawFloorHeight(CanvasWrapper canvas, CarWrapper car)
{
    Vector drawLocation = car.GetLocation();
    drawLocation.Z = *floorThreshold;

    RT::Circle floorHeightCircle;
    floorHeightCircle.steps = 48;

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

void DribbleTrainer::DrawSafeZone(CanvasWrapper canvas, CameraWrapper camera, CarWrapper car, BallWrapper ball)
{
    //Collect values
    Vector cameraLocation = camera.GetLocation();
    Vector ballLocation = ball.GetLocation();
    Vector carLocation = car.GetLocation();
    RT::Matrix3 carMat(car.GetRotation());

    //Crosshair and center-of-balance circle
    canvas.SetColor(LinearColor{0,255,0,255});
    RT::Line crosshairFront( carMat.forward *  5 + carLocation, carMat.forward *  50 + carLocation); crosshairFront.thickness = 2; crosshairFront.Draw(canvas);
    RT::Line crosshairRight( carMat.right   *  5 + carLocation, carMat.right   *  50 + carLocation); crosshairRight.thickness = 2; crosshairRight.Draw(canvas);
    RT::Line crosshairBack ( carMat.forward * -5 + carLocation, carMat.forward * -50 + carLocation); crosshairBack.thickness  = 2; crosshairBack.Draw(canvas);
    RT::Line crosshairLeft ( carMat.right   * -5 + carLocation, carMat.right   * -50 + carLocation); crosshairLeft.thickness  = 2; crosshairLeft.Draw(canvas);
    RT::Circle centerOfBalanceCircle(carLocation, carMat.ToQuat(), 20); centerOfBalanceCircle.lineThickness = 3; centerOfBalanceCircle.Draw(canvas, RA.frustum);

    //DEVELOPMENT TESTING
    //Draw the vector of the ball's reset velocity
    //canvas.SetColor(LinearColor{0,100,255,255});
    //RT::DrawVector(canvas, ballResetVelocity, carLocation + ballResetLocation);
    //canvas.SetPosition(canvas.Project(carLocation + ballResetLocation) - Vector2{20, 20});
    //canvas.DrawString(std::to_string(ballResetVelocity.magnitude()));
    //
    //Draw the reset location
    //canvas.SetColor(LinearColor{0,255,0,50});
    //RT::Sphere ballResetSphere = RT::Sphere(carLocation + ballResetLocation, Quat(), ball.GetRadius());
    //ballResetSphere.Draw(canvas, RA.frustum, camera.GetLocation(), 64);
}

void DribbleTrainer::DrawLineUnderBall(CanvasWrapper canvas, CameraWrapper camera, CarWrapper car, BallWrapper ball)
{
    //Collect values
    Vector cameraLocation = camera.GetLocation();
    Vector ballLocation = ball.GetLocation();
    Vector carLocation = car.GetLocation();
    RT::Matrix3 carMat(car.GetRotation());
    RT::Sphere ballSphere = RT::Sphere(ballLocation, ball.GetRadius());

    //Check if ball crosshair is inside frustum, or obscured by the ball itself
    Vector ballCrosshair = {ballLocation.X, ballLocation.Y, carLocation.Z};
    RT::Line ballCrosshairToCamera(ballCrosshair, cameraLocation);
    if(!RA.frustum.IsInFrustum(ballCrosshair, 5.f) || ballSphere.IsOccludingLine(ballCrosshairToCamera)) { return; }

    //Check if the bottom of the ball is outside the frustum or if it is below the crosshair
    Vector ballBottom = {ballLocation.X, ballLocation.Y, ballLocation.Z - ball.GetRadius()};
    if(ballBottom.Z <= ballCrosshair.Z || !RA.frustum.IsInFrustum(ballBottom, 0.f)) { return; }

    //Create ball location crosshair. Keep crosshair parallel with ground, but rotated to match car planar rotation
    float lineLength = 20;
    Quat ZRot = RT::SingleAxisAlignment(RT::Matrix3(), carMat.forward, LookAtAxis::AXIS_FORWARD, 1).ToQuat();
    Vector2F line1Start = canvas.ProjectF(RotateVectorWithQuat(Vector{-lineLength, 0, 0}, ZRot) + ballCrosshair);
    Vector2F line1End   = canvas.ProjectF(RotateVectorWithQuat(Vector{ lineLength, 0, 0}, ZRot) + ballCrosshair);
    Vector2F line2Start = canvas.ProjectF(RotateVectorWithQuat(Vector{0, -lineLength, 0}, ZRot) + ballCrosshair);
    Vector2F line2End   = canvas.ProjectF(RotateVectorWithQuat(Vector{0,  lineLength, 0}, ZRot) + ballCrosshair);
    RT::Circle ballCrosshairCircle(ballCrosshair, Quat(), 4.f); ballCrosshairCircle.steps = 8;
    
    //Draw ball location crosshair
    canvas.SetColor(LinearColor{255,255,255,255});
    canvas.DrawLine(line1Start, line1End);
    canvas.DrawLine(line2Start, line2End);
    ballCrosshairCircle.Draw(canvas, RA.frustum);

    //Check if the ball is obscuring the top of the line. If it is, determine new "top" of the line
    RT::Line ballBottomToCamera(ballBottom, cameraLocation);
    if(ballSphere.IsOccludingLine(ballBottomToCamera))
    {
        //Get tangent point along ball sphere to create the line for the line-plane intersection
        Vector cameraToBallCenter = ballLocation - cameraLocation;
        Vector cameraToBallBottom = ballBottom - cameraLocation;
        Vector projection = RT::VectorProjection(cameraToBallCenter, cameraToBallBottom) + cameraLocation;
        Vector projectionToBall = projection - ballLocation;
        projectionToBall.normalize();
        Vector tangentVert = (projectionToBall * (ball.GetRadius() - 2)) + ballLocation;
        RT::Line line = RT::Line(cameraLocation, tangentVert);

        //Create a plane at ball's location with normal parallel to ground
        Vector planeVec1 = cameraLocation;
        Vector planeVec2 = ballLocation;
        planeVec1.Z = planeVec2.Z;
        Vector planeNormal = planeVec1 - planeVec2;
        RT::Plane plane = RT::Plane(planeNormal, ballLocation);

        //Calculate intersection point on plane to assign new Z position to ballBottom
        Vector newBallBottom = plane.LinePlaneIntersectionPoint(line);
        ballBottom.Z = newBallBottom.Z;
    }

    //Draw the vertical line from the ball crosshair to the calculated bottom of the ball
    canvas.DrawLine(canvas.ProjectF(ballBottom), canvas.ProjectF(ballCrosshair));
}

void DribbleTrainer::DrawLaunchTimer(CanvasWrapper canvas, CameraWrapper camera, BallWrapper ball)
{
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
