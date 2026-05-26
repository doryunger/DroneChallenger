#include "DroneFlightController.h"
#include "DroneMotorMixer.h"

void FDroneFlightController::Update(
	const FDroneControlInput& Input,
	const FVector& ActorUp,
	const FVector& BodyAngVelDeg,
	float DeltaTime,
	float OutThrottle[4])
{
	const float TiltCos     = FMath::Max(ActorUp.Z, 0.5f);
	const float TiltComp    = 1.0f / TiltCos;
	const float ThrottleCmd = FMath::Clamp((HoverThrottle + Input.Throttle * ThrottleRange) * TiltComp, 0.0f, 1.0f);

	const float DesiredPitchRate = Input.Pitch * MaxPitchRollRate;
	const float DesiredRollRate  = Input.Roll  * MaxPitchRollRate;
	const float DesiredYawRate   = Input.Yaw   * MaxYawRate;

	const float PitchOut = PitchRatePID.Update(DesiredPitchRate - BodyAngVelDeg.Y, BodyAngVelDeg.Y, DeltaTime);
	const float RollOut  = RollRatePID .Update(DesiredRollRate  - BodyAngVelDeg.X, BodyAngVelDeg.X, DeltaTime);
	const float YawOut   = YawRatePID  .Update(DesiredYawRate   - BodyAngVelDeg.Z, BodyAngVelDeg.Z, DeltaTime);

	FDroneMotorMixer::Mix(ThrottleCmd, PitchOut, RollOut, YawOut, OutThrottle);
}

void FDroneFlightController::Reset()
{
	PitchRatePID.Reset();
	RollRatePID.Reset();
	YawRatePID.Reset();
}
