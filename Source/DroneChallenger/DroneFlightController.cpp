#include "DroneFlightController.h"
#include "DroneMotorMixer.h"

void FDroneFlightController::Update(
	const FDroneControlInput& Input,
	const FVector& AttitudeDeg,
	const FVector& AngularVelocityBodyDeg,
	float DeltaTime,
	float OutThrottle[4])
{
	const float ThrottleCmd = FMath::Clamp(HoverThrottle + Input.Throttle * ThrottleRange, 0.0f, 1.0f);

	const float DesiredPitchAngle = Input.Pitch * MaxTiltAngle;
	const float DesiredRollAngle  = Input.Roll  * MaxTiltAngle;

	const float DesiredPitchRate = (DesiredPitchAngle - AttitudeDeg.Y) * AngleGain;
	const float DesiredRollRate  = (DesiredRollAngle  - AttitudeDeg.X) * AngleGain;

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
