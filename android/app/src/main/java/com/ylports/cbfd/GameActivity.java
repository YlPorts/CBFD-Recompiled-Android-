package com.ylports.cbfd;

import android.os.Bundle;
import android.os.Build;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.widget.Toast;
import org.libsdl.app.SDLSurface;
import android.view.ViewGroup;
import java.io.File;
import org.libsdl.app.SDLActivity;

public final class GameActivity extends SDLActivity {
    private TouchControls touch;
    static native void nativeInput(int buttons, float x, float y);
    static native void nativeCamera(float x, float y);
    private static native void nativeSurface(Surface surface, int width, int height);
    private static native void nativeForeground(boolean active);
    private static native void nativeRequestQuit();
    @Override protected String[] getLibraries() { return new String[] {"c++_shared", "SDL2", "main"}; }
    @Override protected String getMainSharedObject() {
        // These libraries are mapped from the APK; nativeLibraryDir may have no files.
        return "libmain.so";
    }
    @Override public void loadLibraries() {
        for (String library : getLibraries()) {
            StartupDiagnostics.log("Loading " + library);
            try { System.loadLibrary(library); }
            catch (UnsatisfiedLinkError | RuntimeException error) {
                StartupDiagnostics.failure("Loading " + library, error);
                throw error;
            }
        }
        StartupDiagnostics.log("All native libraries loaded");
    }
    @Override protected SDLSurface createSDLSurface(android.content.Context context) {
        // Publish before SDL can resume its native thread. Keeping a separate
        // ANativeWindow reference avoids racing SDL's surfaceDestroyed cleanup.
        return new SDLSurface(context) {
            private boolean applied;
            private void applyRate(SurfaceHolder holder) {
                if(applied || Build.VERSION.SDK_INT<30 || !holder.getSurface().isValid()) return;
                try {
                    if(Build.VERSION.SDK_INT>=31) holder.getSurface().setFrameRate(60f,
                        Surface.FRAME_RATE_COMPATIBILITY_DEFAULT, Surface.CHANGE_FRAME_RATE_ONLY_IF_SEAMLESS);
                    else holder.getSurface().setFrameRate(60f,Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
                    applied=true;
                } catch(RuntimeException error) { StartupDiagnostics.log("Surface frame-rate hint: "+error); }
            }
            @Override public void surfaceCreated(SurfaceHolder holder) {
                applied=false;
                super.surfaceCreated(holder);
            }
            @Override public void surfaceChanged(SurfaceHolder holder,int format,int width,int height) {
                nativeSurface(holder.getSurface(),width,height);
                applyRate(holder);
                StartupDiagnostics.log("Native Surface="+width+"x"+height+"; fixed2x; lifecycle publish; build=blend-surface-018");
                super.surfaceChanged(holder,format,width,height);
            }
            @Override public void surfaceDestroyed(SurfaceHolder holder) {
                nativeSurface(null,0,0);
                applied=false;
                StartupDiagnostics.log("Native Surface withdrawn before SDL destroy");
                super.surfaceDestroyed(holder);
            }
        };
    }
    private void copyLiveDiagnostics() {
        // Read the private tail off the UI/game threads; copy only after the user's long press.
        new Thread(()->{
            String report=StartupDiagnostics.report();
            runOnUiThread(()->{
                if(isFinishing()||isDestroyed()) return;
                ClipboardManager clipboard=(ClipboardManager)getSystemService(CLIPBOARD_SERVICE);
                if(clipboard!=null) clipboard.setPrimaryClip(ClipData.newPlainText("Conker: diagnóstico",report));
                Toast.makeText(this,"Diagnóstico de rendimiento copiado",Toast.LENGTH_SHORT).show();
            });
        },"ConkerDiagnostic").start();
    }
    @Override protected String[] getArguments() {
        return new String[] {"--rom", new File(getFilesDir(), "roms/" + RomImporter.ROM_NAME).getAbsolutePath(),
            "--data", new File(getFilesDir(), "state").getAbsolutePath()};
    }
    @Override protected void onCreate(Bundle state) {
        StartupDiagnostics.log("GameActivity.onCreate: before SDL");
        super.onCreate(state);
        StartupDiagnostics.log("GameActivity.onCreate: SDL returned; broken=" + mBrokenLibraries);
        if (isFinishing() || mBrokenLibraries) return;
        Immersive.apply(this);
        touch = new TouchControls(this, this::copyLiveDiagnostics);
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
        if (!mBrokenLibraries) nativeForeground(false);
        super.onPause();
    }
    @Override protected void onResume() {
        super.onResume();
        if (!mBrokenLibraries) nativeForeground(true);
    }
    @Override public void onBackPressed() {
        if (mBrokenLibraries) { finish(); return; }
        nativeRequestQuit();
    }
    @Override protected void onDestroy() {
        if (touch != null) touch.releaseAll();
        // SDL joins its native thread here; don't kill it before saves have flushed.
        super.onDestroy();
        StartupDiagnostics.gameDestroyed(mBrokenLibraries);
        // Runtime globals are single-start. Only this isolated :game process is ended.
        if (isFinishing() && !isChangingConfigurations()) android.os.Process.killProcess(android.os.Process.myPid());
    }
}
