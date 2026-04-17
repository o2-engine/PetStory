package com.petstory.game;

import android.content.res.AssetManager;

/**
 * Thin Java side of the JNI bridge into the native PetStory shared library.
 * Corresponding C++ exports live in Platforms/Android/app/src/main/cpp/JavaBridge.cpp
 * and are linked into the PetStory.so produced by the project-level CMake build.
 */
public final class NativeBridge {
    static {
        System.loadLibrary("PetStory");
    }

    private NativeBridge() {}

    /**
     * Called once from {@link GLView.Renderer#onSurfaceChanged} after the EGL/GLES2
     * context is live. Passes all state the native side needs to bootstrap.
     */
    public static native void init(AssetManager assetManager,
                                   String dataPath,
                                   int width,
                                   int height);

    /** Called every frame from {@link GLView.Renderer#onDrawFrame}. */
    public static native void step();

    /** Touch/pointer delivery from {@link MainActivity#onTouchEvent}. */
    public static native void touchDown(int pointerId, float x, float y);
    public static native void touchMove(int pointerId, float x, float y);
    public static native void touchUp(int pointerId, float x, float y);
}
