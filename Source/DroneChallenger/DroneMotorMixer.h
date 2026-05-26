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

		float MaxOut = OutThrottle[0], MinOut = OutThrottle[0];
		for (int32 i = 1; i < 4; ++i)
		{
			MaxOut = FMath::Max(MaxOut, OutThrottle[i]);
			MinOut = FMath::Min(MinOut, OutThrottle[i]);
		}
		float Shift = 0.0f;
		if (MaxOut > 1.0f) Shift = 1.0f - MaxOut;
		if ((MinOut + Shift) < 0.0f) Shift = -MinOut;
		for (int32 i = 0; i < 4; ++i)
			OutThrottle[i] = FMath::Clamp(OutThrottle[i] + Shift, 0.0f, 1.0f);
	}
};
