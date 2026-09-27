package com.ylports.cbfd;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.os.Build;
import android.util.Log;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.List;

/** App-private crash evidence only. No network, analytics or storage permission. */
final class StartupDiagnostics {
    static final String VERSION = "0.1.3-alpha";
    private static Context app;

    static void install(Context context) {
        app = context.getApplicationContext();
        Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread, error) -> {
            failure("Java / " + thread.getName(), error);
            if (previous != null) previous.uncaughtException(thread, error);
            else {
                android.os.Process.killProcess(android.os.Process.myPid());
                System.exit(10);
            }
        });
        log("Application " + VERSION + " pid=" + android.os.Process.myPid());
    }

    static File file(String name) { return new File(app.getFilesDir(), "state/" + name); }
    static synchronized void log(String text) {
        Log.i("ConkerStartup", text);
        if (app == null) return;
        try {
            File f = file("startup-java.log");
            File parent = f.getParentFile();
            if (!parent.isDirectory() && !parent.mkdirs()) return;
            if (f.length() > 64000) {
                File old = file("startup-java.previous.log");
                if (old.exists()) old.delete();
                f.renameTo(old);
            }
            try (FileOutputStream out = new FileOutputStream(f, true)) {
                out.write((System.currentTimeMillis() + " pid=" + android.os.Process.myPid() + " " + text + "\n").getBytes(StandardCharsets.UTF_8));
            }
        } catch (IOException | RuntimeException ignored) { /* Diagnostics must never cause a crash. */ }
    }
    static void failure(String stage, Throwable error) {
        Log.e("ConkerStartup", stage, error);
        StringWriter buffer = new StringWriter();
        error.printStackTrace(new PrintWriter(buffer));
        log(stage + "\n" + buffer);
        write("startup-error.txt", stage + "\n" + buffer);
    }
    private static void write(String name, String text) {
        try {
            File f = file(name);
            f.getParentFile().mkdirs();
            try (FileOutputStream out = new FileOutputStream(f)) {
                out.write(text.getBytes(StandardCharsets.UTF_8));
                out.getFD().sync();
            }
        } catch (IOException | RuntimeException ignored) {}
    }
    static boolean needsRecovery() {
        if (file("startup-error.txt").exists() || file("java-game.pending").exists()
            || file("running.marker").exists()) return true;
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                long acknowledged = app.getSharedPreferences("startup", Context.MODE_PRIVATE).getLong("acknowledged", 0);
                ActivityManager am = (ActivityManager) app.getSystemService(Context.ACTIVITY_SERVICE);
                for (ApplicationExitInfo exit : am.getHistoricalProcessExitReasons(app.getPackageName(), 0, 8)) {
                    int reason = exit.getReason();
                    if (exit.getTimestamp() > acknowledged && (reason == ApplicationExitInfo.REASON_CRASH
                        || reason == ApplicationExitInfo.REASON_CRASH_NATIVE
                        || reason == ApplicationExitInfo.REASON_INITIALIZATION_FAILURE)) return true;
                }
            } catch (RuntimeException ignored) {}
        }
        return false;
    }
    static void acknowledge() {
        app.getSharedPreferences("startup", Context.MODE_PRIVATE).edit()
            .putLong("acknowledged", System.currentTimeMillis()).commit();
        if (file("startup-error.txt").exists()) file("startup-error.txt").delete();
    }
    static void beginLaunch() {
        acknowledge();
        write("java-game.pending", "Android " + VERSION + " / " + System.currentTimeMillis() + "\n");
        // Keep the previous log instead of mistaking an old clean shutdown for the new session.
        File current = file("last-run.log"), previous = file("previous-run.log");
        if (current.exists()) {
            if (previous.exists()) previous.delete();
            current.renameTo(previous);
        }
        log("Launching SDL game process");
    }
    static void gameDestroyed(boolean brokenLibraries) {
        if (!brokenLibraries && !file("running.marker").exists()
            && tail(file("last-run.log"), 4096).contains("[shutdown] Runtime finished; save thread joined")) {
            file("java-game.pending").delete();
            log("Clean native shutdown confirmed");
        } else log("Game activity ended without a confirmed clean runtime shutdown");
    }
    static String report() {
        StringBuilder out = new StringBuilder("Conker Android " + VERSION + "\n");
        out.append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
            .append(" / Android ").append(Build.VERSION.RELEASE).append(" API ").append(Build.VERSION.SDK_INT)
            .append("\nABI: ").append(java.util.Arrays.toString(Build.SUPPORTED_ABIS)).append('\n');
        for (String name : new String[]{"startup-error.txt", "startup-java.log", "last-run.log", "rt64/rt64.log", "previous-run.log"}) {
            if (file(name).isFile()) out.append("\n--- ").append(name).append(" ---\n").append(tail(file(name), 24000));
        }
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                ActivityManager am = (ActivityManager) app.getSystemService(Context.ACTIVITY_SERVICE);
                List<ApplicationExitInfo> exits = am.getHistoricalProcessExitReasons(app.getPackageName(), 0, 6);
                for (ApplicationExitInfo exit : exits) {
                    out.append("\nAndroid exit: process=").append(exit.getProcessName())
                        .append(" reason=").append(exit.getReason()).append(" status=").append(exit.getStatus())
                        .append(" timestamp=").append(exit.getTimestamp()).append(" RSS-KB=").append(exit.getRss())
                        .append("\n").append(exit.getDescription()).append('\n');
                }
            } catch (RuntimeException e) { out.append("\nExitInfo no disponible: ").append(e); }
        }
        return out.toString();
    }
    private static String tail(File f, int limit) {
        try (RandomAccessFile in = new RandomAccessFile(f, "r")) {
            long length = in.length();
            in.seek(Math.max(0, length - limit));
            byte[] bytes = new byte[(int) Math.min(length, limit)];
            in.readFully(bytes);
            return new String(bytes, StandardCharsets.UTF_8);
        } catch (IOException | RuntimeException e) { return "Sin registro: " + e.getMessage() + "\n"; }
    }
    private StartupDiagnostics() {}
}
