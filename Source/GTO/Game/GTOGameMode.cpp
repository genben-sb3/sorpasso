#include "Game/GTOGameMode.h"

#include "Game/GTOHUD.h"
#include "Vehicle/GTOVehiclePawn.h"

AGTOGameMode::AGTOGameMode()
{
	DefaultPawnClass = AGTOVehiclePawn::StaticClass();
	HUDClass = AGTOHUD::StaticClass();
}
