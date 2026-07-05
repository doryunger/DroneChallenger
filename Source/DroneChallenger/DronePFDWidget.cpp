#include "DronePFDWidget.h"
#include "DroneActor.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

namespace
{
	const FColor kSky        {  55, 110, 175, 255 };
	const FColor kEarth      { 110,  68,  22, 255 };
	const FColor kHorizon    { 255, 255, 255, 255 };
	const FColor kLadder     { 220, 220, 220, 220 };
	const FColor kDim        { 160, 160, 160, 140 };
	const FColor kArcFrame   {  80,  85, 110, 204 };
	const FColor kPanel      {  18,  20,  28, 224 };
	const FColor kHighlight  {  55,  95, 155, 255 };
	const FColor kValueText  { 230, 230, 230, 255 };
	const FColor kTickText   { 170, 175, 190, 217 };
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
	const FVector Vel  = Drone->GetVelocity();
	CachedSpeedMs      = FVector(Vel.X, Vel.Y, 0.f).Size() / 100.f;
	CachedVertSpeedMs  = Vel.Z / 100.f;
	CachedAltM         = Drone->GetActorLocation().Z / 100.f;
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

	// Scale up gently for ultrawide aspect ratios so the PFD fills the same
	// visual fraction of the screen as it does on 16:9.
	const float BaseR    = FMath::Min(W * 0.0627f, H * 0.1010f);
	const float ARBoost  = FMath::Sqrt(FMath::Max(1.f, (W / H) / (16.f / 9.f)));
	const float R        = BaseR * ARBoost;
	const float CX       = W * 0.5f;
	const float CY       = H - R - H * 0.02f;
	const float LabelH   = R * 0.17f;
	const float StripW   = R * 0.50f;
	const float StripGap = R * 0.24f;
	const float StripTop = CY - R + LabelH;
	const float StripH   = R * 2.f - LabelH;

	const float SpeedR_bg = CX - R - StripGap;
	const float SpeedL_bg = SpeedR_bg - StripW;
	const float AltL_bg   = CX + R + StripGap;
	const float AltR_bg   = AltL_bg + StripW;

	const int32 FontSm = FMath::Clamp(FMath::RoundToInt(R * 0.060f), 7, 16);

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

	static const FSlateBrush* const kWhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

	auto MakeBox = [&](FVector2D TL, FVector2D BR, const FColor& Col)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f((float)(BR.X - TL.X), (float)(BR.Y - TL.Y)),
				FSlateLayoutTransform(FVector2f((float)TL.X, (float)TL.Y))),
			kWhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(Col));
	};

	{
		const float BgL = FMath::Max(0.f, SpeedL_bg);
		const float BgR = FMath::Min(W,   AltR_bg);
		const float BgT = StripTop - LabelH * 0.85f;
		const float BgB = StripTop + StripH;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(BgR - BgL, BgB - BgT),
				FSlateLayoutTransform(FVector2f(BgL, BgT))),
			kWhiteBrush, ESlateDrawEffect::None,
			FLinearColor(8.f/255, 10.f/255, 16.f/255, 0.95f));
	}

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
			V.Position     = LocalToAbs(FVector2D(X, Y));
			V.TexCoords[0] = 0.5f;
			V.TexCoords[1] = 0.5f;
			V.TexCoords[2] = 0.5f;
			V.TexCoords[3] = 0.5f;
			V.Color        = Col;
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

	++LayerId;

	auto ClipToCircle = [&](FVector2D& A, FVector2D& B) -> bool
	{
		const float dx = (float)(B.X - A.X), dy = (float)(B.Y - A.Y);
		const float fx = (float)(A.X - CX),  fy = (float)(A.Y - CY);
		const float a  = dx*dx + dy*dy;
		if (a < 1e-6f) return (fx*fx + fy*fy) <= R*R;
		const float b    = 2.f*(fx*dx + fy*dy);
		const float c    = fx*fx + fy*fy - R*R;
		const float disc = b*b - 4.f*a*c;
		if (disc < 0.f) return false;
		const float sq = FMath::Sqrt(disc);
		const float t0 = FMath::Clamp((-b - sq) / (2.f*a), 0.f, 1.f);
		const float t1 = FMath::Clamp((-b + sq) / (2.f*a), 0.f, 1.f);
		if (t1 <= t0) return false;
		const FVector2D Orig = A;
		A = FVector2D(Orig.X + dx*t0, Orig.Y + dy*t0);
		B = FVector2D(Orig.X + dx*t1, Orig.Y + dy*t1);
		return true;
	};

	{
		const float CosR = FMath::Cos(RollRad);
		const float SinR = FMath::Sin(RollRad);
		FVector2D HL(CX - R * CosR, CY + PitchOffset - R * SinR);
		FVector2D HR(CX + R * CosR, CY + PitchOffset + R * SinR);
		if (ClipToCircle(HL, HR))
			MakeLines({ HL, HR }, FLinearColor(kHorizon), 2.f);
	}

	{
		struct FMark { float Deg; float Half; float Thick; bool bLabel; bool bSerif; bool bDash; };
		static constexpr FMark kMarks[] = {
			{-20.f, 0.28f, 1.5f, true,  true,  false},
			{-15.f, 0.20f, 1.2f, false, true,  false},
			{-10.f, 0.22f, 1.5f, true,  true,  false},
			{ -5.f, 0.14f, 1.2f, false, false, true },
			{ -2.5f, 0.07f, 1.0f, false, false, false},
			{  2.5f, 0.07f, 1.0f, false, false, false},
			{  5.f, 0.14f, 1.2f, false, false, true },
			{ 10.f, 0.22f, 1.5f, true,  true,  false},
			{ 15.f, 0.20f, 1.2f, false, true,  false},
			{ 20.f, 0.28f, 1.5f, true,  true,  false},
		};

		const float CosR = FMath::Cos(RollRad);
		const float SinR = FMath::Sin(RollRad);

		for (const FMark& M : kMarks)
		{
			const float PixOffset = -(M.Deg / MaxDisplayPitchDeg) * R * 0.85f;
			const float LineHalf  = R * M.Half;

			const FVector2D LC(CX - PixOffset * SinR, CY + PixOffset * CosR + PitchOffset);
			FVector2D Left (LC.X - LineHalf * CosR, LC.Y - LineHalf * SinR);
			FVector2D Right(LC.X + LineHalf * CosR, LC.Y + LineHalf * SinR);
			if (!ClipToCircle(Left, Right)) continue;

			const FLinearColor LineCol = (FMath::Abs(M.Deg) < 3.f)
				? FLinearColor(kDim) : FLinearColor(kLadder);

			if (M.bDash)
			{
				const float DashLen = R * 0.04f;
				const float GapLen  = R * 0.025f;
				const float Total   = FMath::Sqrt(
					(float)((Right.X-Left.X)*(Right.X-Left.X) + (Right.Y-Left.Y)*(Right.Y-Left.Y)));
				float Dist = 0.f;
				bool  bOn  = true;
				while (Dist < Total)
				{
					const float SegEnd = FMath::Min(Dist + (bOn ? DashLen : GapLen), Total);
					if (bOn)
					{
						const FVector2D PA = Left + FVector2D(Dist   * CosR, Dist   * SinR);
						const FVector2D PB = Left + FVector2D(SegEnd * CosR, SegEnd * SinR);
						MakeLines({ PA, PB }, LineCol, M.Thick, false);
					}
					Dist = SegEnd;
					bOn  = !bOn;
				}
			}
			else
			{
				MakeLines({ Left, Right }, LineCol, M.Thick, false);
			}

			if (M.bSerif)
			{
				const float SerLen = R * 0.055f;
				const FVector2D InDir = (M.Deg > 0.f)
					? FVector2D(-SinR,  CosR)
					: FVector2D( SinR, -CosR);
				MakeLines({ Left,  Left  + InDir * SerLen }, FLinearColor(kLadder), M.Thick, false);
				MakeLines({ Right, Right + InDir * SerLen }, FLinearColor(kLadder), M.Thick, false);
			}

			if (M.bLabel)
			{
				const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", FontSm);
				const FString Label = FString::FromInt(FMath::Abs(FMath::RoundToInt(M.Deg)));
				const FVector2D TextSize = FSlateApplication::Get().GetRenderer()
					->GetFontMeasureService()->Measure(Label, Font);
				const float LY  = Left.Y - 4.f * SinR - (float)TextSize.Y * 0.5f;
				const float TH  = (float)TextSize.Y;
				const float TW  = (float)TextSize.X;
				const float Pad = R * 0.012f;

				auto InsideCircle = [&](float X, float Y) -> bool {
					const float dx = X - CX, dy = Y - CY;
					return dx * dx + dy * dy < R * R;
				};

				const float LTX = Left.X  - Pad - TW;
				const float RTX = Right.X + Pad;
				if (InsideCircle(LTX,      LY     ) && InsideCircle(LTX,      LY + TH) &&
				    InsideCircle(LTX + TW, LY     ) && InsideCircle(LTX + TW, LY + TH))
				{
					FSlateDrawElement::MakeText(OutDrawElements, LayerId,
						AllottedGeometry.ToPaintGeometry(
							FVector2f(TW, TH),
							FSlateLayoutTransform(FVector2f(LTX, LY))),
						FText::FromString(Label), Font, ESlateDrawEffect::None,
						FLinearColor(kLadder));
				}

				if (InsideCircle(RTX,      LY     ) && InsideCircle(RTX,      LY + TH) &&
				    InsideCircle(RTX + TW, LY     ) && InsideCircle(RTX + TW, LY + TH))
				{
					FSlateDrawElement::MakeText(OutDrawElements, LayerId,
						AllottedGeometry.ToPaintGeometry(
							FVector2f(TW, TH),
							FSlateLayoutTransform(FVector2f(RTX, LY))),
						FText::FromString(Label), Font, ESlateDrawEffect::None,
						FLinearColor(kLadder));
				}
			}
		}
	}

	{
		const FColor kWing{ 255, 215, 40, 242 };
		const float  WingLen = R * 0.22f;
		const float  WingGap = 6.f;
		MakeLines({ FVector2D(CX - WingLen - WingGap, CY), FVector2D(CX - WingGap, CY) },
			FLinearColor(kWing), 2.f);
		MakeLines({ FVector2D(CX + WingGap, CY), FVector2D(CX + WingLen + WingGap, CY) },
			FLinearColor(kWing), 2.f);
		MakeBox(FVector2D(CX - 2.5f, CY - 2.5f), FVector2D(CX + 2.5f, CY + 2.5f), kWing);
	}

	{
		const TArray<float> BankAngles = { -60.f, -45.f, -30.f, -20.f, -10.f,
		                                     0.f,
		                                   10.f,  20.f,  30.f,  45.f,  60.f };
		const float EdgeR = R * 0.985f;
		for (float BankDeg : BankAngles)
		{
			const float A       = FMath::DegreesToRadians(-90.f + BankDeg);
			const bool  bMajor  = (FMath::Abs(FMath::RoundToInt(BankDeg)) % 30 == 0 || BankDeg == 0.f);
			const float TickLen = bMajor ? R * 0.08f : R * 0.045f;
			const FVector2D Outer(CX + EdgeR             * FMath::Cos(A), CY + EdgeR             * FMath::Sin(A));
			const FVector2D Inner(CX + (EdgeR - TickLen) * FMath::Cos(A), CY + (EdgeR - TickLen) * FMath::Sin(A));
			MakeLines({ Outer, Inner }, FLinearColor(kArcFrame), 1.2f);
		}

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
		float StepMinor, float StepMajor, bool bIsLeft,
		float VertSpeed = -9999.f, float ValFontMult = 0.090f, float ValXBias = 0.f)
	{
		const float TapeW      = TapeR - TapeL;
		const float PixPerUnit = StripH / (RangeHalf * 2.f);

		MakeBox(FVector2D(TapeL, StripTop), FVector2D(TapeR, StripTop + StripH), kPanel);

		const float MinorFirst = FMath::CeilToFloat ((Value - RangeHalf) / StepMinor) * StepMinor;
		const float MinorLast  = FMath::FloorToFloat((Value + RangeHalf) / StepMinor) * StepMinor;

		for (float V = MinorFirst; V <= MinorLast + 0.001f; V += StepMinor)
		{
			const float Y = CY - (V - Value) * PixPerUnit;
			if (Y < StripTop || Y > StripTop + StripH) continue;

			const bool  bMajor  = FMath::Abs(FMath::Fmod(V, StepMajor)) < StepMinor * 0.1f;
			const float TickLen = bMajor ? TapeW * 0.38f : TapeW * 0.21f;

			if (bIsLeft)
				MakeLines({ FVector2D(TapeL, Y), FVector2D(TapeL + TickLen, Y) },
					FLinearColor(bMajor ? kLadder : kDim), 1.f, false);
			else
				MakeLines({ FVector2D(TapeR - TickLen, Y), FVector2D(TapeR, Y) },
					FLinearColor(bMajor ? kLadder : kDim), 1.f, false);

			if (bMajor)
			{
				const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", FontSm);
				const FString Label = FString::FromInt(FMath::RoundToInt(V));
				const FVector2D LblSize = FSlateApplication::Get().GetRenderer()
					->GetFontMeasureService()->Measure(Label, Font);
				const float LblX = bIsLeft
					? TapeL + TickLen + 2.f
					: TapeR - TickLen - 2.f - (float)LblSize.X;
				FSlateDrawElement::MakeText(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(
						FVector2f((float)LblSize.X, (float)LblSize.Y),
						FSlateLayoutTransform(FVector2f(LblX, Y - (float)LblSize.Y * 0.5f))),
					FText::FromString(Label), Font, ESlateDrawEffect::None,
					FLinearColor(kTickText));
			}
		}

		const bool  bShowVert = VertSpeed > -9000.f;
		const float FlagH     = StripH * 0.18f;
		const float FlagL     = bIsLeft ? TapeR            : TapeL - StripGap;
		const float FlagR     = bIsLeft ? TapeR + StripGap : TapeL;
		const float EY0       = CY - FlagH * 0.5f;
		const float EY1       = CY + FlagH * 0.5f;
		MakeBox(FVector2D(FlagL, EY0), FVector2D(FlagR, EY1), kPanel);

		const FLinearColor kEdge(95.f/255, 102.f/255, 128.f/255, 228.f/255);
		if (bIsLeft)
			MakeLines({
				FVector2D(TapeR, StripTop),
				FVector2D(TapeR, EY0),
				FVector2D(FlagR, EY0),
				FVector2D(FlagR, EY1),
				FVector2D(TapeR, EY1),
				FVector2D(TapeR, StripTop + StripH) },
				kEdge, 1.5f, false);
		else
			MakeLines({
				FVector2D(TapeL, StripTop),
				FVector2D(TapeL, EY0),
				FVector2D(FlagL, EY0),
				FVector2D(FlagL, EY1),
				FVector2D(TapeL, EY1),
				FVector2D(TapeL, StripTop + StripH) },
				kEdge, 1.5f, false);

		const FSlateFontInfo ValFont = FCoreStyle::GetDefaultFontStyle("Bold",
			FMath::Clamp(FMath::RoundToInt(R * ValFontMult), 10, 24));
		const FString ValStr  = FString::Printf(TEXT("%.1f"), Value);
		const FVector2D ValSz = FSlateApplication::Get().GetRenderer()
			->GetFontMeasureService()->Measure(ValStr, ValFont);
		const float BoxCX = (FlagL + FlagR) * 0.5f;

		if (bShowVert)
		{
			const FSlateFontInfo VFont = FCoreStyle::GetDefaultFontStyle("Regular",
				FMath::Clamp(FMath::RoundToInt(R * 0.065f), 8, 14));
			const FString VStr  = FString::Printf(TEXT("%s %.1f"),
				VertSpeed >= 0.f ? TEXT("↑") : TEXT("↓"), FMath::Abs(VertSpeed));
			const FVector2D VSz = FSlateApplication::Get().GetRenderer()
				->GetFontMeasureService()->Measure(VStr, VFont);

			const float BlockH = (float)ValSz.Y + (float)VSz.Y + 1.f;
			const float BlockT = CY - BlockH * 0.5f;

			FSlateDrawElement::MakeText(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f((float)ValSz.X, (float)ValSz.Y),
					FSlateLayoutTransform(FVector2f(BoxCX - (float)ValSz.X * 0.5f + ValXBias, BlockT))),
				FText::FromString(ValStr), ValFont, ESlateDrawEffect::None, FLinearColor(kValueText));

			FSlateDrawElement::MakeText(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f((float)VSz.X, (float)VSz.Y),
					FSlateLayoutTransform(FVector2f(BoxCX - (float)VSz.X * 0.5f + ValXBias, BlockT + (float)ValSz.Y + 1.f))),
				FText::FromString(VStr), VFont, ESlateDrawEffect::None, FLinearColor(kValueText));
		}
		else
		{
			FSlateDrawElement::MakeText(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f((float)ValSz.X, (float)ValSz.Y),
					FSlateLayoutTransform(FVector2f(BoxCX - (float)ValSz.X * 0.5f + ValXBias, CY - (float)ValSz.Y * 0.5f))),
				FText::FromString(ValStr), ValFont, ESlateDrawEffect::None, FLinearColor(kValueText));
		}
	};

	const float SpeedR = CX - R - StripGap;
	const float SpeedL = SpeedR - StripW;
	DrawTape(SpeedL, SpeedR, CachedSpeedMs, 8.f,  1.f,  5.f, true, CachedVertSpeedMs);

	const float AltL = CX + R + StripGap;
	const float AltR = AltL + StripW;
	DrawTape(AltL, AltR, CachedAltM, 40.f, 5.f, 20.f, false, -9999.f, 0.090f, R * 0.06f);

	{
		MakeBox(FVector2D(SpeedL, StripTop - LabelH), FVector2D(SpeedR, StripTop), kPanel);
		MakeBox(FVector2D(AltL,   StripTop - LabelH), FVector2D(AltR,   StripTop), kPanel);

		const FLinearColor kEdge(95.f/255, 102.f/255, 128.f/255, 228.f/255);
		MakeLines({ FVector2D(SpeedR, StripTop - LabelH), FVector2D(SpeedR, StripTop) }, kEdge, 1.5f, false);
		MakeLines({ FVector2D(AltL,   StripTop - LabelH), FVector2D(AltL,   StripTop) }, kEdge, 1.5f, false);

		const FSlateFontInfo UnitFont = FCoreStyle::GetDefaultFontStyle("Bold",
			FMath::Clamp(FMath::RoundToInt(R * 0.072f), 8, 17));
		const FLinearColor UnitCol(0.80f, 0.82f, 0.90f, 0.90f);
		const float UnitY = StripTop - LabelH * 0.75f;

		const FString SpdStr = TEXT("SPD (m/s)");
		const FString AltStr = TEXT("ALT (m)");
		const FVector2D SpdSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(SpdStr, UnitFont);
		const FVector2D AltSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(AltStr, UnitFont);

		FSlateDrawElement::MakeText(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f((float)SpdSize.X, (float)SpdSize.Y),
				FSlateLayoutTransform(FVector2f(SpeedL + (StripW - (float)SpdSize.X) * 0.5f, UnitY))),
			FText::FromString(SpdStr), UnitFont, ESlateDrawEffect::None, UnitCol);
		FSlateDrawElement::MakeText(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f((float)AltSize.X, (float)AltSize.Y),
				FSlateLayoutTransform(FVector2f(AltL + (StripW - (float)AltSize.X) * 0.5f, UnitY))),
			FText::FromString(AltStr), UnitFont, ESlateDrawEffect::None, UnitCol);
	}

	return LayerId + 1;
}
