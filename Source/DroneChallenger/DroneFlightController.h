#pragma once

#include "CoreMinimal.h"
#include "DronePIDController.h"

// Normalised stick input in [-1, 1].
struct FDroneControlInput
{
	float Throttle = 0.0f; // -1 = full descent, 0 = hover, +1 = full climb
	float Pitch    = 0.0f; // -1 = nose down / forward, +1 = nose up / backward
	float Roll     = 0.0f; // -1 = roll left, +1 = roll right
	float Yaw      = 0.0f; // -1 = yaw left, +1 = yaw right
};

// Rate-mode cascade flight controller.
// Stick inputs map to desired angular rates; rate PIDs drive the motor mixer.
class FDroneFlightController
{
public:
	// Rate PID gains — tune in-engine; start conservative.
	FDronePID PitchRatePID { 0.0008f, 0.0f, 0.00002f, 0.2f };
	FDronePID RollRatePID  { 0.0008f, 0.0f, 0.00002f, 0.2f };
	FDronePID YawRatePID   { 0.001f,  0.0f, 0.0f,     0.2f };

	// Max commanded angular rate per axis (deg/s).
	float MaxPitchRate = 180.0f;
	float MaxRollRate  = 180.0f;
	float MaxYawRate   =  90.0f;

	// Hover throttle fraction [0..1] — set by DroneActor from gravity/thrust math.
	float HoverThrottle = 0.0f;

	// Stick ±1 maps to HoverThrottle ± ThrottleRange.
	float ThrottleRange = 0.4f;

	// Fills OutThrottle[4] with per-rotor throttle commands in [0..1].
	// AngularVelocityBodyDeg: (roll, pitch, yaw) rates in drone body frame, deg/s.
	void Update(const FDroneControlInput& Input,
	            const FVector& AngularVelocityBodyDeg,
	            float DeltaTime,
	            float OutThrottle[4]);

	void Reset();
};
