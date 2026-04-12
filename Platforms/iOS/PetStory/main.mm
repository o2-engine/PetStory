#include "GameApplication.h"
#include "o2/O2.h"

extern void InitializeTypesGameLib();

using namespace o2;

int main(int argc, char * argv[]) {
	InitializeTypesGameLib();
	INITIALIZE_O2;

	auto app = mmake<GameApplication>();
	app->Run(argc, argv);

	return 0;
}
