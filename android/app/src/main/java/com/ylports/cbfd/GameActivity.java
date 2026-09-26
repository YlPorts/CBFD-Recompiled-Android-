package com.ylports.cbfd;

import android.os.Bundle;
import android.view.ViewGroup;
import java.io.File;
import org.libsdl.app.SDLActivity;

public final class GameActivity extends SDLActivity {
    private TouchControls touch;
    static native void nativeInput(int buttons, float x, float y);
    private static native void nativeRequestQuit();
    @Override protected String[] getLibraries() { return new String[] {"SDL2", "main"}; }
    @Override protected String[] getArguments() {
        return new String[] {"--rom", new File(getFilesDir(), "roms/" + RomImporter.ROM_NAME).getAbsolutePath(),
            "--data", new File(getFilesDir(), "state").getAbsolutePath()};
    }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (isFinishing() || mBrokenLibraries) return;
        Immersive.apply(this);
        touch = new TouchControls(this);
        ViewGroup content = findViewById(android.R.id.content);
        content.addView(touch, new ViewGroup.LayoutParams(-1, -1));
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused) Immersive.apply(this);
        else if (touch != null) touch.releaseAll();
    }
    @Override protected void onPause() {
        if (touch != null) touch.releaseAll();
        super.onPause();
    }
    @Override public void onBackPressed() {
        if (mBrokenLibraries) { finish(); return; }
        nativeRequestQuit();
    }
    @Override protected void onDestroy() {
        if (touch != null) touch.releaseAll();
        // SDL joins its native thread here; don't kill it before saves have flushed.
        super.onDestroy();
        // Runtime globals are single-start. Only this isolated :game process is ended.
        if (isFinishing() && !isChangingConfigurations()) android.os.Process.killProcess(android.os.Process.myPid());
    }
}
