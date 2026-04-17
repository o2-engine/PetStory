// JNI bridge for com.petstory.game.NativeBridge.
//
// Compiled into libPetStory.so (the shared-library variant of the PetStory
// target selected when ANDROID is defined — see the project-level CMakeLists).
// Java loads the .so via `System.loadLibrary("PetStory")` in NativeBridge.java.
//
// The Java side owns the EGL/GLES2 context (GLSurfaceView), so this bridge:
//   - On init():   stashes platform state via AndroidPlatform::Set*, then runs
//                  the standard o2 bootstrap (INITIALIZE_O2 + GameApplication).
//   - On step():   pumps one frame.
//   - On touchXX():forwards pointer events to o2::Input.

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>

#include "GameApplication.h"
#include "o2/O2.h"
#include "o2/Application/Application.h"
#include "o2/Application/Input.h"
#include "o2/Application/Android/AndroidPlatform.h"

extern void InitializeTypesGameLib();

namespace
{
    o2::Ref<GameApplication> gApp;
    JavaVM*                  gJavaVM = nullptr;

    constexpr const char* kLogTag = "PetStory";

    // Translate Android raw pixel (top-left origin, Y-down) into the center-origin,
    // Y-up pixel space o2 uses everywhere else (see iOS/Windows/Web touch handlers).
    o2::Vec2F ToEngineCursorPos(float x, float y)
    {
        const o2::Vec2I res = o2::AndroidPlatform::GetResolution();
        return o2::Vec2F(x - res.x * 0.5f, res.y * 0.5f - y);
    }
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
{
    gJavaVM = vm;
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "JNI_OnLoad: PetStory native library loaded");
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_petstory_game_NativeBridge_init(JNIEnv* env,
                                         jclass  /*clazz*/,
                                         jobject jAssetManager,
                                         jstring jDataPath,
                                         jint    width,
                                         jint    height)
{
    if (gApp)
    {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "NativeBridge.init called twice — ignoring second call");
        return;
    }

    AAssetManager* assetManager = AAssetManager_fromJava(env, jAssetManager);

    const char* dataPathChars = env->GetStringUTFChars(jDataPath, nullptr);
    o2::String  dataPath      = dataPathChars ? dataPathChars : "";
    if (dataPathChars)
        env->ReleaseStringUTFChars(jDataPath, dataPathChars);

    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "NativeBridge.init: %dx%d, data='%s'",
                        width, height, dataPath.Data());

    o2::AndroidPlatform::SetJVM(gJavaVM);
    o2::AndroidPlatform::SetAssetManager(assetManager);
    o2::AndroidPlatform::SetDataPath(dataPath);
    o2::AndroidPlatform::SetResolution(o2::Vec2I(width, height));

    INITIALIZE_O2;
    InitializeTypesGameLib();

    gApp = mmake<GameApplication>();
    gApp->Initialize();
    gApp->Launch();
}

extern "C" JNIEXPORT void JNICALL
Java_com_petstory_game_NativeBridge_step(JNIEnv* /*env*/, jclass /*clazz*/)
{
    if (gApp)
        gApp->Update();
}

extern "C" JNIEXPORT void JNICALL
Java_com_petstory_game_NativeBridge_touchDown(JNIEnv* /*env*/, jclass /*clazz*/,
                                              jint pointerId, jfloat x, jfloat y)
{
    if (gApp)
        o2Input.OnCursorPressed(ToEngineCursorPos(x, y), static_cast<o2::CursorId>(pointerId));
}

extern "C" JNIEXPORT void JNICALL
Java_com_petstory_game_NativeBridge_touchMove(JNIEnv* /*env*/, jclass /*clazz*/,
                                              jint pointerId, jfloat x, jfloat y)
{
    if (gApp)
        o2Input.OnCursorMoved(ToEngineCursorPos(x, y), static_cast<o2::CursorId>(pointerId));
}

extern "C" JNIEXPORT void JNICALL
Java_com_petstory_game_NativeBridge_touchUp(JNIEnv* /*env*/, jclass /*clazz*/,
                                            jint pointerId, jfloat /*x*/, jfloat /*y*/)
{
    if (gApp)
        o2Input.OnCursorReleased(static_cast<o2::CursorId>(pointerId));
}
