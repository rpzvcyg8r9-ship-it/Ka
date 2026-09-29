#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Ocean/EisenwalOceanTypes.h"
#include "EisenwalOceanSubsystem.generated.h"

/**
 * Wellen-Manager: die EINZIGE Quelle fuer die Wasseroberflaeche.
 * Physik (Auftrieb), Grafik (Ozean-Mesh) und Effekte fragen alle hier ab, deshalb passen
 * Schiff und sichtbare Welle immer zusammen - auch bei Tsunami, Monsterwelle und Strudel.
 *
 * Oberflaeche = Grundseegang (Gerstner, vom Wind gesteuert)
 *             + Einzelwellen (Tsunami, Monsterwelle, Ringwellen)
 *             + Strudel-Trichter + brodelnde Gasblasen-Zonen.
 */
UCLASS()
class EISENWAL_API UEisenwalOceanSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 NumSwellWaves = 8;

	// USubsystem / FTickableGameObject
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return true; }

	// ---------------- Abfragen ----------------

	/** Hoehe, Stroemung und Auftriebsfaktor an einem Weltpunkt (Z wird ignoriert). */
	UFUNCTION(BlueprintPure, Category = "Eisenwal|Ozean")
	FEisenwalWaterSample SampleWater(const FVector& WorldLocation, bool bComputeNormal = false) const;

	UFUNCTION(BlueprintPure, Category = "Eisenwal|Ozean")
	float GetWaterHeight(const FVector& WorldLocation) const;

	/** Wassertiefe bis zum Meeresgrund (Tiefwasser = DeepWaterDepthCm). */
	UFUNCTION(BlueprintPure, Category = "Eisenwal|Ozean")
	float GetSeabedDepth(const FVector2D& Location) const;

	/**
	 * Fuer das Ozean-Mesh: verschiebt einen Gitterpunkt wie ein Wasserteilchen (Gerstner vorwaerts).
	 * Wellen kuerzer als MinWavelengthCm werden weggelassen (LOD-Filter gegen Flimmern).
	 * OutFoam 0..1, OutCrest 0..1 (fuer Vertex-Farben).
	 */
	FVector DisplaceGridPoint(const FVector2D& GridPoint, float MinWavelengthCm, float& OutFoam, float& OutCrest) const;

	// ---------------- Seegang ----------------

	/** Setzt den Ziel-Seegang. Der aktuelle Zustand blendet ueber TransitionSeconds weich dorthin. */
	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	void SetTargetSeaState(float Beaufort, float WindDirectionDeg, float TransitionSeconds = 60.f);

	UFUNCTION(BlueprintPure, Category = "Eisenwal|Ozean")
	float GetCurrentBeaufort() const { return CurrentBeaufort; }

	UFUNCTION(BlueprintPure, Category = "Eisenwal|Ozean")
	float GetWindDirectionDeg() const { return CurrentWindDirDeg; }

	/** Signifikante Wellenhoehe des aktuellen Grundseegangs (cm). */
	UFUNCTION(BlueprintPure, Category = "Eisenwal|Ozean")
	float GetSignificantWaveHeightCm() const { return CurrentHsCm; }

	UFUNCTION(BlueprintPure, Category = "Eisenwal|Ozean")
	float GetOceanTime() const { return OceanTime; }

	// ---------------- Einzelwellen / Strudel / Gasblasen / Flachwasser ----------------

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	int32 AddEventWave(const FEisenwalEventWave& Wave);

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	bool GetEventWave(int32 Id, FEisenwalEventWave& OutWave) const;

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	void RemoveEventWave(int32 Id);

	/** Wo steht der Kamm der Welle gerade? (Line: Punkt auf der Front ueber Origin; Ring: Zentrum) */
	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	FVector2D GetEventWaveFrontPoint(int32 Id) const;

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	int32 AddWhirlpool(const FEisenwalWhirlpool& Whirlpool);

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	void UpdateWhirlpool(int32 Id, const FEisenwalWhirlpool& Whirlpool);

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	void RemoveWhirlpool(int32 Id);

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	int32 AddAerationZone(const FEisenwalAerationZone& Zone);

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	void UpdateAerationZone(int32 Id, const FEisenwalAerationZone& Zone);

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	void RemoveAerationZone(int32 Id);

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	int32 AddShallowArea(const FEisenwalShallowArea& Area);

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Ozean")
	void RemoveShallowArea(int32 Id);

	const TArray<FEisenwalEventWave>& GetEventWaves() const { return EventWaves; }
	const TArray<FEisenwalWhirlpool>& GetWhirlpools() const { return Whirlpools; }

	/** Wassertiefe im offenen Ozean (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eisenwal|Ozean")
	float DeepWaterDepthCm = 400000.f;

	/** Referenztiefe fuer Tsunami-Aufsteilung. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eisenwal|Ozean")
	float ShoalingReferenceDepthCm = 400000.f;

	/** Maximaler Aufsteilungsfaktor (40 m Tsunami an der Kueste aus ~10 m im Tiefwasser). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eisenwal|Ozean")
	float MaxShoalingFactor = 4.f;

private:
	struct FSwellSlot
	{
		float WavelengthMultiplier = 1.f;
		float DirectionOffsetDeg = 0.f;
		float AmplitudeWeight = 0.f;
	};

	void RebuildSeaStateTable();
	void GetSeaStateForBeaufort(float Beaufort, float& OutHsCm, float& OutPeakWavelengthCm, float& OutSteepness) const;
	void UpdateSwellWaves(float DeltaTime);

	/** Horizontale Gerstner-Verschiebung eines Ruhepunkts (nur Grundseegang). */
	FVector2D GerstnerHorizontal(const FVector2D& RestPoint, float MinWavelengthCm) const;
	float GerstnerHeightAtRest(const FVector2D& RestPoint, float MinWavelengthCm, FVector2D* OutOrbitalVelocity) const;

	/** Summe der Zusatz-Oberflaechen (Einzelwellen, Strudel, Gasblasen) am Weltpunkt. */
	float EvaluateEventHeight(const FVector2D& P, FVector2D* OutVelocity, float* OutBuoyancy, float* OutFoam) const;

	float EventWaveProfile(float SignedDistance, float Width, float TroughFactor, float& OutCrestWeight) const;

	FSwellSlot Slots[NumSwellWaves];
	FEisenwalGerstnerWave SwellWaves[NumSwellWaves];

	TArray<FEisenwalEventWave> EventWaves;
	TArray<FEisenwalWhirlpool> Whirlpools;
	TArray<FEisenwalAerationZone> AerationZones;
	TArray<FEisenwalShallowArea> ShallowAreas;
	int32 NextId = 1;

	/** Beaufort 0..12 -> Seegang (aus DataTable oder Standardwerten). */
	TArray<FEisenwalSeaStateRow> SeaStates;

	float OceanTime = 0.f;
	float CurrentBeaufort = 3.f;
	float TargetBeaufort = 3.f;
	float CurrentWindDirDeg = 0.f;
	float TargetWindDirDeg = 0.f;
	float BeaufortRate = 0.f;
	float WindDirRate = 0.f;
	float CurrentHsCm = 60.f;
	float CurrentSteepness = 0.3f;
};
