#include "DronePFDWidget.h"
#include "DroneActor.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	const FColor kSky        {  55, 110, 175, 255 };
	const FColor kEarth      { 110,  68,  22, 255 };
	const FColor kHorizon    { 255, 255, 255, 255 };
	const FColor kLadder     { 220, 220, 220, 220 };
	const FColor kDim        { 160, 160, 160, 180 };
	const FColor kArcFrame   {  80,  85, 110, 200 };
	const FColor kPanel      {  18,  20,  28, 215 };
	const FColor kHighlight  {  55,  95, 155, 255 };
	const FColor kValueText  { 230, 230, 230, 255 };
	const FColor kTickText   { 170, 175, 190, 200 };
}

static FSlateResourceHandle GetFillHandle()
{
	static FSlateResourceHandle Handle;
	static bool bInit = false;
	if (!bInit)
	{
		if (const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush"))
			Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
		bInit = true;
	}
	return Handle;
}

void UDronePFDWidget::Init(ADroneActor* InDrone)
{
	Drone = InDrone;
}

void UDronePFDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Drone) return;
	const FRotator Rot = Drone->GetActorRotation();
	CachedPitch   = Rot.Pitch;
	CachedRoll    = Rot.Roll;
	CachedSpeedMs = Drone->GetVelocity().Size() / 100.f;
	CachedAltM    = Drone->GetActorLocation().Z / 100.f;
}

int32 UDronePFDWidget::NativePaint(
	const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float W = Size.X;
	const float H = Size.Y;

	const float R       = FMath::Min(W * 0.32f, H * 0.44f);
	const float CX      = W * 0.5f;
	const float CY      = H * 0.5f;
	const float StripW  = W * 0.16f;
	const float StripGap = W * 0.025f;
	const float StripH  = R * 2.f;
	const float StripTop = CY - R;

	const float PitchOffset = FMath::Clamp(
		(CachedPitch / MaxDisplayPitchDeg) * R * 0.85f, -R * 0.97f, R * 0.97f);
	const float RollRad     = FMath::DegreesToRadians(CachedRoll);
	const FVector2D HorizonPt(CX, CY + PitchOffset);
	const FVector2D SkyNormal(FMath::Sin(RollRad), -FMath::Cos(RollRad));

	const auto Geo = AllottedGeometry.ToPaintGeometry();

	auto MakeLines = [&](const TArray<FVector2D>& Pts, const FLinearColor& Col,
		float Thick, bool bAA = true)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geo,
			Pts, ESlateDrawEffect::None, Col, bAA, Thick);
	};

	auto MakeBox = [&](FVector2D TL, FVector2D BR, const FColor& Col)
	{
		const FLinearColor LC = FLinearColor(Col);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(TL, BR - TL),
			&FSlateColorBrush(LC));
	};

	auto LocalToAbs = [&](FVector2D P) -> FVector2f
	{
		const FVector2D A = AllottedGeometry.LocalToAbsolute(P);
		return FVector2f((float)A.X, (float)A.Y);
	};

	auto AddRect = [&](TArray<FSlateVertex>& Verts, TArray<SlateIndex>& Idx,
		float X1, float Y1, float X2, float Y2, const FColor& Col)
	{
		const SlateIndex B = (SlateIndex)Verts.Num();
		auto AV = [&](float X, float Y) {
			FSlateVertex V;
			V.Position  = LocalToAbs(FVector2D(X, Y));
			V.TexCoords = FVector4f(0.5f, 0.5f, 0.5f, 0.5f);
			V.Color     = Col;
			Verts.Add(V);
		};
		AV(X1, Y1); AV(X2, Y1); AV(X2, Y2); AV(X1, Y2);
		Idx.Add(B);   Idx.Add(B+1); Idx.Add(B+2);
		Idx.Add(B);   Idx.Add(B+2); Idx.Add(B+3);
	};

	{
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex>   Indices;

		const float Step  = 1.5f;
		const float YMin  = CY - R;
		const float YMax  = CY + R;

		for (float Y = YMin; Y <= YMax; Y += Step)
		{
			const float Dy        = Y - CY;
			const float ChordHalf = FMath::Sqrt(FMath::Max(0.f, R * R - Dy * Dy));
			const float XL        = CX - ChordHalf;
			const float XR        = CX + ChordHalf;
			if (XR <= XL) continue;

			auto SignedDist = [&](float X, float YRow) -> float {
				return (X - HorizonPt.X) * SkyNormal.X + (YRow - HorizonPt.Y) * SkyNormal.Y;
			};

			const float DL = SignedDist(XL, Y);
			const float DR = SignedDist(XR, Y);

			if (DL >= 0.f && DR >= 0.f)
			{
				AddRect(Verts, Indices, XL, Y, XR, Y + Step, kSky);
			}
			else if (DL <= 0.f && DR <= 0.f)
			{
				AddRect(Verts, Indices, XL, Y, XR, Y + Step, kEarth);
			}
			else
			{
				float XH;
				if (FMath::Abs(SkyNormal.X) > KINDA_SMALL_NUMBER)
					XH = HorizonPt.X - (Y - HorizonPt.Y) * SkyNormal.Y / SkyNormal.X;
				else
					XH = HorizonPt.X;
				XH = FMath::Clamp(XH, XL, XR);

				if (DL >= 0.f)
				{
					AddRect(Verts, Indices, XL, Y, XH, Y + Step, kSky);
					AddRect(Verts, Indices, XH, Y, XR, Y + Step, kEarth);
				}
				else
				{
					AddRect(Verts, Indices, XL, Y, XH, Y + Step, kEarth);
					AddRect(Verts, Indices, XH, Y, XR, Y + Step, kSky);
				}
			}
		}

		if (Verts.Num() > 0)
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId,
				GetFillHandle(), Verts, Indices, nullptr, 0, 0);
	}

	{
		const float HLen = HorizonPt.X > CX ? W : W;
		const float CosR = FMath::Cos(RollRad);
		const float SinR = FMath::Sin(RollRad);
		const float Ext  = R * 1.1f;
		MakeLines({
			FVector2D(CX - Ext * CosR, CY + PitchOffset - Ext * SinR),
			FVector2D(CX + Ext * CosR, CY + PitchOffset + Ext * SinR) },
			FLinearColor(kHorizon), 2.f);
	}

	{
		const float CosR = FMath::Cos(RollRad);
		const float SinR = FMath::Sin(RollRad);
		const TArray<float> PitchAngles = { -20.f, -10.f, -5.f, 5.f, 10.f, 20.f };
		for (float Angle : PitchAngles)
		{
			const float PixOffset = -(Angle / MaxDisplayPitchDeg) * R * 0.85f;
			const float PX = CX + (PitchOffset + PixOffset) * SinR;
			const float PY = CY + PitchOffset - PixOffset * CosR + PixOffset * SinR;

			const float LineHalf = (FMath::Abs(Angle) >= 10.f) ? R * 0.28f : R * 0.15f;
			const FVector2D Ladder_CX(CX + (PitchOffset + PixOffset) * SinR,
				CY + (PitchOffset + PixOffset) * (-CosR) + CY - CY);

			const FVector2D LC(CX - (-PixOffset) * SinR, CY + (-PixOffset) * CosR + PitchOffset);
			const FVector2D Left (LC.X - LineHalf * CosR, LC.Y - LineHalf * SinR);
			const FVector2D Right(LC.X + LineHalf * CosR, LC.Y + LineHalf * SinR);
			MakeLines({ Left, Right }, FLinearColor(kLadder), 1.f, false);

			if (FMath::Abs(Angle) >= 10.f)
			{
				const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
				const FString Label = FString::FromInt(FMath::Abs(FMath::RoundToInt(Angle)));
				const FVector2D LabelPos(Left.X - R * 0.12f, Left.Y - 7.f);
				FSlateDrawElement::MakeText(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(22.f, 14.f),
						FSlateLayoutTransform(FVector2f((float)LabelPos.X, (float)LabelPos.Y))),
					FText::FromString(Label), Font, ESlateDrawEffect::None,
					FLinearColor(kLadder));
			}
		}
	}

	{
		const float ArcOuter = R + R * 0.08f;
		const float ArcInner = R + R * 0.01f;
		const float ArcSpan  = 150.f;
		constexpr int32 kArcSegs = 40;

		TArray<FVector2D> ArcOPts, ArcIPts;
		for (int32 i = 0; i <= kArcSegs; ++i)
		{
			const float A = FMath::DegreesToRadians(-90.f - ArcSpan * 0.5f + ArcSpan * i / kArcSegs);
			ArcOPts.Add(FVector2D(CX + ArcOuter * FMath::Cos(A), CY + ArcOuter * FMath::Sin(A)));
			ArcIPts.Add(FVector2D(CX + ArcInner * FMath::Cos(A), CY + ArcInner * FMath::Sin(A)));
		}
		MakeLines(ArcOPts, FLinearColor(kArcFrame), 1.f);

		const TArray<float> RollTicks = { -60.f, -45.f, -30.f, -20.f, -10.f,
		                                     0.f,
		                                   10.f,  20.f,  30.f,  45.f,  60.f };
		for (float TickDeg : RollTicks)
		{
			const float A      = FMath::DegreesToRadians(-90.f + TickDeg);
			const float TickLen = (FMath::Abs(FMath::RoundToInt(TickDeg)) % 30 == 0 || TickDeg == 0.f)
				? R * 0.09f : R * 0.05f;
			const FVector2D TO(CX + ArcOuter * FMath::Cos(A), CY + ArcOuter * FMath::Sin(A));
			const FVector2D TI(CX + (ArcOuter - TickLen) * FMath::Cos(A),
				CY + (ArcOuter - TickLen) * FMath::Sin(A));
			MakeLines({ TI, TO }, FLinearColor(kArcFrame), 1.2f);
		}

		const float IndicatorAngle = FMath::DegreesToRadians(-90.f + CachedRoll);
		const float IndR     = ArcOuter + R * 0.06f;
		const float IndBase  = ArcOuter + R * 0.01f;
		const float IndHalf  = R * 0.04f;
		const FVector2D IndTip(CX + IndR * FMath::Cos(IndicatorAngle),
			CY + IndR * FMath::Sin(IndicatorAngle));
		const FVector2D IndPerp(-FMath::Sin(IndicatorAngle), FMath::Cos(IndicatorAngle));
		const FVector2D IndBaseL = FVector2D(CX + IndBase * FMath::Cos(IndicatorAngle),
			CY + IndBase * FMath::Sin(IndicatorAngle)) + IndPerp * IndHalf;
		const FVector2D IndBaseR = FVector2D(CX + IndBase * FMath::Cos(IndicatorAngle),
			CY + IndBase * FMath::Sin(IndicatorAngle)) - IndPerp * IndHalf;
		MakeLines({ IndTip, IndBaseL, IndBaseR, IndTip }, FLinearColor(kHorizon), 1.5f);
	}

	{
		constexpr int32 kCircleSegs = 64;
		TArray<FVector2D> Circle;
		for (int32 i = 0; i <= kCircleSegs; ++i)
		{
			const float A = 2.f * PI * i / kCircleSegs;
			Circle.Add(FVector2D(CX + R * FMath::Cos(A), CY + R * FMath::Sin(A)));
		}
		MakeLines(Circle, FLinearColor(kArcFrame), 2.f);
	}

	auto DrawTape = [&](float TapeL, float TapeR, float Value, float RangeHalf,
		float StepMinor, float StepMajor, const FString& UnitStr)
	{
		MakeBox(FVector2D(TapeL, StripTop), FVector2D(TapeR, StripTop + StripH),
			kPanel);

		const float TapeW     = TapeR - TapeL;
		const float PixPerUnit = StripH / (RangeHalf * 2.f);
		const float MinorFirst = FMath::CeilToFloat((Value - RangeHalf) / StepMinor) * StepMinor;
		const float MinorLast  = FMath::FloorToFloat((Value + RangeHalf) / StepMinor) * StepMinor;

		for (float V = MinorFirst; V <= MinorLast; V += StepMinor)
		{
			const float Y  = CY - (V - Value) * PixPerUnit;
			if (Y < StripTop || Y > StripTop + StripH) continue;

			const bool bMajor = FMath::Abs(FMath::Fmod(V, StepMajor)) < StepMinor * 0.1f;
			const float TickLen = bMajor ? TapeW * 0.45f : TapeW * 0.25f;
			const float TickX   = TapeR - TickLen;

			MakeLines({ FVector2D(TickX, Y), FVector2D(TapeR, Y) },
				FLinearColor(bMajor ? kLadder : kDim), 1.f, false);

			if (bMajor)
			{
				const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 9);
				const FString Label = FString::FromInt(FMath::RoundToInt(V));
				FSlateDrawElement::MakeText(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(TapeW * 0.8f, 14.f),
						FSlateLayoutTransform(FVector2f((float)TapeL + 2.f, (float)Y - 7.f))),
					FText::FromString(Label), Font, ESlateDrawEffect::None,
					FLinearColor(kTickText));
			}
		}

		const float BoxH    = StripH * 0.11f;
		const float BoxHalf = BoxH * 0.5f;
		MakeBox(FVector2D(TapeL + 1.f, CY - BoxHalf),
			FVector2D(TapeR - 1.f, CY + BoxHalf), kHighlight);

		const FSlateFontInfo ValFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
		const FString ValStr = FString::Printf(TEXT("%.1f"), Value);
		FSlateDrawElement::MakeText(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(TapeW, BoxH),
				FSlateLayoutTransform(FVector2f((float)TapeL, (float)(CY - BoxHalf)))),
			FText::FromString(ValStr), ValFont, ESlateDrawEffect::None,
			FLinearColor(kValueText));
	};

	const float SpeedR = CX - R - StripGap;
	const float SpeedL = SpeedR - StripW;
	DrawTape(SpeedL, SpeedR, CachedSpeedMs, 8.f, 1.f, 5.f, TEXT("m/s"));

	const float AltL = CX + R + StripGap;
	const float AltR = AltL + StripW;
	DrawTape(AltL, AltR, CachedAltM, 40.f, 5.f, 20.f, TEXT("m"));

	const FSlateFontInfo UnitFont = FCoreStyle::GetDefaultFontStyle("Regular", 7);
	FSlateDrawElement::MakeText(OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2f(StripW, 12.f),
			FSlateLayoutTransform(FVector2f((float)SpeedL, (float)(StripTop - 12.f)))),
		FText::FromString(TEXT("SPD m/s")), UnitFont,
		ESlateDrawEffect::None, FLinearColor(kDim));
	FSlateDrawElement::MakeText(OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2f(StripW, 12.f),
			FSlateLayoutTransform(FVector2f((float)AltL, (float)(StripTop - 12.f)))),
		FText::FromString(TEXT("ALT m")), UnitFont,
		ESlateDrawEffect::None, FLinearColor(kDim));

	return LayerId + 1;
}
