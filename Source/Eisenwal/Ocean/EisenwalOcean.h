#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "EisenwalOcean.generated.h"

class UMaterialInterface;
class UEisenwalOceanSubsystem;

/** Ein Detailring des Ozean-Gitters. */
USTRUCT(BlueprintType)
struct FEisenwalOceanLOD
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ozean")
	float CellSizeCm = 400.f;

	/** Anzahl Zellen pro Seite (gerade Zahl). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ozean")
	int32 CellsPerSide = 160;
};

/**
 * Sichtbare Ozeanoberflaeche. Ein Gitter mit mehreren Detailringen folgt der Kamera;
 * jeder Punkt wird ueber UEisenwalOceanSubsystem verschoben - also exakt dieselbe Oberflaeche,
 * auf der das Schiff schwimmt (inkl. Tsunami, Monsterwelle, Strudel).
 */
UCLASS()
class EISENWAL_API AEisenwalOcean : public AActor
{
	GENERATED_BODY()

public:
	AEisenwalOcean();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	/** Detailringe von innen nach aussen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ozean")
	TArray<FEisenwalOceanLOD> LODs;

	/** Wellen kuerzer als CellSize * Faktor werden im jeweiligen Ring weggelassen (gegen Flimmern). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ozean")
	float MinWavelengthCellRatio = 2.5f;

	/** Aeussere Ringe liegen minimal tiefer, damit sie nicht durch innere Ringe stechen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ozean")
	float LODDepthOffsetCm = 25.f;

	/** Ueberschreibt das Material aus den Projekteinstellungen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ozean")
	TObjectPtr<UMaterialInterface> OceanMaterialOverride;

	/** Falls das Wasser nur von unten sichtbar ist: Dreiecke umdrehen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ozean")
	bool bFlipWinding = false;

	/** Im Editor mitanimieren (kostet Leistung im Viewport). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ozean")
	bool bAnimateInEditor = true;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ozean")
	TObjectPtr<UProceduralMeshComponent> Mesh;

private:
	struct FLODState
	{
		FVector2D Center = FVector2D(TNumericLimits<double>::Max(), 0.0);
		FIntPoint HoleKey = FIntPoint(TNumericLimits<int32>::Max(), 0);
		bool bSectionCreated = false;
		TArray<FVector> Vertices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Triangles;
	};

	void ApplyMaterial();
	void UpdateSurface();
	void UpdateLOD(int32 Index, const FVector& ViewLocation, const UEisenwalOceanSubsystem& Ocean);
	FVector GetViewLocation() const;

	TArray<FLODState> States;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ActiveMaterial;
};
