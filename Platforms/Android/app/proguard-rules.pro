# Keep JNI bridge methods reachable after R8 shrinking.
-keep class com.petstory.game.NativeBridge { *; }
