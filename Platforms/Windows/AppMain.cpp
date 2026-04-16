#include "GameApplication.h"
#include "o2/O2.h"

extern void InitializeTypesGameLib();

int main()
{
	INITIALIZE_O2;
	InitializeTypesGameLib();

	auto app = mmake<GameApplication>();
	app->Initialize();
	app->Launch();

	return 0;
}
