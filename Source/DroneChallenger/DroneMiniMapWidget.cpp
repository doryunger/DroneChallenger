#include "DroneMiniMapWidget.h"
#include "DroneActor.h"
#include "TargetPawn.h"
#include "DroneGraph.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	const FColor kBgFill      {  10,  13,  20, 200 };
	const FColor kRingBorder  {  55,  62,  90, 230 };
	const FColor kRoadCasing  {  18,  22,  35, 255 };
	const FColor kRoadLine    { 200, 208, 225, 255 };
	const FColor kDroneColor  {  85, 165, 255, 255 };
	const FColor kFOVFill     { 100, 200, 255,  38 };
	const FColor kFOVEdge     { 120, 220, 255, 220 };
	const FColor kTargetColor { 255, 165,  45, 255 };
	const FColor kCompassCol  { 140, 148, 170, 200 };
	const FColor kHeadingCol  {  85, 165, 255, 255 };

	constexpr float kPerspTilt   = 0.28f;
	constexpr int32 kCircleSegs  = 56;
	constexpr float kRoadCasingW = 3.5f;
	constexpr float kRoadLineW   = 1.8f;

	FSlateResourceHandle GetFillHandle()
	{
		static FSlateResourceHandle H;
		static bool bInit = false;
		if (!bInit)
		{
			if (const FSlateBrush* B = FCoreStyle::Get().GetBrush("WhiteBrush"))
				H = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*B);
			bInit = true;
		}
		return H;
	}
}

void UDroneMiniMapWidget::Init(ADroneActor* InDrone, ATargetPawn* InTarget)
{
	Drone  = InDrone;
	Target = InTarget;
}

void UDroneMiniMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Drone)
	{
		CachedDronePos = Drone->GetActorLocation();
		CachedDroneYaw = Drone->GetActorRotation().Yaw;
		CachedAltM     = CachedDronePos.Z / 100.f;
	}
	if (Target)
	{
		CachedTargetPos    = Target->GetActorLocation();
		bCachedTargetInFOV = Target->IsDroneInFOV();
	}
}

int32 UDroneMiniMapWidget::NativePaint(
	const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size   = AllottedGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5f;
	const float     R      = FMath::Min(Center.X, Center.Y) - 4.f;

	const float AltFactor      = FMath::Clamp(FMath::Max(CachedAltM, 10.f) / FMath::Max(ReferenceAltitudeM, 1.f), 0.15f, 6.f);
	const float EffectiveRadius = FMath::Clamp(MapRadiusCm * AltFactor, MinMapRadiusCm, MaxMapRadiusCm);
	const float Scale           = R / EffectiveRadius;

	// Tighten clip radius so that after perspective scaling (max factor 1+kPerspTilt)
	// no clipped endpoint can project beyond screen radius R.
	const float ClipRadius = EffectiveRadius / (1.f + kPerspTilt) * 0.97f;

	auto LocalToAbs = [&](FVector2D P) -> FVector2f
	{
		const FVector2D A = AllottedGeometry.LocalToAbsolute(P);
		return FVector2f((float)A.X, (float)A.Y);
	};

	auto AddAbsVert = [&](TArray<FSlateVertex>& V, float LX, float LY, FColor C)
	{
		FSlateVertex Vert;
		Vert.Position     = LocalToAbs(FVector2D(LX, LY));
		Vert.TexCoords[0] = 0.5f;
		Vert.TexCoords[1] = 0.5f;
		Vert.TexCoords[2] = 0.5f;
		Vert.TexCoords[3] = 0.5f;
		Vert.Color        = C;
		V.Add(Vert);
	};

	auto MakeLines = [&](const TArray<FVector2D>& Pts, const FLinearColor& Col,
		float Thick, bool bAA = true)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(), Pts,
			ESlateDrawEffect::None, Col, bAA, Thick);
	};

	// 2.5D perspective: both axes scale by the same factor that varies with screen-Y,
	// giving the map a tilted-overhead appearance. kPerspTilt applies equally to X and Y.
	auto ToScreen = [&](float RelNorthCm, float RelEastCm) -> FVector2D
	{
		const float sX = RelEastCm   * Scale;
		const float sY = -RelNorthCm * Scale;
		const float f  = FMath::Lerp(1.f - kPerspTilt, 1.f + kPerspTilt,
			FMath::Clamp((sY / R + 1.f) * 0.5f, 0.f, 1.f));
		return FVector2D(Center.X + sX * f, Center.Y + sY * f);
	};

	// Clips segment [RA, RB] (world-space relative to drone, in cm) against a circle
	// of radius ClipRadius. Handles all cases including both endpoints outside.
	auto ClipToCircle = [&](FVector2D RA, FVector2D RB)
		-> TOptional<TPair<FVector2D, FVector2D>>
	{
		const float R2   = ClipRadius * ClipRadius;
		const bool  bAIn = RA.SizeSquared() <= R2;
		const bool  bBIn = RB.SizeSquared() <= R2;
		if (bAIn && bBIn) return TPair<FVector2D, FVector2D>(RA, RB);

		const FVector2D D = RB - RA;
		const float     a = FVector2D::DotProduct(D, D);
		if (a < KINDA_SMALL_NUMBER) return {};
		const float b    = 2.f * FVector2D::DotProduct(RA, D);
		const float c    = RA.SizeSquared() - R2;
		const float disc = b * b - 4.f * a * c;
		if (disc < 0.f) return {};

		const float sq = FMath::Sqrt(disc);
		const float t0 = (-b - sq) / (2.f * a);
		const float t1 = (-b + sq) / (2.f * a);

		if (bAIn) return TPair<FVector2D, FVector2D>(RA, RA + FMath::Clamp(t1, 0.f, 1.f) * D);
		if (bBIn) return TPair<FVector2D, FVector2D>(RA + FMath::Clamp(t0, 0.f, 1.f) * D, RB);

		// Both outside: only draw the chord if the segment transits the circle interior.
		if (t1 < 0.f || t0 > 1.f || t0 >= t1) return {};
		return TPair<FVector2D, FVector2D>(RA + FMath::Clamp(t0, 0.f, 1.f) * D,
		                                   RA + FMath::Clamp(t1, 0.f, 1.f) * D);
	};

	{
		TArray<FSlateVertex> BgV;
		TArray<SlateIndex>   BgI;
		AddAbsVert(BgV, Center.X, Center.Y, kBgFill);
		for (int32 i = 0; i < kCircleSegs; ++i)
		{
			const float A = 2.f * PI * i / kCircleSegs;
			AddAbsVert(BgV, Center.X + R * FMath::Cos(A), Center.Y + R * FMath::Sin(A), kBgFill);
		}
		for (int32 i = 0; i < kCircleSegs; ++i)
		{
			BgI.Add(0);
			BgI.Add((SlateIndex)(i + 1));
			BgI.Add((SlateIndex)((i + 1) % kCircleSegs + 1));
		}
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId,
			GetFillHandle(), BgV, BgI, nullptr, 0, 0);
	}

	if (Target)
	{
		const FDroneGraph& Graph = Target->GetGraph();
		const float BpR2 = (ClipRadius * 2.f) * (ClipRadius * 2.f);

		TArray<TPair<FVector2D, FVector2D>> VisEdges;
		for (const auto& [NodeId, Neighbors] : Graph.Adjacency)
		{
			const FVector* PosA = Graph.NodeWorldPos.Find(NodeId);
			if (!PosA) continue;
			const FVector2D RelA(PosA->X - CachedDronePos.X, PosA->Y - CachedDronePos.Y);

			for (int32 NeighId : Neighbors)
			{
				if (NeighId <= NodeId) continue;
				const FVector* PosB = Graph.NodeWorldPos.Find(NeighId);
				if (!PosB) continue;
				const FVector2D RelB(PosB->X - CachedDronePos.X, PosB->Y - CachedDronePos.Y);

				// Broad-phase: skip only if both endpoints are clearly out of range.
				if (RelA.SizeSquared() > BpR2 && RelB.SizeSquared() > BpR2) continue;

				auto Seg = ClipToCircle(RelA, RelB);
				if (!Seg) continue;

				VisEdges.Add({
					ToScreen(Seg->Key.X,   Seg->Key.Y),
					ToScreen(Seg->Value.X, Seg->Value.Y) });
			}
		}

		for (const auto& [A, B] : VisEdges)
			MakeLines({ A, B }, FLinearColor(kRoadCasing), kRoadCasingW, false);
		for (const auto& [A, B] : VisEdges)
			MakeLines({ A, B }, FLinearColor(kRoadLine),   kRoadLineW,   false);
	}

	{
		// FOV cone — built from world-space forward/right vectors for correct orientation.
		const FVector FwdWorld = Drone ? Drone->GetActorForwardVector() : FVector(1, 0, 0);
		const FVector RgtWorld = Drone ? Drone->GetActorRightVector()   : FVector(0, 1, 0);
		const float HalfFOV  = FMath::DegreesToRadians(FPVHorizontalFOVDeg * 0.5f);
		const float FrustLen = FMath::Min(DetectionRangeCm, ClipRadius * 0.95f);

		auto FovPt = [&](float SignedAngle) -> FVector2D
		{
			const float CosA = FMath::Cos(SignedAngle);
			const float SinA = FMath::Sin(SignedAngle);
			return ToScreen((FwdWorld.X * CosA + RgtWorld.X * SinA) * FrustLen,
			                (FwdWorld.Y * CosA + RgtWorld.Y * SinA) * FrustLen);
		};

		constexpr int32 kFovSegs = 18;

		TArray<FSlateVertex> FovV;
		TArray<SlateIndex>   FovI;
		AddAbsVert(FovV, Center.X, Center.Y, kFOVFill);
		for (int32 i = 0; i <= kFovSegs; ++i)
		{
			const FVector2D P = FovPt(FMath::Lerp(-HalfFOV, HalfFOV, (float)i / kFovSegs));
			AddAbsVert(FovV, P.X, P.Y, kFOVFill);
		}
		for (int32 i = 0; i < kFovSegs; ++i)
		{
			FovI.Add(0);
			FovI.Add((SlateIndex)(i + 1));
			FovI.Add((SlateIndex)(i + 2));
		}
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId,
			GetFillHandle(), FovV, FovI, nullptr, 0, 0);

		const FVector2D FovLeft  = FovPt(-HalfFOV);
		const FVector2D FovRight = FovPt( HalfFOV);
		MakeLines({ Center, FovLeft  }, FLinearColor(kFOVEdge), 1.5f);
		MakeLines({ Center, FovRight }, FLinearColor(kFOVEdge), 1.5f);

		TArray<FVector2D> FovArc;
		FovArc.Reserve(kFovSegs + 1);
		for (int32 i = 0; i <= kFovSegs; ++i)
			FovArc.Add(FovPt(FMath::Lerp(-HalfFOV, HalfFOV, (float)i / kFovSegs)));
		MakeLines(FovArc, FLinearColor(kFOVEdge), 1.5f);
	}

	if (Target)
	{
		const FVector2D TgtRel(CachedTargetPos.X - CachedDronePos.X,
			                   CachedTargetPos.Y - CachedDronePos.Y);
		if (TgtRel.SizeSquared() <= ClipRadius * ClipRadius)
		{
			const FVector2D TgtScreen = ToScreen(TgtRel.X, TgtRel.Y);
			const float DotR = R * 0.045f;
			const FColor TgtCol = bCachedTargetInFOV ? FColor(255, 230, 80, 255) : kTargetColor;

			TArray<FSlateVertex> TV;
			TArray<SlateIndex>   TI;
			constexpr int32 kDotN = 12;
			AddAbsVert(TV, TgtScreen.X, TgtScreen.Y, TgtCol);
			for (int32 i = 0; i < kDotN; ++i)
			{
				const float A = 2.f * PI * i / kDotN;
				AddAbsVert(TV, TgtScreen.X + DotR * FMath::Cos(A),
					TgtScreen.Y + DotR * FMath::Sin(A), TgtCol);
			}
			for (int32 i = 0; i < kDotN; ++i)
			{
				TI.Add(0);
				TI.Add((SlateIndex)(i + 1));
				TI.Add((SlateIndex)((i + 1) % kDotN + 1));
			}
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId,
				GetFillHandle(), TV, TI, nullptr, 0, 0);

			TArray<FVector2D> Ring;
			for (int32 i = 0; i <= kDotN; ++i)
			{
				const float A = 2.f * PI * i / kDotN;
				Ring.Add(TgtScreen + FVector2D(DotR * 1.6f * FMath::Cos(A),
					DotR * 1.6f * FMath::Sin(A)));
			}
			MakeLines(Ring, FLinearColor(TgtCol) * 0.8f, 1.2f);
		}
	}

	{
		const float HeadRad  = FMath::DegreesToRadians(CachedDroneYaw - 90.f);
		const FVector2D Fwd ( FMath::Cos(HeadRad),  FMath::Sin(HeadRad));
		const FVector2D Perp(-FMath::Sin(HeadRad),  FMath::Cos(HeadRad));
		const float ArrowLen  = R * 0.13f;
		const float ArrowHalf = R * 0.065f;
		const FVector2D ATip = Center + Fwd  * ArrowLen;
		const FVector2D ABL  = Center - Fwd  * ArrowLen * 0.35f + Perp * ArrowHalf;
		const FVector2D ABR  = Center - Fwd  * ArrowLen * 0.35f - Perp * ArrowHalf;

		TArray<FSlateVertex> AV;
		TArray<SlateIndex>   AI;
		AddAbsVert(AV, ATip.X, ATip.Y, kDroneColor);
		AddAbsVert(AV, ABL.X,  ABL.Y,  kDroneColor);
		AddAbsVert(AV, ABR.X,  ABR.Y,  kDroneColor);
		AI = { 0, 1, 2 };
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId,
			GetFillHandle(), AV, AI, nullptr, 0, 0);

		MakeLines({ ATip, ABL, ABR, ATip }, FLinearColor(0.3f, 0.55f, 1.f, 1.f), 1.2f);
	}

	{
		TArray<FVector2D> MapEdge;
		for (int32 i = 0; i <= kCircleSegs; ++i)
		{
			const float A = 2.f * PI * i / kCircleSegs;
			MapEdge.Add(FVector2D(Center.X + R * FMath::Cos(A), Center.Y + R * FMath::Sin(A)));
		}
		MakeLines(MapEdge, FLinearColor(kRingBorder), 3.f);
	}

	{
		const float CompassR  = R + R * 0.10f;
		const float TickLenLg = R * 0.09f;
		const float TickLenSm = R * 0.05f;

		TArray<FVector2D> CompassRing;
		for (int32 i = 0; i <= kCircleSegs; ++i)
		{
			const float A = 2.f * PI * i / kCircleSegs;
			CompassRing.Add(FVector2D(Center.X + CompassR * FMath::Cos(A),
				Center.Y + CompassR * FMath::Sin(A)));
		}
		MakeLines(CompassRing, FLinearColor(kRingBorder) * 0.7f, 1.f);

		const TArray<TPair<FString, float>> Cardinals = {
			{ TEXT("N"),   0.f }, { TEXT("E"),  90.f },
			{ TEXT("S"), 180.f }, { TEXT("W"), 270.f },
		};
		for (const auto& [Label, Bearing] : Cardinals)
		{
			const float A   = FMath::DegreesToRadians(Bearing - 90.f);
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
			MakeLines({ Center + Dir * (CompassR - TickLenLg), Center + Dir * CompassR },
				FLinearColor(kCompassCol), 1.5f);

			const FVector2D TextPos = Center + Dir * (CompassR + 5.f) - FVector2D(5.5f, 6.f);
			FSlateDrawElement::MakeText(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(13.f, 13.f),
					FSlateLayoutTransform(FVector2f((float)TextPos.X, (float)TextPos.Y))),
				FText::FromString(Label),
				FCoreStyle::GetDefaultFontStyle("Regular", 8),
				ESlateDrawEffect::None, FLinearColor(kCompassCol));
		}

		for (float Bearing = 0.f; Bearing < 360.f; Bearing += 10.f)
		{
			if (FMath::Abs(FMath::Fmod(Bearing, 90.f)) < 0.1f) continue;
			const float A    = FMath::DegreesToRadians(Bearing - 90.f);
			const bool bMaj  = FMath::Abs(FMath::Fmod(Bearing, 30.f)) < 0.1f;
			const float TLen = bMaj ? TickLenLg * 0.7f : TickLenSm;
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
			MakeLines({ Center + Dir * (CompassR - TLen), Center + Dir * CompassR },
				FLinearColor(kCompassCol) * (bMaj ? 1.f : 0.6f), 1.f);
		}

		const float     HeadA  = FMath::DegreesToRadians(CachedDroneYaw - 90.f);
		const FVector2D HD (FMath::Cos(HeadA),  FMath::Sin(HeadA));
		const FVector2D HP (-HD.Y, HD.X);
		const FVector2D HTip   = Center + HD * (CompassR + 2.f);
		const FVector2D HBase  = Center + HD * (CompassR - TickLenLg * 1.2f);

		TArray<FSlateVertex> HV;
		TArray<SlateIndex>   HI;
		AddAbsVert(HV, HTip.X,              HTip.Y,              kHeadingCol);
		AddAbsVert(HV, (HBase + HP * TickLenSm).X, (HBase + HP * TickLenSm).Y, kHeadingCol);
		AddAbsVert(HV, (HBase - HP * TickLenSm).X, (HBase - HP * TickLenSm).Y, kHeadingCol);
		HI = { 0, 1, 2 };
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId,
			GetFillHandle(), HV, HI, nullptr, 0, 0);
	}

	return LayerId + 1;
}
