package com.ylports.cbfd;

import android.app.Activity;
import android.os.Build;
import android.view.*;

final class Immersive {
    static void apply(Activity activity) {
        if (activity.isFinishing() || activity.isDestroyed()) return;
        try {
            Window window = activity.getWindow();
            if (window == null) return;
            // PhoneWindow.getInsetsController() dereferences its decor without creating it.
            // The old launcher called it before setContentView and crashed on Android 16.
            View decor = window.getDecorView();
            window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            WindowManager.LayoutParams attributes = window.getAttributes();
            boolean changed = attributes.preferredRefreshRate != 60.0f;
            attributes.preferredRefreshRate = 60.0f;
            if (Build.VERSION.SDK_INT >= 28) {
                changed |= attributes.layoutInDisplayCutoutMode != WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
                attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            }
            if (changed) window.setAttributes(attributes);
            if (Build.VERSION.SDK_INT >= 30) {
                window.setDecorFitsSystemWindows(false);
                hideBars(decor);
                if (!decor.isAttachedToWindow()) {
                    decor.post(() -> {
                        if (!activity.isFinishing() && !activity.isDestroyed()) hideBars(decor);
                    });
                }
            } else {
                decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
            }
        } catch (RuntimeException | LinkageError error) {
            // A device-specific window policy must not prevent importing a ROM.
            StartupDiagnostics.log("Fullscreen setup: " + error);
        }
    }
    private static void hideBars(View decor) {
        if (Build.VERSION.SDK_INT < 30) return;
        WindowInsetsController controls = decor.getWindowInsetsController();
        if (controls != null) {
            controls.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            controls.hide(WindowInsets.Type.systemBars());
        }
    }
    private Immersive() {}
}
