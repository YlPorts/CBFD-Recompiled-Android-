package com.ylports.cbfd;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;

final class RendererSettings {
    static final String EXTRA = "com.ylports.cbfd.RENDERER";
    private static final String PREFS = "graphics";
    static String selected(Context context) {
        String value = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getString("renderer", "vulkan");
        return "opengl".equals(value) ? "opengl" : "vulkan";
    }
    static boolean needsChoice(Context context) {
        return !context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getBoolean("menu014", false);
    }
    static void markShown(Context context) {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putBoolean("menu014", true).commit();
    }
    static String label(Context context) {
        return "opengl".equals(selected(context)) ? "OpenGL ES 3 · GLideN64" : "Vulkan · RT64";
    }
    static void show(Activity activity, Runnable changed) {
        String[] choices = {"Vulkan · RT64", "OpenGL ES 3 · GLideN64 (experimental)"};
        new AlertDialog.Builder(activity).setTitle("Motor gráfico")
            .setSingleChoiceItems(choices, "opengl".equals(selected(activity)) ? 1 : 0, (dialog, which) -> {
                boolean saved = activity.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
                    .putString("renderer", which == 1 ? "opengl" : "vulkan").commit();
                if (saved) { dialog.dismiss(); changed.run(); }
            }).setNegativeButton("Cancelar", null).show();
    }
    private RendererSettings() {}
}
