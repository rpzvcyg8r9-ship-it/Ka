#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EisenwalOceanTypes.generated.h"

/** Eine einzelne Gerstner-Welle des Grundseegangs. Alle Laengen in cm. */
USTRUCT(BlueprintType)
struct FEisenwalGerstnerWave
{
	GENERATED_BODY()

	/** Laufrichtung (normiert, XY). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	FVector2D Direction = FVector2D(1.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float WavelengthCm = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float AmplitudeCm = 50.f;

	/** Horizontale Verschiebung (Q), 0 = Sinus, 1 = maximal spitz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float Steepness = 0.5f;

	/** Aufsummierte Phase (rad). Wird vom Ozean-Subsystem pro Tick fortgeschrieben. */
	float Phase = 0.f;

	float GetWaveNumber() const { return 2.f * PI / FMath::Max(WavelengthCm, 1.f); }
	/** Tiefwasser-Dispersion: omega = sqrt(g * k). */
	float GetAngularFrequency() const;
};

UENUM(BlueprintType)
enum class EEisenwalWaveFrontShape : uint8
{
	/** Gerade Wellenfront (Tsunami, Monsterwelle, Erdrutsch). */
	Line,
	/** Ringfoermige Front um einen Punkt (Meteoriteneinschlag, Lavabombe). */
	Ring
};

/**
 * Eine grosse Einzelwelle, die zusaetzlich zum Grundseegang ueber den Ozean laeuft.
 * Profil: Wellental vor der Front ("Wasser zieht sich zurueck"), dann der Kamm (sech^2), dann ein Nachlauf-Tal.
 */
USTRUCT(BlueprintType)
struct FEisenwalEventWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	EEisenwalWaveFrontShape Shape = EEisenwalWaveFrontShape::Line;

	/** Startpunkt (Line: Punkt auf der Startlinie, Ring: Zentrum). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	FVector2D Origin = FVector2D::ZeroVector;

	/** Laufrichtung (nur Line). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	FVector2D Direction = FVector2D(1.f, 0.f);

	/** Bereits zurueckgelegte Strecke der Front (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	double Distance = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float SpeedCmPerSec = 3000.f;

	/** Kammhoehe im Tiefwasser (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float HeightCm = 2000.f;

	/** Halbe Breite des Kamms (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float WidthCm = 4000.f;

	/** Tiefe des Vorlauf-Tals relativ zur Kammhoehe (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float LeadingTroughFactor = 0.35f;

	/** Seitliche Laenge der Front (nur Line, 0 = unendlich). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float LengthCm = 0.f;

	/** Wie stark die Welle Wasser (und damit das Schiff) mitreisst, 0..1. Tsunami hoch, Monsterwelle niedrig. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float SurgeFactor = 0.3f;

	/** Waechst die Welle in flachem Wasser (Green'sches Gesetz)? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	bool bShoaling = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float Lifetime = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float FadeInTime = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Welle")
	float FadeOutTime = 10.f;

	UPROPERTY(BlueprintReadOnly, Category = "Welle")
	float Age = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Welle")
	int32 Id = INDEX_NONE;

	float GetIntensity() const;
};

/** Riesen-Strudel (Rankine-Wirbel mit Trichter). */
USTRUCT(BlueprintType)
struct FEisenwalWhirlpool
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strudel")
	FVector2D Center = FVector2D::ZeroVector;

	/** Aeusserer Einflussradius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strudel")
	float RadiusCm = 60000.f;

	/** Kernradius mit maximaler Drehgeschwindigkeit (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strudel")
	float CoreRadiusCm = 8000.f;

	/** Trichtertiefe im Zentrum (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strudel")
	float DepthCm = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strudel")
	float MaxTangentialSpeedCmPerSec = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strudel")
	float InwardSpeedCmPerSec = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strudel")
	bool bClockwise = false;

	/** 0..1, wird von der Katastrophe gesteuert (Aufbau/Hoehepunkt/Abklingen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strudel")
	float Intensity = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Strudel")
	int32 Id = INDEX_NONE;
};

/** Zone mit Gasblasen (Seebeben): Wasser brodelt und traegt weniger. */
USTRUCT(BlueprintType)
struct FEisenwalAerationZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gasblasen")
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gasblasen")
	float RadiusCm = 30000.f;

	/** Auftrieb im Zentrum bei voller Intensitaet (1 = normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gasblasen")
	float BuoyancyMultiplier = 0.55f;

	/** Hoehe der brodelnden Wasserbuckel (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gasblasen")
	float BoilHeightCm = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gasblasen")
	float Intensity = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Gasblasen")
	int32 Id = INDEX_NONE;
};

/** Flachwasserbereich um eine Insel (fuer Tsunami-Aufsteilung). */
USTRUCT(BlueprintType)
struct FEisenwalShallowArea
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flachwasser")
	FVector2D Center = FVector2D::ZeroVector;

	/** Radius der Kuestenlinie (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flachwasser")
	float ShoreRadiusCm = 50000.f;

	/** Breite des Schelfs, ueber den die Tiefe bis zum Tiefwasser zunimmt (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flachwasser")
	float ShelfWidthCm = 150000.f;

	UPROPERTY(BlueprintReadOnly, Category = "Flachwasser")
	int32 Id = INDEX_NONE;
};

/** Ergebnis einer Wasserabfrage an einem Weltpunkt. */
USTRUCT(BlueprintType)
struct FEisenwalWaterSample
{
	GENERATED_BODY()

	/** Hoehe der Wasseroberflaeche (Welt-Z, cm). */
	UPROPERTY(BlueprintReadOnly, Category = "Wasser")
	float Height = 0.f;

	/** Stroemung/Orbitalgeschwindigkeit an der Oberflaeche (cm/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Wasser")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Wasser")
	FVector Normal = FVector::UpVector;

	/** 1 = normales Wasser, < 1 = Gasblasen (weniger Auftrieb). */
	UPROPERTY(BlueprintReadOnly, Category = "Wasser")
	float BuoyancyMultiplier = 1.f;

	/** Wassertiefe bis zum Grund (cm). */
	UPROPERTY(BlueprintReadOnly, Category = "Wasser")
	float Depth = 400000.f;
};

/** DataTable-Zeile: Seegang je Beaufort-Stufe. */
USTRUCT(BlueprintType)
struct FEisenwalSeaStateRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seegang")
	int32 Beaufort = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seegang")
	float WindSpeedMS = 0.f;

	/** Signifikante Wellenhoehe in Metern. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seegang")
	float SignificantWaveHeightM = 0.f;

	/** Dominante Wellenlaenge in Metern. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seegang")
	float PeakWavelengthM = 10.f;

	/** Globale Steilheit (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seegang")
	float Steepness = 0.3f;
};
