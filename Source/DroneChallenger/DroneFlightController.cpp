#include "DroneFlightController.h"
#include "DroneMotorMixer.h"

void FDroneFlightController::Update(
	const FDroneControlInput& Input,
	const FVector& ActorUp,
	const FVector& BodyAngVelDeg,
	float WorldYawRateDeg,
	float DeltaTime,
	float OutThrottle[4])
{
	const float TiltCos     = FMath::Max(ActorUp.Z, 0.5f);
	const float TiltComp    = 1.0f / TiltCos;
	const float ThrottleCmd = FMath::Clamp((HoverThrottle + Input.Throttle * ThrottleRange) * TiltComp, 0.0f, 1.0f);

	const float TiltAngleDeg  = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(ActorUp.Z, -1.f, 1.f)));
	const float LimitScale    = 1.f - FMath::Clamp((TiltAngleDeg - MaxTiltAngleDeg) / 15.f, 0.f, 1.f);

	const float TiltRollRad  = FMath::Atan2( ActorUp.Y, ActorUp.Z);
	const float TiltPitchRad = FMath::Atan2(-ActorUp.X, ActorUp.Z);

	constexpr float NearInvertedRad = PI - PI / 12.f;
	const bool bRollNearInverted  = FMath::Abs(TiltRollRad)  > NearInvertedRad;
	const bool bPitchNearInverted = FMath::Abs(TiltPitchRad) > NearInvertedRad;

	const bool bRollCorrects  = bRollNearInverted
	                          || (FMath::Abs(BodyAngVelDeg.X) > 2.f && Input.Roll  * BodyAngVelDeg.X < 0.f)
	                          || (FMath::Abs(TiltRollRad)  > 0.035f && Input.Roll  * TiltRollRad  < 0.f);
	const bool bPitchCorrects = bPitchNearInverted
	                          || (FMath::Abs(BodyAngVelDeg.Y) > 2.f && Input.Pitch * BodyAngVelDeg.Y < 0.f)
	                          || (FMath::Abs(TiltPitchRad) > 0.035f && Input.Pitch * TiltPitchRad < 0.f);

	const float DesiredPitchRate = Input.Pitch * MaxPitchRollRate * (bPitchCorrects ? 1.f : LimitScale);
	const float DesiredRollRate  = Input.Roll  * MaxPitchRollRate * (bRollCorrects  ? 1.f : LimitScale);
	const float DesiredYawRate   = Input.Yaw   * MaxYawRate;

	const float PitchOut = PitchRatePID.Update(DesiredPitchRate - BodyAngVelDeg.Y, BodyAngVelDeg.Y, DeltaTime);
	const float RollOut  = RollRatePID .Update(DesiredRollRate  - BodyAngVelDeg.X, BodyAngVelDeg.X, DeltaTime);
	const float YawOut   = YawRatePID  .Update(DesiredYawRate   - WorldYawRateDeg, WorldYawRateDeg, DeltaTime);

	FDroneMotorMixer::Mix(ThrottleCmd, PitchOut, RollOut, YawOut, OutThrottle);
}

void FDroneFlightController::Reset()
{
	PitchRatePID.Reset();
	RollRatePID.Reset();
	YawRatePID.Reset();
}
