package com.ylports.cbfd;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.*;
import java.io.*;
import java.util.concurrent.*;

/** One-time import. Subsequent launches go straight to the game. */
public final class LauncherActivity extends Activity {
    private static final int PICK_ROM = 10;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private TextView status;
    private Button choose;
    private boolean importing;

    private File romDirectory() { return new File(getFilesDir(), "roms"); }
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        StartupDiagnostics.log("LauncherActivity.onCreate");
        File rom = new File(romDirectory(), RomImporter.ROM_NAME);
        if (StartupDiagnostics.needsRecovery()) { showInterruptedRun(); return; }
        if (rom.isFile() && rom.length() == RomImporter.ROM_SIZE) {
            launch();
            return;
        }
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setGravity(Gravity.CENTER);
        int padding = Math.round(32 * getResources().getDisplayMetrics().density);
        content.setPadding(padding, padding, padding, padding);
        status = new TextView(this);
        status.setText("Conker Recompiled\nSelecciona tu ROM USA o ZIP para empezar.");
        status.setTextSize(20);
        status.setGravity(Gravity.CENTER);
        choose = new Button(this);
        choose.setText("Importar ROM");
        choose.setOnClickListener(v -> {
            if (importing) return;
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivityForResult(intent, PICK_ROM);
        });
        content.addView(status);
        content.addView(choose);
        setContentView(content);
        content.post(() -> Immersive.apply(this));
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_ROM || result != RESULT_OK || data == null || data.getData() == null || importing) return;
        importing = true;
        choose.setEnabled(false);
        status.setText("Comprobando ROM…");
        final android.net.Uri uri = data.getData();
        worker.submit(() -> {
            try (InputStream stream = getContentResolver().openInputStream(uri)) {
                RomImporter.importRom(stream, romDirectory());
                runOnUiThread(() -> { if (!isDestroyed() && !isFinishing()) launch(); });
            } catch (IOException | SecurityException error) {
                runOnUiThread(() -> {
                    if (isDestroyed() || isFinishing()) return;
                    importing = false;
                    choose.setEnabled(true);
                    status.setText(error.getMessage() != null ? error.getMessage() : "No se pudo importar la ROM.");
                });
            }
        });
    }
    private void showInterruptedRun() {
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setGravity(Gravity.CENTER);
        int padding = Math.round(24 * getResources().getDisplayMetrics().density);
        content.setPadding(padding, padding, padding, padding);
        TextView message = new TextView(this);
        message.setText("No se pudo completar el inicio anterior.\nPuedes copiar el diagnóstico o volver a intentarlo.");
        message.setTextSize(18);
        message.setGravity(Gravity.CENTER);
        Button copy = new Button(this);
        copy.setText("Copiar diagnóstico");
        copy.setOnClickListener(v -> {
            android.content.ClipboardManager clipboard = (android.content.ClipboardManager)getSystemService(CLIPBOARD_SERVICE);
            if (clipboard != null) clipboard.setPrimaryClip(android.content.ClipData.newPlainText("Conker: diagnóstico", StartupDiagnostics.report()));
            Toast.makeText(this, "Diagnóstico copiado", Toast.LENGTH_SHORT).show();
        });
        Button retry = new Button(this);
        File rom = new File(romDirectory(), RomImporter.ROM_NAME);
        boolean hasRom = rom.isFile() && rom.length() == RomImporter.ROM_SIZE;
        retry.setText(hasRom ? "Volver a entrar" : "Continuar a importar ROM");
        retry.setOnClickListener(v -> {
            if (hasRom) launch();
            else { StartupDiagnostics.acknowledge(); recreate(); }
        });
        content.addView(message);
        content.addView(copy);
        content.addView(retry);
        setContentView(content);
    }
    private void launch() {
        StartupDiagnostics.beginLaunch();
        try {
            // A string component keeps native/SDL classes out of the launcher's class loading path.
            startActivity(new Intent().setClassName(this, "com.ylports.cbfd.GameActivity"));
            finish();
        } catch (RuntimeException | LinkageError error) {
            StartupDiagnostics.failure("No se pudo abrir GameActivity", error);
            showInterruptedRun();
        }
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused) Immersive.apply(this);
    }
    @Override protected void onDestroy() { worker.shutdownNow(); super.onDestroy(); }
}
