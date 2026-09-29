#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EisenwalSettings.generated.h"

class UDataTable;
class UMaterialInterface;

/**
 * Projekt-Einstellungen fuer Eisenwal (Projekteinstellungen > Game > Eisenwal).
 * Alle Balancing-Tabellen werden hier referenziert, damit Code nie harte Asset-Pfade braucht.
 * Fehlt eine Tabelle, benutzt der Code eingebaute Standardwerte.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Eisenwal"))
class EISENWAL_API UEisenwalSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Seegang pro Beaufort-Stufe (Zeilen: FEisenwalSeaStateRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Tabellen")
	TSoftObjectPtr<UDataTable> SeaStateTable;

	/** Schiffswerte (Zeilen: FEisenwalShipStatsRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Tabellen")
	TSoftObjectPtr<UDataTable> ShipTable;

	/** Katastrophen-Parameter (Zeilen: FEisenwalDisasterRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Tabellen")
	TSoftObjectPtr<UDataTable> DisasterTable;

	/** Upgrades (Zeilen: FEisenwalUpgradeRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Tabellen")
	TSoftObjectPtr<UDataTable> UpgradeTable;

	/** Verbrauchsgueter/Ausruestung (Zeilen: FEisenwalItemRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Tabellen")
	TSoftObjectPtr<UDataTable> ItemTable;

	/** Auftragstypen (Zeilen: FEisenwalMissionRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Tabellen")
	TSoftObjectPtr<UDataTable> MissionTable;

	/** Inseln (Zeilen: FEisenwalIslandRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Tabellen")
	TSoftObjectPtr<UDataTable> IslandTable;

	/** Crew-Rollen (Zeilen: FEisenwalCrewRoleRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Tabellen")
	TSoftObjectPtr<UDataTable> CrewTable;

	/** Material der Ozeanoberflaeche (wird vom Setup-Skript erzeugt). */
	UPROPERTY(Config, EditAnywhere, Category = "Ozean")
	TSoftObjectPtr<UMaterialInterface> OceanMaterial;

	static const UEisenwalSettings* Get() { return GetDefault<UEisenwalSettings>(); }

	/** Laedt eine Tabelle synchron; gibt nullptr zurueck, wenn sie (noch) nicht existiert. */
	static UDataTable* LoadTable(const TSoftObjectPtr<UDataTable>& Table);
};
