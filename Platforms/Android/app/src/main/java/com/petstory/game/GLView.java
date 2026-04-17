package com.petstory.game;

import android.content.Context;
import android.content.res.AssetManager;
import android.opengl.GLSurfaceView;
import android.view.MotionEvent;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * GLSurfaceView that owns the EGL/GLES2 context and drives rendering.
 *
 * The engine's C++ render backend ({@link NativeBridge#init}/{@link NativeBridge#step})
 * assumes a live GLES2 context and a viewport already sized to the surface.
 * This class exists purely to satisfy that contract.
 */
public final class GLView extends GLSurfaceView {
    private final AssetManager assetManager;
    private final String       dataPath;

    public GLView(Context context, AssetManager assetManager, String dataPath) {
        super(context);
        this.assetManager = assetManager;
        this.dataPath     = dataPath;

        setEGLContextClientVersion(2);
        setRenderer(new Renderer());
        setRenderMode(RENDERMODE_CONTINUOUSLY);
    }

    // Touch events arrive on the UI thread; dispatch to the GL thread via
    // queueEvent so native Input queue mutation stays serialized with the
    // engine's PreUpdate drain.
    @Override
    public boolean onTouchEvent(final MotionEvent event) {
        final int action = event.getActionMasked();
        final int index  = event.getActionIndex();
        final int count  = event.getPointerCount();

        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                final int   id = event.getPointerId(index);
                final float x  = event.getX(index);
                final float y  = event.getY(index);
                queueEvent(new Runnable() { @Override public void run() {
                    NativeBridge.touchDown(id, x, y);
                }});
                return true;
            }
            case MotionEvent.ACTION_MOVE: {
                final int[]   ids = new int[count];
                final float[] xs  = new float[count];
                final float[] ys  = new float[count];
                for (int i = 0; i < count; i++) {
                    ids[i] = event.getPointerId(i);
                    xs[i]  = event.getX(i);
                    ys[i]  = event.getY(i);
                }
                queueEvent(new Runnable() { @Override public void run() {
                    for (int i = 0; i < ids.length; i++)
                        NativeBridge.touchMove(ids[i], xs[i], ys[i]);
                }});
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_CANCEL: {
                final int   id = event.getPointerId(index);
                final float x  = event.getX(index);
                final float y  = event.getY(index);
                queueEvent(new Runnable() { @Override public void run() {
                    NativeBridge.touchUp(id, x, y);
                }});
                return true;
            }
        }
        return super.onTouchEvent(event);
    }

    private final class Renderer implements GLSurfaceView.Renderer {
        private boolean initialized = false;

        @Override
        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            // Context was just (re)created — the native side will (re)initialize in
            // onSurfaceChanged when we know the viewport dimensions.
            initialized = false;
        }

        @Override
        public void onSurfaceChanged(GL10 gl, int width, int height) {
            if (!initialized) {
                NativeBridge.init(assetManager, dataPath, width, height);
                initialized = true;
            }
        }

        @Override
        public void onDrawFrame(GL10 gl) {
            if (initialized) {
                NativeBridge.step();
            }
        }
    }
}
