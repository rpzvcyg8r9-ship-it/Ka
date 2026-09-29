#include "World/EisenwalSky.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEisenwalSky::AEisenwalSky()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Sonne: Atmosphaeren-Licht 0
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sonne"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetAtmosphereSunLightIndex(0);
	Sun->SetIntensity(SunIntensityLux);
	Sun->SetDynamicShadowDistanceMovableLight(20000.f);
	Sun->SetUseTemperature(true);
	Sun->SetTemperature(5900.f);

	// Mond: Atmosphaeren-Licht 1 (zeichnet die Mondscheibe in den Himmel)
	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Mond"));
	Moon->SetupAttachment(Root);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetAtmosphereSunLight(true);
	Moon->SetAtmosphereSunLightIndex(1);
	Moon->SetIntensity(0.f);
	Moon->SetLightColor(FLinearColor(0.55f, 0.65f, 1.f));
	Moon->SetCastShadows(false);
	Moon->SetLightSourceAngle(0.55f);
	Moon->SetAtmosphereSunDiskColorScale(FLinearColor(40.f, 40.f, 40.f));

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphaere"));
	Atmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("Himmelslicht"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Wolken"));
	Clouds->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudMaterial(
		TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
	if (CloudMaterial.Succeeded())
	{
		CloudBaseMaterial = CloudMaterial.Object;
		Clouds->SetMaterial(CloudMaterial.Object);
	}
	Clouds->SetLayerBottomAltitude(1.2f);
	Clouds->SetLayerHeight(6.f);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Nebel"));
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.004f);
	Fog->SetFogHeightFalloff(0.08f);
	Fog->SetVolumetricFog(true);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Farbstimmung"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;
	FPostProcessSettings& PP = PostProcess->Settings;
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = -3.f;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = 16.f;
	PP.bOverride_AutoExposureSpeedUp = true;
	PP.AutoExposureSpeedUp = 2.f;
	PP.bOverride_AutoExposureSpeedDown = true;
	PP.AutoExposureSpeedDown = 1.f;

	StarDome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sternenkuppel"));
	StarDome->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		StarDome->SetStaticMesh(SphereMesh.Object);
	}
	StarDome->SetRelativeScale3D(FVector(20000.f)); // 50 cm * 20000 = 10 km Radius
	StarDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StarDome->SetCastShadow(false);
	StarDome->bAffectDistanceFieldLighting = false;
	StarDome->SetVisibleInRayTracing(false);
	StarDome->SetVisibility(false);
}

void AEisenwalSky::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureMaterials();
	SetCelestial(EditorSunElevation, EditorSunAzimuth, -EditorSunElevation, EditorSunAzimuth + 180.f, 0.5f);
	ApplyLook(CurrentLook);
}

void AEisenwalSky::BeginPlay()
{
	Super::BeginPlay();
	EnsureMaterials();
}

void AEisenwalSky::EnsureMaterials()
{
	if (!CloudMID && CloudBaseMaterial)
	{
		CloudMID = UMaterialInstanceDynamic::Create(CloudBaseMaterial, this);
		CloudMID->SetFlags(RF_Transient);
		Clouds->SetMaterial(CloudMID);
	}

	if (!StarMID)
	{
		// M_Sterne wird vom Setup-Skript erzeugt; ohne Material bleibt die Kuppel unsichtbar.
		if (UMaterialInterface* StarMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Eisenwal/Materials/M_Sterne.M_Sterne")))
		{
			StarMID = UMaterialInstanceDynamic::Create(StarMaterial, this);
			StarMID->SetFlags(RF_Transient);
			StarDome->SetMaterial(0, StarMID);
			StarDome->SetVisibility(true);
		}
	}
}

float AEisenwalSky::GetDaylight() const
{
	return FMath::SmoothStep(-6.f, 8.f, SunElevation);
}

void AEisenwalSky::SetCelestial(float SunElevationDeg, float SunAzimuthDeg, float MoonElevationDeg, float MoonAzimuthDeg, float MoonPhase)
{
	SunElevation = SunElevationDeg;
	MoonElevation = MoonElevationDeg;
	MoonPhaseValue = MoonPhase;

	// Das Licht zeigt vom Himmelskoerper weg (Rotation = Laufrichtung des Lichts).
	Sun->SetWorldRotation(FRotator(-SunElevationDeg, SunAzimuthDeg + 180.f, 0.f));
	Moon->SetWorldRotation(FRotator(-MoonElevationDeg, MoonAzimuthDeg + 180.f, 0.f));

	ApplyLook(CurrentLook);
}

void AEisenwalSky::ApplyLook(const FEisenwalSkyLook& Look)
{
	CurrentLook = Look;
	const float Daylight = GetDaylight();

	// Sonne verschwindet unter dem Horizont und hinter dichten Wolken.
	const float SunAboveHorizon = FMath::SmoothStep(-4.f, 6.f, SunElevation);
	Sun->SetIntensity(SunIntensityLux * SunAboveHorizon * FMath::Lerp(0.15f, 1.f, Look.SunVisibility));

	// Mondhelligkeit nach Phase: 0 = Neumond, 0.5 = Vollmond.
	const float PhaseBrightness = 0.5f - 0.5f * FMath::Cos(2.f * PI * MoonPhaseValue);
	const float MoonUp = FMath::SmoothStep(-2.f, 5.f, MoonElevation);
	Moon->SetIntensity(MoonIntensityLux * PhaseBrightness * MoonUp * (1.f - Daylight) * FMath::Lerp(0.2f, 1.f, 1.f - Look.Cloudiness));

	Fog->SetFogDensity(Look.FogDensity);
	Fog->SetFogInscatteringColor(Look.FogColor * FMath::Lerp(0.03f, 1.f, Daylight));

	if (CloudMID)
	{
		// Parameternamen des Engine-Wolkenmaterials bzw. unseres eigenen M_Wolken.
		CloudMID->SetScalarParameterValue(TEXT("Wolkendichte"), Look.Cloudiness);
		CloudMID->SetScalarParameterValue(TEXT("CloudCoverage"), Look.Cloudiness);
	}

	if (StarMID)
	{
		StarMID->SetScalarParameterValue(TEXT("Helligkeit"), (1.f - Daylight) * (1.f - Look.Cloudiness) * 4.f);
	}

	FPostProcessSettings& PP = PostProcess->Settings;
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(Look.Saturation, Look.Saturation, Look.Saturation, 1.f);
	PP.bOverride_ColorGain = true;
	PP.ColorGain = FVector4(Look.GradeTint.R, Look.GradeTint.G, Look.GradeTint.B, 1.f);

	if (LightningStrength <= 0.f)
	{
		SkyLight->SetIntensity(1.f);
	}
}

void AEisenwalSky::FlashLightning(float Strength)
{
	LightningStrength = FMath::Max(LightningStrength, Strength);
	LightningTime = 0.f;
}

void AEisenwalSky::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (LightningStrength > 0.f)
	{
		LightningTime += DeltaSeconds;
		// Doppelblitz: zwei kurze Spitzen, dann Abklingen.
		const float Flicker = (LightningTime < 0.08f || (LightningTime > 0.16f && LightningTime < 0.24f)) ? 1.f : 0.25f;
		const float Envelope = FMath::Clamp(1.f - LightningTime / 0.6f, 0.f, 1.f);
		SkyLight->SetIntensity(1.f + 25.f * LightningStrength * Flicker * Envelope);
		if (Envelope <= 0.f)
		{
			LightningStrength = 0.f;
			SkyLight->SetIntensity(1.f);
		}
	}
}
