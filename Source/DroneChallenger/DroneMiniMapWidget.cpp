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
	const FColor kRingBorder  {  55,  62,  90, 220 };
	const FColor kRoadCasing  {  18,  22,  35, 255 };
	const FColor kRoadLine    { 200, 208, 225, 255 };
	const FColor kDroneColor  {  85, 165, 255, 255 };
	const FColor kFrustumCol  {  85, 150, 225,  70 };
	const FColor kTargetColor { 255, 165,  45, 255 };
	const FColor kCompassCol  { 140, 148, 170, 200 };
	const FColor kHeadingCol  {  85, 165, 255, 255 };

	constexpr float kPerspTilt   = 0.20f;
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
	}
	if (Target)
	{
		CachedTargetPos     = Target->GetActorLocation();
		bCachedTargetInFOV  = Target->IsDroneInFOV();
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
	const float     R      = FMath::Min(Center.X, Center.Y) - 2.f;
	const float     Scale  = R / MapRadiusCm;

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

	auto ToScreen = [&](float RelNorthCm, float RelEastCm) -> FVector2D
	{
		const float sX = RelEastCm   * Scale;
		const float sY = -RelNorthCm * Scale;
		const float perspFactor = 1.0f + (sY / R) * kPerspTilt;
		return FVector2D(Center.X + sX * perspFactor, Center.Y + sY);
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

		TArray<TPair<FVector2D, FVector2D>> VisEdges;
		for (const auto& [NodeId, Neighbors] : Graph.Adjacency)
		{
			const FVector* PosA = Graph.NodeWorldPos.Find(NodeId);
			if (!PosA) continue;
			const float DistA = FVector2D::Distance(
				FVector2D(PosA->X, PosA->Y),
				FVector2D(CachedDronePos.X, CachedDronePos.Y));
			if (DistA > MapRadiusCm * 1.35f) continue;

			const FVector2D SA = ToScreen(PosA->X - CachedDronePos.X, PosA->Y - CachedDronePos.Y);

			for (int32 NeighId : Neighbors)
			{
				if (NeighId <= NodeId) continue;
				const FVector* PosB = Graph.NodeWorldPos.Find(NeighId);
				if (!PosB) continue;
				const float DistB = FVector2D::Distance(
					FVector2D(PosB->X, PosB->Y),
					FVector2D(CachedDronePos.X, CachedDronePos.Y));
				if (DistB > MapRadiusCm * 1.35f) continue;

				const FVector2D SB = ToScreen(PosB->X - CachedDronePos.X, PosB->Y - CachedDronePos.Y);
				VisEdges.Add({ SA, SB });
			}
		}

		for (const auto& [A, B] : VisEdges)
			MakeLines({ A, B }, FLinearColor(kRoadCasing), kRoadCasingW, false);

		for (const auto& [A, B] : VisEdges)
			MakeLines({ A, B }, FLinearColor(kRoadLine), kRoadLineW, false);
	}

	{
		const float HeadRad  = FMath::DegreesToRadians(CachedDroneYaw - 90.f);
		const float HalfFOV  = FMath::DegreesToRadians(FPVHorizontalFOVDeg * 0.5f);
		const float FrustLen = DetectionRangeCm * Scale;
		const float LeftAng  = HeadRad - HalfFOV;
		const float RightAng = HeadRad + HalfFOV;

		auto FrustumPt = [&](float Ang, float Len) -> FVector2D
		{
			const float sX = FMath::Cos(Ang) * Len;
			const float sY = FMath::Sin(Ang) * Len;
			const float pf = 1.0f + (sY / R) * kPerspTilt;
			return FVector2D(Center.X + sX * pf, Center.Y + sY);
		};

		const FVector2D LeftEnd  = FrustumPt(LeftAng,  FrustLen);
		const FVector2D RightEnd = FrustumPt(RightAng, FrustLen);

		MakeLines({ Center, LeftEnd  }, FLinearColor(kFrustumCol), 1.2f);
		MakeLines({ Center, RightEnd }, FLinearColor(kFrustumCol), 1.2f);

		constexpr int32 kArcN = 14;
		TArray<FVector2D> FrustArc;
		FrustArc.Reserve(kArcN + 1);
		for (int32 i = 0; i <= kArcN; ++i)
		{
			const float A = FMath::Lerp(LeftAng, RightAng, (float)i / kArcN);
			FrustArc.Add(FrustumPt(A, FrustLen));
		}
		MakeLines(FrustArc, FLinearColor(kFrustumCol), 1.2f);
	}

	if (Target)
	{
		const FVector2D TgtScreen = ToScreen(
			CachedTargetPos.X - CachedDronePos.X,
			CachedTargetPos.Y - CachedDronePos.Y);
		const float DistPx = FVector2D::Distance(TgtScreen, Center);

		if (DistPx <= R * 1.05f)
		{
			const float DotR = R * 0.045f;
			const FColor TgtCol = bCachedTargetInFOV
				? FColor(255, 230, 80, 255)
				: kTargetColor;

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
			Ring.Reserve(kDotN + 1);
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
		const float HeadRad = FMath::DegreesToRadians(CachedDroneYaw - 90.f);
		const FVector2D Fwd (FMath::Cos(HeadRad),  FMath::Sin(HeadRad));
		const FVector2D Perp(-FMath::Sin(HeadRad), FMath::Cos(HeadRad));

		const float ArrowLen  = R * 0.13f;
		const float ArrowHalf = R * 0.065f;

		const FVector2D ATip = Center + Fwd * ArrowLen;
		const FVector2D ABL  = Center - Fwd * ArrowLen * 0.35f + Perp * ArrowHalf;
		const FVector2D ABR  = Center - Fwd * ArrowLen * 0.35f - Perp * ArrowHalf;

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
		MapEdge.Reserve(kCircleSegs + 1);
		for (int32 i = 0; i <= kCircleSegs; ++i)
		{
			const float A = 2.f * PI * i / kCircleSegs;
			MapEdge.Add(FVector2D(Center.X + R * FMath::Cos(A), Center.Y + R * FMath::Sin(A)));
		}
		MakeLines(MapEdge, FLinearColor(kRingBorder), 2.f);
	}

	{
		const float CompassR    = R + R * 0.10f;
		const float TickLenLg   = R * 0.09f;
		const float TickLenSm   = R * 0.05f;

		TArray<FVector2D> CompassRing;
		CompassRing.Reserve(kCircleSegs + 1);
		for (int32 i = 0; i <= kCircleSegs; ++i)
		{
			const float A = 2.f * PI * i / kCircleSegs;
			CompassRing.Add(FVector2D(Center.X + CompassR * FMath::Cos(A),
				Center.Y + CompassR * FMath::Sin(A)));
		}
		MakeLines(CompassRing, FLinearColor(kRingBorder) * 0.7f, 1.f);

		const TArray<TPair<FString, float>> Cardinals = {
			{ TEXT("N"),   0.f },
			{ TEXT("E"),  90.f },
			{ TEXT("S"), 180.f },
			{ TEXT("W"), 270.f },
		};
		for (const auto& [Label, Bearing] : Cardinals)
		{
			const float A = FMath::DegreesToRadians(Bearing - 90.f);
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
			const FVector2D Outer = Center + Dir * CompassR;
			const FVector2D Inner = Center + Dir * (CompassR - TickLenLg);
			MakeLines({ Inner, Outer }, FLinearColor(kCompassCol), 1.5f);

			const FVector2D TextPos = Outer + Dir * 5.f - FVector2D(5.5f, 6.f);
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
			const bool bCardinal = FMath::Abs(FMath::Fmod(Bearing, 90.f)) < 0.1f;
			if (bCardinal) continue;
			const float A = FMath::DegreesToRadians(Bearing - 90.f);
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
			const bool bMajor = FMath::Abs(FMath::Fmod(Bearing, 30.f)) < 0.1f;
			const float TLen = bMajor ? TickLenLg * 0.7f : TickLenSm;
			MakeLines({
				Center + Dir * (CompassR - TLen),
				Center + Dir *  CompassR },
				FLinearColor(kCompassCol) * (bMajor ? 1.f : 0.6f), 1.f);
		}

		const float HeadA = FMath::DegreesToRadians(CachedDroneYaw - 90.f);
		const FVector2D HD(FMath::Cos(HeadA), FMath::Sin(HeadA));
		const FVector2D HPerp(-HD.Y, HD.X);
		const float     HBase   = CompassR - TickLenLg * 1.2f;
		const FVector2D HTip    = Center + HD * (CompassR + 2.f);
		const FVector2D HBL     = Center + HD * HBase + HPerp * TickLenSm;
		const FVector2D HBR     = Center + HD * HBase - HPerp * TickLenSm;

		TArray<FSlateVertex> HV;
		TArray<SlateIndex>   HI;
		AddAbsVert(HV, HTip.X, HTip.Y, kHeadingCol);
		AddAbsVert(HV, HBL.X,  HBL.Y,  kHeadingCol);
		AddAbsVert(HV, HBR.X,  HBR.Y,  kHeadingCol);
		HI = { 0, 1, 2 };
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId,
			GetFillHandle(), HV, HI, nullptr, 0, 0);
	}

	return LayerId + 1;
}
