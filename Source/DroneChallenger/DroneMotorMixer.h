#pragma once

#include "CoreMinimal.h"

struct FDroneMotorMixer
{
	static void Mix(float Throttle, float Pitch, float Roll, float Yaw, float OutThrottle[4])
	{
		OutThrottle[0] = Throttle + Pitch + Roll + Yaw;
		OutThrottle[1] = Throttle + Pitch - Roll - Yaw;
		OutThrottle[2] = Throttle - Pitch + Roll - Yaw;
		OutThrottle[3] = Throttle - Pitch - Roll + Yaw;

		for (int32 i = 0; i < 4; ++i)
			OutThrottle[i] = FMath::Clamp(OutThrottle[i], 0.0f, 1.0f);
	}
};
