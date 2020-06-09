#include "o2/stdafx.h"
#include "o2/O2.h"
#include "o2Editor/Core/EditorApplication.h"
#include "o2Editor/Core/EditorConfig.h"
#include "o2Editor/Core/ToolsPanel.h"
#include "o2Editor/Core/WindowsSystem/WindowsManager.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"

using namespace o2;

DECLARE_SINGLETON(Editor::WindowsManager);
DECLARE_SINGLETON(Editor::EditorConfig);
DECLARE_SINGLETON(Editor::ToolsPanel);


int main()
{
	DataDocument doc;
	doc["abc"] = "asd";
	doc.AddElement() = 5;

	INITIALIZE_O2;

	Editor::EditorApplication* app = mnew Editor::EditorApplication();
	app->Initialize();
	app->Launch();

	return 0;
}
