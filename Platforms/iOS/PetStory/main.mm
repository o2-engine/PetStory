#include "GameApplication.h"
#include "o2/O2.h"

using namespace o2;

int main(int argc, char * argv[]) {
	INITIALIZE_O2;
	
	mmake<GameApplication>()->Run(argc, argv);
	
	return 0;
}
