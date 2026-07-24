#include "App/GameApplication.h"
#include "o2/O2.h"

extern void InitializeTypesGameLib();
extern void InitializeTypesPetStoryLib();

int main()
{
	INITIALIZE_O2;
	InitializeTypesGameLib();
	InitializeTypesPetStoryLib();

	auto app = mmake<GameApplication>();
	app->Initialize();
	app->Launch();

	return 0;
}
