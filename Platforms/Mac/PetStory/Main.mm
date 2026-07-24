#include "App/GameApplication.h"
#include "o2/O2.h"

using namespace o2;

int main(int argc, const char * argv[]) {
	INITIALIZE_O2;
	
	GameApplication* app = mnew GameApplication();
	app->Initialize();
	app->Launch();
	delete app;

	return 0;
}
