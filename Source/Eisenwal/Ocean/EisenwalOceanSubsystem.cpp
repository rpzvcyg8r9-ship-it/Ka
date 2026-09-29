#include "Ocean/EisenwalOceanSubsystem.h"
#include "Eisenwal.h"
#include "Core/EisenwalSettings.h"
#include "Engine/DataTable.h"

// ---------------------------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------------------------

float FEisenwalGerstnerWave::GetAngularFrequency() const
{
	return FMath::Sqrt(Eisenwal::Gravity * GetWaveNumber());
}

float FEisenwalEventWave::GetIntensity() const
{
	const float In = FadeInTime > KINDA_SMALL_NUMBER ? FMath::Clamp(Age / FadeInTime, 0.f, 1.f) : 1.f;
	const float Out = FadeOutTime > KINDA_SMALL_NUMBER ? FMath::Clamp((Lifetime - Age) / FadeOutTime, 0.f, 1.f) : 1.f;
	return In * Out;
}

namespace
{
	float Sech2(float X)
	{
		const float Clamped = FMath::Clamp(X, -20.f, 20.f);
		const float Sech = 2.f / (FMath::Exp(Clamped) + FMath::Exp(-Clamped));
		return Sech * Sech;
	}

	float Gauss(float X)
	{
		return FMath::Exp(-X * X);
	}

	float SmoothStep01(float Edge0, float Edge1, float X)
	{
		return FMath::SmoothStep(Edge0, Edge1, X);
	}

	/** Standard-Seegang, falls DT_Seegang fehlt. Beaufort, Wind m/s, Hs m, Wellenlaenge m, Steilheit. */
	const float GDefaultSeaStates[13][5] =
	{
		{ 0,  0.2f,  0.02f,   6.f, 0.10f },
		{ 1,  1.0f,  0.10f,  10.f, 0.15f },
		{ 2,  2.5f,  0.25f,  14.f, 0.20f },
		{ 3,  4.4f,  0.60f,  22.f, 0.25f },
		{ 4,  6.7f,  1.00f,  32.f, 0.30f },
		{ 5,  9.3f,  2.00f,  50.f, 0.35f },
		{ 6, 12.3f,  3.00f,  70.f, 0.45f },
		{ 7, 15.5f,  4.00f,  90.f, 0.50f },
		{ 8, 18.9f,  5.50f, 115.f, 0.55f },
		{ 9, 22.6f,  7.00f, 140.f, 0.60f },
		{ 10, 26.4f, 9.00f, 170.f, 0.65f },
		{ 11, 30.5f, 11.5f, 210.f, 0.70f },
		{ 12, 34.0f, 14.0f, 260.f, 0.75f },
	};
}

// ---------------------------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------------------------

void UEisenwalOceanSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Verteilung der 8 Grundwellen um die dominante Wellenlaenge/Windrichtung.
	const float Multipliers[NumSwellWaves] = { 1.0f, 0.78f, 1.3f, 0.6f, 0.45f, 1.65f, 0.33f, 0.24f };
	const float DirOffsets[NumSwellWaves] = { 0.f, 22.f, -17.f, -35.f, 40.f, 9.f, -55.f, 65.f };
	const float Weights[NumSwellWaves] = { 0.55f, 0.42f, 0.38f, 0.33f, 0.28f, 0.22f, 0.20f, 0.16f };

	float WeightSq = 0.f;
	for (int32 i = 0; i < NumSwellWaves; ++i)
	{
		WeightSq += Weights[i] * Weights[i];
	}
	const float Norm = 1.f / FMath::Sqrt(WeightSq);

	FRandomStream Rng(1337);
	for (int32 i = 0; i < NumSwellWaves; ++i)
	{
		Slots[i].WavelengthMultiplier = Multipliers[i];
		Slots[i].DirectionOffsetDeg = DirOffsets[i];
		Slots[i].AmplitudeWeight = Weights[i] * Norm;
		SwellWaves[i].Phase = Rng.FRandRange(0.f, 2.f * PI);
	}

	RebuildSeaStateTable();
	UpdateSwellWaves(0.f);
}

bool UEisenwalOceanSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

TStatId UEisenwalOceanSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEisenwalOceanSubsystem, STATGROUP_Tickables);
}

void UEisenwalOceanSubsystem::RebuildSeaStateTable()
{
	SeaStates.Reset();

	if (UDataTable* Table = UEisenwalSettings::LoadTable(UEisenwalSettings::Get()->SeaStateTable))
	{
		TArray<FEisenwalSeaStateRow*> Rows;
		Table->GetAllRows<FEisenwalSeaStateRow>(TEXT("Seegang"), Rows);
		for (const FEisenwalSeaStateRow* Row : Rows)
		{
			if (Row)
			{
				SeaStates.Add(*Row);
			}
		}
		SeaStates.Sort([](const FEisenwalSeaStateRow& A, const FEisenwalSeaStateRow& B) { return A.Beaufort < B.Beaufort; });
	}

	if (SeaStates.Num() < 2)
	{
		SeaStates.Reset();
		for (const auto& D : GDefaultSeaStates)
		{
			FEisenwalSeaStateRow Row;
			Row.Beaufort = static_cast<int32>(D[0]);
			Row.WindSpeedMS = D[1];
			Row.SignificantWaveHeightM = D[2];
			Row.PeakWavelengthM = D[3];
			Row.Steepness = D[4];
			SeaStates.Add(Row);
		}
	}
}

void UEisenwalOceanSubsystem::GetSeaStateForBeaufort(float Beaufort, float& OutHsCm, float& OutPeakWavelengthCm, float& OutSteepness) const
{
	const FEisenwalSeaStateRow* Lower = &SeaStates[0];
	const FEisenwalSeaStateRow* Upper = &SeaStates.Last();
	for (int32 i = 0; i + 1 < SeaStates.Num(); ++i)
	{
		if (Beaufort >= SeaStates[i].Beaufort && Beaufort <= SeaStates[i + 1].Beaufort)
		{
			Lower = &SeaStates[i];
			Upper = &SeaStates[i + 1];
			break;
		}
	}
	const float Span = FMath::Max(1.f, static_cast<float>(Upper->Beaufort - Lower->Beaufort));
	const float Alpha = FMath::Clamp((Beaufort - Lower->Beaufort) / Span, 0.f, 1.f);

	OutHsCm = FMath::Lerp(Lower->SignificantWaveHeightM, Upper->SignificantWaveHeightM, Alpha) * 100.f;
	OutPeakWavelengthCm = FMath::Lerp(Lower->PeakWavelengthM, Upper->PeakWavelengthM, Alpha) * 100.f;
	OutSteepness = FMath::Lerp(Lower->Steepness, Upper->Steepness, Alpha);
}

void UEisenwalOceanSubsystem::SetTargetSeaState(float Beaufort, float WindDirectionDeg, float TransitionSeconds)
{
	TargetBeaufort = FMath::Clamp(Beaufort, 0.f, 12.f);
	TargetWindDirDeg = FRotator::NormalizeAxis(WindDirectionDeg);
	const float Seconds = FMath::Max(TransitionSeconds, 0.01f);
	BeaufortRate = FMath::Abs(TargetBeaufort - CurrentBeaufort) / Seconds;
	WindDirRate = FMath::Abs(FRotator::NormalizeAxis(TargetWindDirDeg - CurrentWindDirDeg)) / Seconds;
}

void UEisenwalOceanSubsystem::UpdateSwellWaves(float DeltaTime)
{
	// Weicher Uebergang zum Ziel-Seegang: Staerke und Richtung kommen gleichzeitig an.
	CurrentBeaufort = FMath::FInterpConstantTo(CurrentBeaufort, TargetBeaufort, DeltaTime, BeaufortRate);
	const float DirDelta = FRotator::NormalizeAxis(TargetWindDirDeg - CurrentWindDirDeg);
	const float DirStep = FMath::Min(FMath::Abs(DirDelta), WindDirRate * DeltaTime);
	CurrentWindDirDeg = FRotator::NormalizeAxis(CurrentWindDirDeg + FMath::Sign(DirDelta) * DirStep);

	float PeakWavelengthCm = 1000.f;
	GetSeaStateForBeaufort(CurrentBeaufort, CurrentHsCm, PeakWavelengthCm, CurrentSteepness);

	const float AmplitudeScale = CurrentHsCm / (2.f * UE_SQRT_2);
	for (int32 i = 0; i < NumSwellWaves; ++i)
	{
		FEisenwalGerstnerWave& W = SwellWaves[i];
		const float DirRad = FMath::DegreesToRadians(CurrentWindDirDeg + Slots[i].DirectionOffsetDeg);
		W.Direction = FVector2D(FMath::Cos(DirRad), FMath::Sin(DirRad));
		W.WavelengthCm = FMath::Max(PeakWavelengthCm * Slots[i].WavelengthMultiplier, 150.f);
		W.AmplitudeCm = Slots[i].AmplitudeWeight * AmplitudeScale;

		const float K = W.GetWaveNumber();
		const float Denominator = K * FMath::Max(W.AmplitudeCm, 0.01f) * NumSwellWaves;
		W.Steepness = FMath::Clamp(CurrentSteepness / Denominator, 0.f, 1.f);

		W.Phase = FMath::Fmod(W.Phase + W.GetAngularFrequency() * DeltaTime, 2.f * PI);
	}
}

void UEisenwalOceanSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DeltaTime = FMath::Min(DeltaTime, 0.1f);
	OceanTime += DeltaTime;
	UpdateSwellWaves(DeltaTime);

	for (int32 i = EventWaves.Num() - 1; i >= 0; --i)
	{
		FEisenwalEventWave& W = EventWaves[i];
		W.Age += DeltaTime;
		W.Distance += W.SpeedCmPerSec * DeltaTime;
		if (W.Age >= W.Lifetime)
		{
			EventWaves.RemoveAtSwap(i);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Grundseegang (Gerstner)
// ---------------------------------------------------------------------------------------------

FVector2D UEisenwalOceanSubsystem::GerstnerHorizontal(const FVector2D& RestPoint, float MinWavelengthCm) const
{
	FVector2D Offset = FVector2D::ZeroVector;
	for (const FEisenwalGerstnerWave& W : SwellWaves)
	{
		if (W.WavelengthCm < MinWavelengthCm || W.AmplitudeCm < 0.5f)
		{
			continue;
		}
		const float Theta = W.GetWaveNumber() * FVector2D::DotProduct(W.Direction, RestPoint) - W.Phase;
		Offset += W.Direction * (W.Steepness * W.AmplitudeCm * FMath::Cos(Theta));
	}
	return Offset;
}

float UEisenwalOceanSubsystem::GerstnerHeightAtRest(const FVector2D& RestPoint, float MinWavelengthCm, FVector2D* OutOrbitalVelocity) const
{
	float Height = 0.f;
	FVector2D Velocity = FVector2D::ZeroVector;
	for (const FEisenwalGerstnerWave& W : SwellWaves)
	{
		if (W.WavelengthCm < MinWavelengthCm || W.AmplitudeCm < 0.5f)
		{
			continue;
		}
		const float Theta = W.GetWaveNumber() * FVector2D::DotProduct(W.Direction, RestPoint) - W.Phase;
		float S, C;
		FMath::SinCos(&S, &C, Theta);
		Height += W.AmplitudeCm * S;
		if (OutOrbitalVelocity)
		{
			Velocity += W.Direction * (W.AmplitudeCm * W.GetAngularFrequency() * S);
		}
	}
	if (OutOrbitalVelocity)
	{
		*OutOrbitalVelocity = Velocity;
	}
	return Height;
}

// ---------------------------------------------------------------------------------------------
// Einzelwellen, Strudel, Gasblasen
// ---------------------------------------------------------------------------------------------

float UEisenwalOceanSubsystem::EventWaveProfile(float S, float Width, float TroughFactor, float& OutCrestWeight) const
{
	// S > 0: vor dem Kamm (noch nicht angekommen), S < 0: dahinter.
	const float W = FMath::Max(Width, 100.f);
	OutCrestWeight = Sech2(S / W);
	const float Lead = -TroughFactor * Gauss((S - 2.2f * W) / (1.3f * W));
	const float Tail = -0.25f * Gauss((S + 2.5f * W) / (1.6f * W));
	return OutCrestWeight + Lead + Tail;
}

float UEisenwalOceanSubsystem::EvaluateEventHeight(const FVector2D& P, FVector2D* OutVelocity, float* OutBuoyancy, float* OutFoam) const
{
	float Height = 0.f;
	FVector2D Velocity = FVector2D::ZeroVector;
	float Buoyancy = 1.f;
	float Foam = 0.f;

	// Einzelwellen
	for (const FEisenwalEventWave& Wave : EventWaves)
	{
		const float Intensity = Wave.GetIntensity();
		if (Intensity <= 0.f)
		{
			continue;
		}

		FVector2D Dir;
		float S;
		float Lateral = 1.f;
		float Decay = 1.f;
		if (Wave.Shape == EEisenwalWaveFrontShape::Line)
		{
			Dir = Wave.Direction.GetSafeNormal();
			const FVector2D Rel = P - Wave.Origin;
			S = static_cast<float>(FVector2D::DotProduct(Rel, Dir) - Wave.Distance);
			if (Wave.LengthCm > 0.f)
			{
				const float Half = Wave.LengthCm * 0.5f;
				const float Side = FMath::Abs(static_cast<float>(FVector2D::CrossProduct(Dir, Rel)));
				Lateral = 1.f - SmoothStep01(Half * 0.6f, Half, Side);
			}
		}
		else
		{
			const FVector2D Rel = P - Wave.Origin;
			const float R = static_cast<float>(Rel.Size());
			Dir = R > 1.f ? Rel / R : FVector2D(1.f, 0.f);
			S = R - static_cast<float>(Wave.Distance);
			const float Ref = Wave.WidthCm * 4.f;
			Decay = FMath::Sqrt(Ref / FMath::Max(static_cast<float>(Wave.Distance), Ref));
		}

		if (Lateral <= 0.f || FMath::Abs(S) > Wave.WidthCm * 8.f)
		{
			continue;
		}

		float Shoal = 1.f;
		if (Wave.bShoaling && ShallowAreas.Num() > 0)
		{
			const float Depth = FMath::Max(GetSeabedDepth(P), 1000.f);
			Shoal = FMath::Clamp(FMath::Pow(ShoalingReferenceDepthCm / Depth, 0.25f), 1.f, MaxShoalingFactor);
		}

		float CrestWeight = 0.f;
		const float Profile = EventWaveProfile(S, Wave.WidthCm, Wave.LeadingTroughFactor, CrestWeight);
		const float Scale = Wave.HeightCm * Intensity * Lateral * Decay * Shoal;
		Height += Profile * Scale;

		// Kamm reisst Wasser mit, im Vorlauf-Tal stroemt es zurueck zur Welle ("Wasser zieht sich zurueck").
		const float LeadWeight = Gauss((S - 2.2f * Wave.WidthCm) / (1.3f * Wave.WidthCm));
		const float Surge = Wave.SpeedCmPerSec * Wave.SurgeFactor * Intensity * Lateral * Decay;
		Velocity += Dir * Surge * (CrestWeight * FMath::Min(Shoal, 2.f) - 0.3f * LeadWeight);

		Foam = FMath::Max(Foam, CrestWeight * FMath::Clamp(Scale / 600.f, 0.f, 1.f) * (S > 0.f ? 1.f : 0.6f));
	}

	// Strudel
	for (const FEisenwalWhirlpool& Pool : Whirlpools)
	{
		if (Pool.Intensity <= 0.f)
		{
			continue;
		}
		const FVector2D Rel = P - Pool.Center;
		const float R = static_cast<float>(Rel.Size());
		if (R >= Pool.RadiusCm)
		{
			continue;
		}
		const float Falloff = 1.f - SmoothStep01(Pool.RadiusCm * 0.55f, Pool.RadiusCm, R);
		const float Core = FMath::Max(Pool.CoreRadiusCm, 100.f);
		const float RelR = R / Core;
		const float I = Pool.Intensity * Falloff;

		// Trichter + Spiralarme
		const float Funnel = -Pool.DepthCm / (1.f + RelR * RelR);
		const float Angle = FMath::Atan2(Rel.Y, Rel.X) * (Pool.bClockwise ? -1.f : 1.f);
		const float Spiral = FMath::Sin(3.f * Angle + R / 1500.f - OceanTime * 1.7f) * Pool.DepthCm * 0.06f * SmoothStep01(Core * 0.5f, Core * 2.f, R);
		Height += (Funnel + Spiral) * I;

		if (R > 1.f)
		{
			const FVector2D Radial = Rel / R;
			FVector2D Tangent(-Radial.Y, Radial.X);
			if (Pool.bClockwise)
			{
				Tangent = -Tangent;
			}
			const float Vt = Pool.MaxTangentialSpeedCmPerSec * (R < Core ? RelR : 1.f / RelR);
			Velocity += (Tangent * Vt - Radial * Pool.InwardSpeedCmPerSec) * I;
		}
		Foam = FMath::Max(Foam, 0.7f * I * (1.f - SmoothStep01(Core * 0.5f, Core * 3.f, R)));
	}

	// Gasblasen (Seebeben)
	for (const FEisenwalAerationZone& Zone : AerationZones)
	{
		if (Zone.Intensity <= 0.f)
		{
			continue;
		}
		const float D = static_cast<float>((P - Zone.Center).Size()) / FMath::Max(Zone.RadiusCm, 1.f);
		if (D >= 1.f)
		{
			continue;
		}
		const float W = FMath::Square(1.f - D * D) * Zone.Intensity;
		Buoyancy *= FMath::Lerp(1.f, Zone.BuoyancyMultiplier, W);
		const float Boil = FMath::Sin(P.X * 0.004f + OceanTime * 2.3f) * FMath::Sin(P.Y * 0.0037f - OceanTime * 1.9f)
			+ 0.5f * FMath::Sin(P.X * 0.011f - P.Y * 0.009f + OceanTime * 4.1f);
		Height += Boil * Zone.BoilHeightCm * W;
		Foam = FMath::Max(Foam, 0.85f * W);
	}

	if (OutVelocity) { *OutVelocity = Velocity; }
	if (OutBuoyancy) { *OutBuoyancy = Buoyancy; }
	if (OutFoam) { *OutFoam = Foam; }
	return Height;
}

// ---------------------------------------------------------------------------------------------
// Abfragen
// ---------------------------------------------------------------------------------------------

float UEisenwalOceanSubsystem::GetSeabedDepth(const FVector2D& Location) const
{
	float Depth = DeepWaterDepthCm;
	for (const FEisenwalShallowArea& Area : ShallowAreas)
	{
		const float Dist = static_cast<float>((Location - Area.Center).Size());
		if (Dist <= Area.ShoreRadiusCm)
		{
			return 0.f;
		}
		const float T = FMath::Clamp((Dist - Area.ShoreRadiusCm) / FMath::Max(Area.ShelfWidthCm, 1.f), 0.f, 1.f);
		const float AreaDepth = FMath::Lerp(300.f, DeepWaterDepthCm, T * T * T);
		Depth = FMath::Min(Depth, AreaDepth);
	}
	return Depth;
}

FEisenwalWaterSample UEisenwalOceanSubsystem::SampleWater(const FVector& WorldLocation, bool bComputeNormal) const
{
	FEisenwalWaterSample Result;
	const FVector2D P(WorldLocation.X, WorldLocation.Y);

	// Gerstner invertieren: welcher Ruhepunkt landet nach der Verschiebung bei P?
	FVector2D Rest = P;
	for (int32 Iter = 0; Iter < 3; ++Iter)
	{
		Rest = P - GerstnerHorizontal(Rest, 0.f);
	}

	FVector2D Orbital;
	const float SwellHeight = GerstnerHeightAtRest(Rest, 0.f, &Orbital);

	FVector2D EventVelocity;
	float Buoyancy = 1.f;
	const float EventHeight = EvaluateEventHeight(P, &EventVelocity, &Buoyancy, nullptr);

	Result.Height = SwellHeight + EventHeight;
	Result.Velocity = FVector(Orbital + EventVelocity, 0.f);
	Result.BuoyancyMultiplier = Buoyancy;
	Result.Depth = GetSeabedDepth(P);

	if (bComputeNormal)
	{
		constexpr float Delta = 150.f;
		const float Hx = GetWaterHeight(WorldLocation + FVector(Delta, 0.f, 0.f));
		const float Hy = GetWaterHeight(WorldLocation + FVector(0.f, Delta, 0.f));
		Result.Normal = FVector(-(Hx - Result.Height), -(Hy - Result.Height), Delta).GetSafeNormal();
	}
	return Result;
}

float UEisenwalOceanSubsystem::GetWaterHeight(const FVector& WorldLocation) const
{
	const FVector2D P(WorldLocation.X, WorldLocation.Y);
	FVector2D Rest = P;
	for (int32 Iter = 0; Iter < 3; ++Iter)
	{
		Rest = P - GerstnerHorizontal(Rest, 0.f);
	}
	return GerstnerHeightAtRest(Rest, 0.f, nullptr) + EvaluateEventHeight(P, nullptr, nullptr, nullptr);
}

FVector UEisenwalOceanSubsystem::DisplaceGridPoint(const FVector2D& GridPoint, float MinWavelengthCm, float& OutFoam, float& OutCrest) const
{
	const FVector2D Horizontal = GerstnerHorizontal(GridPoint, MinWavelengthCm);
	const float SwellHeight = GerstnerHeightAtRest(GridPoint, MinWavelengthCm, nullptr);
	const FVector2D Displaced = GridPoint + Horizontal;

	float EventFoam = 0.f;
	const float EventHeight = EvaluateEventHeight(Displaced, nullptr, nullptr, &EventFoam);

	// Schaum auf hohen, spitzen Kaemmen des Grundseegangs.
	const float Hs = FMath::Max(CurrentHsCm, 1.f);
	const float Crest = FMath::Clamp((SwellHeight / Hs) * 0.8f + 0.5f, 0.f, 1.f);
	const float SwellFoam = FMath::Clamp((SwellHeight / Hs - 0.35f) * 2.5f, 0.f, 1.f) * FMath::Clamp((CurrentBeaufort - 3.f) / 5.f, 0.f, 1.f);

	OutFoam = FMath::Max(SwellFoam, EventFoam);
	OutCrest = FMath::Max(Crest, FMath::Clamp(EventHeight / 1500.f, 0.f, 1.f));
	return FVector(Displaced.X, Displaced.Y, SwellHeight + EventHeight);
}

// ---------------------------------------------------------------------------------------------
// Registrierung
// ---------------------------------------------------------------------------------------------

int32 UEisenwalOceanSubsystem::AddEventWave(const FEisenwalEventWave& Wave)
{
	FEisenwalEventWave& Added = EventWaves.Add_GetRef(Wave);
	Added.Id = NextId++;
	Added.Age = 0.f;
	Added.Direction = Added.Direction.GetSafeNormal();
	return Added.Id;
}

bool UEisenwalOceanSubsystem::GetEventWave(int32 Id, FEisenwalEventWave& OutWave) const
{
	if (const FEisenwalEventWave* Found = EventWaves.FindByPredicate([Id](const FEisenwalEventWave& W) { return W.Id == Id; }))
	{
		OutWave = *Found;
		return true;
	}
	return false;
}

void UEisenwalOceanSubsystem::RemoveEventWave(int32 Id)
{
	EventWaves.RemoveAllSwap([Id](const FEisenwalEventWave& W) { return W.Id == Id; });
}

FVector2D UEisenwalOceanSubsystem::GetEventWaveFrontPoint(int32 Id) const
{
	FEisenwalEventWave W;
	if (!GetEventWave(Id, W))
	{
		return FVector2D::ZeroVector;
	}
	return W.Shape == EEisenwalWaveFrontShape::Line ? W.Origin + W.Direction * W.Distance : W.Origin;
}

int32 UEisenwalOceanSubsystem::AddWhirlpool(const FEisenwalWhirlpool& Whirlpool)
{
	FEisenwalWhirlpool& Added = Whirlpools.Add_GetRef(Whirlpool);
	Added.Id = NextId++;
	return Added.Id;
}

void UEisenwalOceanSubsystem::UpdateWhirlpool(int32 Id, const FEisenwalWhirlpool& Whirlpool)
{
	if (FEisenwalWhirlpool* Found = Whirlpools.FindByPredicate([Id](const FEisenwalWhirlpool& W) { return W.Id == Id; }))
	{
		*Found = Whirlpool;
		Found->Id = Id;
	}
}

void UEisenwalOceanSubsystem::RemoveWhirlpool(int32 Id)
{
	Whirlpools.RemoveAllSwap([Id](const FEisenwalWhirlpool& W) { return W.Id == Id; });
}

int32 UEisenwalOceanSubsystem::AddAerationZone(const FEisenwalAerationZone& Zone)
{
	FEisenwalAerationZone& Added = AerationZones.Add_GetRef(Zone);
	Added.Id = NextId++;
	return Added.Id;
}

void UEisenwalOceanSubsystem::UpdateAerationZone(int32 Id, const FEisenwalAerationZone& Zone)
{
	if (FEisenwalAerationZone* Found = AerationZones.FindByPredicate([Id](const FEisenwalAerationZone& Z) { return Z.Id == Id; }))
	{
		*Found = Zone;
		Found->Id = Id;
	}
}

void UEisenwalOceanSubsystem::RemoveAerationZone(int32 Id)
{
	AerationZones.RemoveAllSwap([Id](const FEisenwalAerationZone& Z) { return Z.Id == Id; });
}

int32 UEisenwalOceanSubsystem::AddShallowArea(const FEisenwalShallowArea& Area)
{
	FEisenwalShallowArea& Added = ShallowAreas.Add_GetRef(Area);
	Added.Id = NextId++;
	return Added.Id;
}

void UEisenwalOceanSubsystem::RemoveShallowArea(int32 Id)
{
	ShallowAreas.RemoveAllSwap([Id](const FEisenwalShallowArea& A) { return A.Id == Id; });
}
