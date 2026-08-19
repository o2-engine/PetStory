#include "o2/stdafx.h"
#include "o2/O2.h"
#include "o2/Utils/Memory/MemoryAnalyzer.h"
#include "o2Editor/EditorApplication.h"
#include "o2Editor/EditorConfig.h"
#include "o2Editor/ToolsPanel.h"
#include "o2Editor/Windows/WindowsManager.h"

#include <unistd.h>

using namespace o2;

DECLARE_SINGLETON(Editor::WindowsManager);
DECLARE_SINGLETON(Editor::EditorConfig);
DECLARE_SINGLETON(Editor::ToolsPanel);

extern void InitializeTypeso2Editor();
extern void InitializeTypesGameLib();
extern void InitializeTypesPetStoryLib();
extern void InitializeTypesEditorLib();
extern void InitializeTypesAssetsBuildTool();

int main()
{
    // The page preRun streams the project working tree into MEMFS under /project;
    // running from Bin/WebAssembly inside it makes the editor's relative paths
    // (../../Assets, ../../EditorConfig.json, ...) resolve as on desktop.
    chdir("/project/Bin/WebAssembly");

    // Unlike desktop EditorMain, object tracking stays off: it roughly doubles
    // allocation cost and pushes the editor past the wasm memory ceiling
    o2::MemoryAnalyzer::enabledObjectsTracking = false;
    INITIALIZE_O2;
    InitializeTypesGameLib();
    InitializeTypesPetStoryLib();
    InitializeTypeso2Editor();
    InitializeTypesEditorLib();
    InitializeTypesAssetsBuildTool();

    auto app = mmake<Editor::EditorApplication>();
    app->Initialize();
    app->Launch();

    return 0;
}
