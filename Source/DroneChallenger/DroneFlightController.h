#pragma once

#include "CoreMinimal.h"
#include "DronePIDController.h"

struct FDroneControlInput
{
	float Throttle = 0.0f;
	float Pitch    = 0.0f;
	float Roll     = 0.0f;
	float Yaw      = 0.0f;
};

class FDroneFlightController
{
public:
	FDronePID PitchRatePID { 0.0003f, 0.0f, 0.0f, 0.5f };
	FDronePID RollRatePID  { 0.0003f, 0.0f, 0.0f, 0.5f };
	FDronePID YawRatePID   { 0.0005f, 0.0f, 0.0f, 0.5f };

	float MaxPitchRollRate = 30.0f;
	float MaxYawRate       = 30.0f;
	float HoverThrottle    = 0.0f;
	float ThrottleRange    = 0.5f;

	void Update(const FDroneControlInput& Input,
	            const FVector& ActorUp,
	            const FVector& BodyAngVelDeg,
	            float DeltaTime,
	            float OutThrottle[4]);

	void Reset();
};
