#include "DroneFlightController.h"
#include "DroneMotorMixer.h"

void FDroneFlightController::Update(
	const FDroneControlInput& Input,
	const FVector& AttitudeDeg,
	const FVector& AngularVelocityBodyDeg,
	float DeltaTime,
	float OutThrottle[4])
{
	const float TiltDeg  = FMath::Sqrt(AttitudeDeg.X * AttitudeDeg.X + AttitudeDeg.Y * AttitudeDeg.Y);
	const float TiltRad  = FMath::DegreesToRadians(FMath::Min(TiltDeg, 60.0f));
	const float TiltComp = 1.0f / FMath::Max(FMath::Cos(TiltRad), 0.5f);
	const float ThrottleCmd = FMath::Clamp((HoverThrottle + Input.Throttle * ThrottleRange) * TiltComp, 0.0f, 1.0f);

	const float DesiredPitchAngle = Input.Pitch * MaxTiltAngle;
	const float DesiredRollAngle  = Input.Roll  * MaxTiltAngle;

	const float DesiredPitchRate = FMath::Clamp((DesiredPitchAngle - AttitudeDeg.Y) * AngleGain, -MaxAngularRate, MaxAngularRate);
	const float DesiredRollRate  = FMath::Clamp((DesiredRollAngle  - AttitudeDeg.X) * AngleGain, -MaxAngularRate, MaxAngularRate);

	const float DesiredYawRate = Input.Yaw * MaxYawRate;

	const float PitchRateError = DesiredPitchRate - AngularVelocityBodyDeg.Y;
	const float RollRateError  = DesiredRollRate  - AngularVelocityBodyDeg.X;
	const float YawRateError   = DesiredYawRate   - AngularVelocityBodyDeg.Z;

	const float PitchOut = PitchRatePID.Update(PitchRateError, AngularVelocityBodyDeg.Y, DeltaTime);
	const float RollOut  = RollRatePID .Update(RollRateError,  AngularVelocityBodyDeg.X, DeltaTime);
	const float YawOut   = YawRatePID  .Update(YawRateError,   AngularVelocityBodyDeg.Z, DeltaTime);

	FDroneMotorMixer::Mix(ThrottleCmd, PitchOut, RollOut, YawOut, OutThrottle);
}

void FDroneFlightController::Reset()
{
	PitchRatePID.Reset();
	RollRatePID.Reset();
	YawRatePID.Reset();
}
