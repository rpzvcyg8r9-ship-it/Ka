#include "Core/EisenwalSettings.h"
#include "Engine/DataTable.h"

UDataTable* UEisenwalSettings::LoadTable(const TSoftObjectPtr<UDataTable>& Table)
{
	if (Table.IsNull())
	{
		return nullptr;
	}
	return Table.LoadSynchronous();
}
