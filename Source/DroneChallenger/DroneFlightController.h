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
	FDronePID PitchRatePID { 0.0008f, 0.0f, 0.00002f, 0.2f };
	FDronePID RollRatePID  { 0.0008f, 0.0f, 0.00002f, 0.2f };
	FDronePID YawRatePID   { 0.001f,  0.0f, 0.0f,     0.2f };

	float AngleGain              = 10.0f;
	float MaxTiltAngle           = 50.0f;
	float MaxYawRate             = 90.0f;
	float MaxAngleCorrectionRate = 300.0f;
	float HoverThrottle          = 0.0f;
	float ThrottleRange          = 0.4f;

	void Update(const FDroneControlInput& Input,
	            const FVector& AttitudeDeg,
	            const FVector& AngularVelocityBodyDeg,
	            float DeltaTime,
	            float OutThrottle[4]);

	void Reset();
};
