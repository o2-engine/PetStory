#include "o2/stdafx.h"
#include "o2/O2.h"
#include "o2/Application/Application.h"
#include <gtest/gtest.h>

#include <filesystem>

extern void InitializeTypeso2TestsSupport();
extern void InitializeTypesGameLib();

using namespace o2;

// Rendered test runner for the PetStory game: full Application::Initialize with a real
// window and render device, so tests can drive the game through AppTestDriver (cursor
// input, frame pumping, screenshots). cwd is pinned to the executable folder so the
// relative asset paths resolve the same way as a direct launch from Bin.
int main(int argc, char** argv)
{
	if (argc > 0 && argv[0])
	{
		std::filesystem::path exe(argv[0]);
		if (exe.has_parent_path())
		{
			std::error_code ec;
			std::filesystem::current_path(exe.parent_path(), ec);
		}
	}

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
