#include "o2Editor/stdafx.h"
#include "o2/Assets/Assets.h"
#include "o2/EngineSettings.h"

#include <emscripten.h>
#include "o2AssetBuilder/AssetsBuilder.h"
#include "o2Editor/EditorApplication.h"
#include "o2/Assets/Types/SceneAsset.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/ActorTransform.h"
#include "o2/Scene/Component.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Scripts/ScriptEngine.h"
#include "o2Editor/Windows/WindowsManager.h"
#include "o2Editor/Windows/GameWindow/GameWindow.h"

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

// Full rebuild. The incremental one trusts the built tree, so a built file that
// went missing while its entry stayed is never restored - this heals that.
extern "C" EMSCRIPTEN_KEEPALIVE void o2_web_rebuild_assets_forced()
{
    o2Assets.RebuildAssets(true);
}

// Play mode control for the page (the AI agent drives the game this way, since
// clicking the editor chrome from script is unreliable)
extern "C" EMSCRIPTEN_KEEPALIVE void o2_web_set_play(int playing)
{
    ((Editor::EditorApplication*)(o2::Application::InstancePtr()))->SetPlaying(playing != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE int o2_web_is_playing()
{
    return ((Editor::EditorApplication*)(o2::Application::InstancePtr()))->IsPlaying() ? 1 : 0;
}

// Scene control for the page: the agent cannot click the editor chrome, so it
// opens and saves scenes through here
extern "C" EMSCRIPTEN_KEEPALIVE void o2_web_open_scene(const char* path)
{
    ((Editor::EditorApplication*)(o2::Application::InstancePtr()))
        ->LoadScene(o2::AssetRef<o2::SceneAsset>(o2::String(path)));
}

extern "C" EMSCRIPTEN_KEEPALIVE void o2_web_save_scene()
{
    ((Editor::EditorApplication*)(o2::Application::InstancePtr()))->SaveScene();
}

// ---------------------------------------------------------------- web scene API
// The AI agent cannot click the editor and cannot see the scene, so it reads the
// hierarchy, the transforms and the widget layouts from here, and does everything
// else by running scripts in the engine.

using namespace o2;

static char* WebDup(const String& str)
{
    char* result = (char*)malloc(str.Length() + 1);
    memcpy(result, str.Data(), str.Length() + 1);
    return result;
}

static String WebEscape(const String& str)
{
    String result;
    for (int i = 0; i < str.Length(); i++)
    {
        char c = str[i];
        if (c == '"' || c == '\\') { result += '\\'; result += c; }
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else result += c;
    }
    return result;
}

static String WebVec(const Vec2F& v)
{
    return "{\"x\":" + (String)v.x + ",\"y\":" + (String)v.y + "}";
}

static String WebRect(const RectF& r)
{
    return "{\"left\":" + (String)r.left + ",\"top\":" + (String)r.top +
           ",\"right\":" + (String)r.right + ",\"bottom\":" + (String)r.bottom + "}";
}

static String WebDumpActor(const Ref<Actor>& actor, const String& path, int depth)
{
    if (!actor)
        return "null";

    String actorPath = path.IsEmpty() ? actor->GetName() : path + "/" + actor->GetName();
    String result = "{\"name\":\"" + WebEscape(actor->GetName()) + "\"";
    result += ",\"path\":\"" + WebEscape(actorPath) + "\"";
    result += ",\"type\":\"" + WebEscape(actor->GetType().GetName()) + "\"";
    result += ",\"enabled\":" + String(actor->IsEnabled() ? "true" : "false");

    if (auto transform = actor->transform)
    {
        result += ",\"position\":" + WebVec(transform->GetPosition2D());
        result += ",\"size\":" + WebVec(transform->GetSize2D());
        result += ",\"scale\":" + WebVec(transform->GetScale2D());
        result += ",\"angleDegrees\":" + (String)transform->GetAngleDegrees();
        result += ",\"worldRect\":" + WebRect(transform->GetWorldRect());
    }

    if (auto widget = DynamicCast<Widget>(actor))
    {
        if (auto layout = widget->layout)
        {
            result += ",\"widgetLayout\":{\"anchorMin\":" + WebVec(layout->GetAnchorMin()) +
                      ",\"anchorMax\":" + WebVec(layout->GetAnchorMax()) +
                      ",\"offsetMin\":" + WebVec(layout->GetOffsetMin()) +
                      ",\"offsetMax\":" + WebVec(layout->GetOffsetMax()) + "}";
        }
        result += ",\"transparency\":" + (String)widget->GetTransparency();
    }

    String components;
    for (auto& component : actor->GetComponents())
    {
        if (!component)
            continue;
        if (!components.IsEmpty())
            components += ",";
        components += "\"" + WebEscape(component->GetType().GetName()) + "\"";
    }
    result += ",\"components\":[" + components + "]";

    auto& children = actor->GetChildren();
    result += ",\"childrenCount\":" + (String)children.Count();
    if (depth > 0 && !children.IsEmpty())
    {
        String childrenDump;
        for (auto& child : children)
        {
            if (!childrenDump.IsEmpty())
                childrenDump += ",";
            childrenDump += WebDumpActor(child, actorPath, depth - 1);
        }
        result += ",\"children\":[" + childrenDump + "]";
    }

    return result + "}";
}

// Hierarchy of the open scene, with transforms, widget layouts and component types
extern "C" EMSCRIPTEN_KEEPALIVE char* o2_web_scene_dump(const char* rootPath, int depth)
{
    String path(rootPath ? rootPath : "");
    String result;

    String openScene = ((Editor::EditorApplication*)(o2::Application::InstancePtr()))->GetLoadedSceneName();

    if (!path.IsEmpty())
    {
        auto actor = o2Scene.FindActor(path);
        if (!actor)
            return WebDup("{\"error\":\"actor not found: " + WebEscape(path) + "\"}");

        result = "{\"openScene\":\"" + WebEscape(openScene) + "\",\"root\":" +
                 WebDumpActor(actor, o2FileSystem.GetParentPath(path), depth) + "}";
    }
    else
    {
        String roots;
        for (auto& actor : o2Scene.GetRootActors())
        {
            if (!roots.IsEmpty())
                roots += ",";
            roots += WebDumpActor(actor, "", depth);
        }
        result = "{\"openScene\":\"" + WebEscape(openScene) + "\",\"actors\":[" + roots + "]}";
    }

    return WebDup(result);
}

// Everything needed to turn a world position into a canvas pixel: the canvas size,
// the Game window rectangle and the game camera
extern "C" EMSCRIPTEN_KEEPALIVE char* o2_web_view_info()
{
    Vec2F resolution = o2Render.GetResolution();
    bool playing = ((Editor::EditorApplication*)(o2::Application::InstancePtr()))->IsPlaying();

    String sceneName = ((Editor::EditorApplication*)(o2::Application::InstancePtr()))->GetLoadedSceneName();

    String result = "{\"canvas\":" + WebVec(resolution) +
                    ",\"playing\":" + String(playing ? "true" : "false") +
                    ",\"openScene\":\"" + WebEscape(sceneName) + "\"";

    if (auto gameWindow = Editor::WindowsManager::Instance().GetWindow<Editor::GameWindow>())
    {
        if (auto view = gameWindow->GetGameViewWidget())
            result += ",\"gameView\":" + WebRect(view->layout->GetWorldRect());
    }

    auto& cameras = o2Scene.GetCameras();
    if (!cameras.IsEmpty())
    {
        if (auto camera = cameras[0].Lock())
            result += ",\"camera\":{\"position\":" + WebVec(camera->transform->GetWorldPosition2D()) +
                      ",\"size\":" + WebVec(camera->transform->GetSize2D()) + "}";
    }

    return WebDup(result + "}");
}

// Runs JavaScript inside the engine, with the scene reachable through `sceneRoots`
// and `findActor(path)`. This is how the agent does anything the tools do not cover.
extern "C" EMSCRIPTEN_KEEPALIVE char* o2_web_run_script(const char* code)
{
    if (!code)
        return WebDup("{\"ok\":false,\"error\":\"empty script\"}");

    auto global = o2Scripts.GetGlobal();

    ScriptValue roots = ScriptValue::EmptyArray();
    for (auto& actor : o2Scene.GetRootActors())
        roots.AddElement(ScriptValue(actor));

    global.SetProperty("sceneRoots", roots);
    global.SetProperty("findActor", ScriptValue([](const String& path) { return o2Scene.FindActor(path); }));

    ScriptValue result = o2Scripts.Eval(String(code));
    String text = result.ToString();

    return WebDup("{\"ok\":true,\"result\":\"" + WebEscape(text) + "\"}");
}
