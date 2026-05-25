#include "DroneFlightController.h"
#include "DroneMotorMixer.h"

void FDroneFlightController::Update(
	const FDroneControlInput& Input,
	const FVector& AngularVelocityBodyDeg,
	float DeltaTime,
	float OutThrottle[4])
{
	const float ThrottleCmd = FMath::Clamp(HoverThrottle + Input.Throttle * ThrottleRange, 0.0f, 1.0f);

	const float DesiredPitchRate = Input.Pitch * MaxPitchRate;
	const float DesiredRollRate  = Input.Roll  * MaxRollRate;
	const float DesiredYawRate   = Input.Yaw   * MaxYawRate;

	// Body-frame angular velocity: X = roll rate, Y = pitch rate, Z = yaw rate.
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
