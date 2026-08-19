#include "o2/stdafx.h"
#include "o2/Assets/Assets.h"
#include "o2/EngineSettings.h"

#include <emscripten.h>
#include "o2AssetBuilder/AssetsBuilder.h"

namespace o2
{
    // Called by Assets::RebuildAssets under PLATFORM_WASM (declared extern there,
    // so o2Framework does not depend on AssetsBuildTool headers). Runs the asset
    // builder in-process over MEMFS: the editor sees fresh BuiltAssets immediately
    // and the FS mirror persists them into the server session.
    void o2_WasmRebuildAssets(bool forcible)
    {
        AssetsBuilder builder;
        builder.BuildAssets(Platform::WebAssembly,
                            ::GetAssetsPath(),
                            ::GetBuiltAssetsPath(),
                            ::GetBuiltAssetsTreePath(),
                            String(::GetEditorAssetsPath()) + "../../CompressToolsConfig.json",
                            forcible);
    }
}

// Page-callable rebuild (used by the shell and tests): full editor flow —
// build, reload the assets tree, fire onAssetsRebuilt
extern "C" EMSCRIPTEN_KEEPALIVE void o2_web_rebuild_assets()
{
    o2Assets.RebuildAssets(false);
}
