#pragma once

#include "CoreMinimal.h"

struct FDronePID
{
	float Kp = 1.0f;
	float Ki = 0.0f;
	float Kd = 0.0f;
	float IntegralLimit = 1.0f;

	float Integral = 0.0f;
	float PrevMeasurement = 0.0f;

	// Derivative is computed on measurement (not error) to avoid kick on setpoint changes.
	float Update(float Error, float Measurement, float DeltaTime)
	{
		Integral = FMath::Clamp(Integral + Error * DeltaTime, -IntegralLimit, IntegralLimit);
		const float Derivative = (PrevMeasurement - Measurement) / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
		PrevMeasurement = Measurement;
		return Kp * Error + Ki * Integral + Kd * Derivative;
	}

	void Reset()
	{
		Integral = 0.0f;
		PrevMeasurement = 0.0f;
	}
};
