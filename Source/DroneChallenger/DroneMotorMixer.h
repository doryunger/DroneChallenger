#pragma once

#include "CoreMinimal.h"

// X-configuration quadcopter motor mixer.
//
// Motor layout (top view, X = forward, Y = right):
//   FL(0,CCW)  FR(1,CW)
//   RL(2,CW)   RR(3,CCW)
//
// Mixing table — sign = contribution direction:
//             Throttle  Pitch  Roll  Yaw
//   FL (CCW):    +        -     +     +
//   FR  (CW):    +        -     -     -
//   RL  (CW):    +        +     +     -
//   RR (CCW):    +        +     -     +
//
// Pitch > 0 → nose up (rear rotors spin up).
// Roll  > 0 → right roll (left rotors spin up).
// Yaw   > 0 → yaw right / CW from above (CCW rotors spin up).
struct FDroneMotorMixer
{
	static void Mix(float Throttle, float Pitch, float Roll, float Yaw, float OutThrottle[4])
	{
		OutThrottle[0] = Throttle - Pitch + Roll + Yaw; // FL
		OutThrottle[1] = Throttle - Pitch - Roll - Yaw; // FR
		OutThrottle[2] = Throttle + Pitch + Roll - Yaw; // RL
		OutThrottle[3] = Throttle + Pitch - Roll + Yaw; // RR

		for (int32 i = 0; i < 4; ++i)
			OutThrottle[i] = FMath::Clamp(OutThrottle[i], 0.0f, 1.0f);
	}
};
