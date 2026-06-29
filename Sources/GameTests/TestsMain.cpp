#include "o2/stdafx.h"
#include "o2/O2.h"
#include "o2/Application/Application.h"
#include <gtest/gtest.h>

extern void InitializeTypeso2TestsSupport();
extern void InitializeTypesGameLib();

using namespace o2;

// Test runner for the PetStory game components. Initializes o2 headless (Scene, Assets,
// Time, ...) and registers both the shared test support types and the GameLib types.
int main(int argc, char** argv)
{
	Application::SetHeadless(true);

	InitializeTypeso2TestsSupport();
	InitializeTypesGameLib();
	INITIALIZE_O2;

	::testing::InitGoogleTest(&argc, argv);

	bool listOnly = ::testing::GTEST_FLAG(list_tests);

	Ref<Application> app;
	if (!listOnly)
	{
		app = mmake<Application>();
		app->Initialize();
	}

	int result = RUN_ALL_TESTS();

	if (app)
		app->Deinitialize();

	return result;
}
