#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EisenwalSky.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/** Optik-Zustand, den Wetter + Tageszeit auf den Himmel anwenden. */
USTRUCT(BlueprintType)
struct FEisenwalSkyLook
{
	GENERATED_BODY()

	/** 0 = klar, 1 = komplett zugezogen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himmel")
	float Cloudiness = 0.3f;

	/** Dichte des Hoehennebels (Regen/Nebel/Asche). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himmel")
	float FogDensity = 0.004f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himmel")
	FLinearColor FogColor = FLinearColor(0.45f, 0.55f, 0.65f);

	/** Farbstimmung (klar = blau, Sturm = grau-gruen, Vulkan = orange-rot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himmel")
	FLinearColor GradeTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himmel")
	float Saturation = 1.f;

	/** Wie viel Sonne durch die Wolken kommt (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himmel")
	float SunVisibility = 1.f;
};

/**
 * Himmel, Sonne, Mond, Sterne, Wolken, Nebel und Farbstimmung in einem Actor.
 * Das Tag/Nacht- und Wettersystem steuert ihn ueber SetCelestial() und ApplyLook().
 */
UCLASS()
class EISENWAL_API AEisenwalSky : public AActor
{
	GENERATED_BODY()

public:
	AEisenwalSky();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Sonne und Mond positionieren (Grad). MoonPhase 0 = Neumond, 0.5 = Vollmond. */
	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Himmel")
	void SetCelestial(float SunElevationDeg, float SunAzimuthDeg, float MoonElevationDeg, float MoonAzimuthDeg, float MoonPhase);

	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Himmel")
	void ApplyLook(const FEisenwalSkyLook& Look);

	/** Kurzer Lichtblitz (Gewitter). */
	UFUNCTION(BlueprintCallable, Category = "Eisenwal|Himmel")
	void FlashLightning(float Strength = 1.f);

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Eisenwal|Himmel")
	float GetSunElevation() const { return SunElevation; }

	/** Tageslicht 0 (Nacht) .. 1 (voller Tag). */
	UFUNCTION(BlueprintPure, Category = "Eisenwal|Himmel")
	float GetDaylight() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himmel")
	float SunIntensityLux = 10.f;

	/** Mondlicht bei Vollmond. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himmel")
	float MoonIntensityLux = 0.25f;

	/** Startwerte im Editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himmel")
	float EditorSunElevation = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himmel")
	float EditorSunAzimuth = 120.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Himmel")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Himmel")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Himmel")
	TObjectPtr<UDirectionalLightComponent> Moon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Himmel")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Himmel")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Himmel")
	TObjectPtr<UVolumetricCloudComponent> Clouds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Himmel")
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Himmel")
	TObjectPtr<UPostProcessComponent> PostProcess;

	/** Riesige Kugel mit Sternen (nur nachts sichtbar). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Himmel")
	TObjectPtr<UStaticMeshComponent> StarDome;

private:
	void EnsureMaterials();

	UPROPERTY()
	TObjectPtr<UMaterialInterface> CloudBaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CloudMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StarMID;

	float SunElevation = 35.f;
	float MoonElevation = -30.f;
	float MoonPhaseValue = 0.5f;
	float LightningTime = 0.f;
	float LightningStrength = 0.f;
	FEisenwalSkyLook CurrentLook;
};
