package com.ylports.cbfd;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.*;
import java.io.*;
import java.util.concurrent.*;

/** Direct boot with an optional ROM manager, opened only after the game exits. */
public final class LauncherActivity extends Activity {
    static final String MANAGE_ROMS = "com.ylports.cbfd.MANAGE_ROMS";
    private static final int PICK_ROM = 10;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private TextView status;
    private Button choose, play, versions;
    private boolean importing, managing;
    private File[] available = new File[0];

    private File romDirectory() { return new File(getFilesDir(), "roms"); }
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        StartupDiagnostics.log("LauncherActivity.onCreate");
        managing = getIntent().getBooleanExtra(MANAGE_ROMS, false);
        if (managing) { showImporter(); return; }
        if (StartupDiagnostics.needsRecovery()) { showInterruptedRun(); return; }
        if (RendererSettings.needsChoice(this)) { managing = RomVersions.hasRom(romDirectory()); showImporter(); return; }
        if (RomVersions.hasRom(romDirectory())) { launch(); return; }
        showImporter();
    }
    private LinearLayout layout(int paddingDp) {
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setGravity(Gravity.CENTER);
        int padding = Math.round(paddingDp * getResources().getDisplayMetrics().density);
        content.setPadding(padding, padding, padding, padding);
        return content;
    }
    private void showImporter() {
        LinearLayout content = layout(24);
        status = new TextView(this);
        status.setText("Conker Recompiled " + StartupDiagnostics.VERSION + "\nSelecciona tu ROM USA o ZIP para empezar.");
        status.setTextSize(18);
        status.setGravity(Gravity.CENTER);
        choose = new Button(this);
        choose.setText(managing ? "Añadir ROM" : "Importar ROM");
        choose.setOnClickListener(v -> {
            if (importing) return;
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivityForResult(intent, PICK_ROM);
        });
        play = new Button(this);
        play.setText("Jugar");
        play.setOnClickListener(v -> { if (!importing) launch(); });
        versions = new Button(this);
        versions.setText("Cambiar versión");
        versions.setOnClickListener(v -> {
            if (importing || available.length == 0) return;
            String[] labels = new String[available.length];
            for (int i = 0; i < labels.length; i++) labels[i] = RomVersions.label(available[i]);
            new AlertDialog.Builder(this).setTitle("Versión de ROM").setItems(labels, (dialog, index) -> {
                File selected = available[index];
                setBusy(true);
                worker.submit(() -> {
                    try { RomVersions.select(selected, romDirectory()); refreshVersions(); }
                    catch (IOException error) { showError(error); }
                });
            }).show();
        });
        content.addView(status);
        content.addView(choose);
        content.addView(versions);
        Button graphics = new Button(this);
        graphics.setText("Gráficos: " + RendererSettings.label(this));
        graphics.setOnClickListener(v -> RendererSettings.show(this,
            () -> graphics.setText("Gráficos: " + RendererSettings.label(this))));
        content.addView(graphics);
        TextView rendererHelp = new TextView(this);
        rendererHelp.setText("OpenGL es experimental y mantiene los FPS originales del juego. Vulkan permite interpolación. Ambos conservan tus partidas.");
        rendererHelp.setGravity(Gravity.CENTER);
        content.addView(rendererHelp);
        content.addView(play);
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(content);
        setContentView(scroll);
        setBusy(true);
        worker.submit(this::refreshVersions);
        content.post(() -> Immersive.apply(this));
    }
    private void setBusy(boolean busy) {
        importing = busy;
        choose.setEnabled(!busy);
        play.setVisibility(RomVersions.hasRom(romDirectory()) ? android.view.View.VISIBLE : android.view.View.GONE);
        play.setEnabled(!busy);
        versions.setVisibility(managing ? android.view.View.VISIBLE : android.view.View.GONE);
        versions.setEnabled(!busy && available.length > 1);
    }
    private void refreshVersions() {
        try {
            File[] found = RomVersions.list(romDirectory());
            String current = RomVersions.currentLabel(romDirectory());
            runOnUiThread(() -> {
                if (isDestroyed() || isFinishing()) return;
                available = found;
                setBusy(false);
                status.setText("Conker Recompiled " + StartupDiagnostics.VERSION + "\n" + current + "\nROM USA original o con cambios de textos, sonidos y gráficos.");
            });
        } catch (IOException error) { showError(error); }
    }
    private void showError(Exception error) {
        runOnUiThread(() -> {
            if (isDestroyed() || isFinishing()) return;
            setBusy(false);
            status.setText(error.getMessage() != null ? error.getMessage() : "No se pudo importar la ROM.");
        });
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_ROM || result != RESULT_OK || data == null || data.getData() == null || importing) return;
        setBusy(true);
        status.setText("Comprobando ROM…");
        final android.net.Uri uri = data.getData();
        worker.submit(() -> {
            try (InputStream stream = getContentResolver().openInputStream(uri)) {
                RomImporter.importRom(stream, romDirectory());
                if (managing) refreshVersions();
                else runOnUiThread(() -> { if (!isDestroyed() && !isFinishing()) launch(); });
            } catch (IOException | SecurityException error) { showError(error); }
        });
    }
    private void showInterruptedRun() {
        LinearLayout content = layout(24);
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
        boolean hasRom = RomVersions.hasRom(romDirectory());
        retry.setText(hasRom ? "Volver a entrar" : "Continuar a importar ROM");
        retry.setOnClickListener(v -> {
            if (hasRom) launch();
            else { StartupDiagnostics.acknowledge(); showImporter(); }
        });
        Button change = new Button(this);
        change.setText("Gráficos y ROM");
        change.setOnClickListener(v -> { managing = true; showImporter(); });
        content.addView(message);
        content.addView(copy);
        content.addView(retry);
        content.addView(change);
        setContentView(content);
    }
    private void launch() {
        RendererSettings.markShown(this);
        StartupDiagnostics.beginLaunch();
        try {
            startActivity(new Intent().setClassName(this, "com.ylports.cbfd.GameActivity")
                .putExtra(RendererSettings.EXTRA, RendererSettings.selected(this)));
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
