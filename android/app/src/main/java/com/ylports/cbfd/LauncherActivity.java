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
        Immersive.apply(this);
        File rom = new File(romDirectory(), RomImporter.ROM_NAME);
        if (rom.isFile() && rom.length() == RomImporter.ROM_SIZE) { launch(); return; }
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setGravity(Gravity.CENTER);
        int padding = Math.round(32 * getResources().getDisplayMetrics().density);
        content.setPadding(padding, padding, padding, padding);
        status = new TextView(this);
        status.setText("Conker Recompiled\nSelecciona tu ROM USA para empezar.");
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
    private void launch() {
        startActivity(new Intent(this, GameActivity.class));
        finish();
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused) Immersive.apply(this);
    }
    @Override protected void onDestroy() { worker.shutdownNow(); super.onDestroy(); }
}
