package com.ylports.cbfd;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;

/** Bounded, private evidence that survives short launches. No Android dependencies. */
final class DiagnosticFiles {
    static final int LIMIT = 48000;

    static String tail(File file, int limit) {
        try (RandomAccessFile in = new RandomAccessFile(file, "r")) {
            long length = in.length();
            in.seek(Math.max(0, length - limit));
            byte[] bytes = new byte[(int)Math.min(length, limit)];
            in.readFully(bytes);
            return new String(bytes, StandardCharsets.UTF_8);
        } catch (IOException | RuntimeException e) { return "Sin registro: " + e.getMessage() + "\n"; }
    }

    static void write(File file, String text) throws IOException {
        Files.createDirectories(file.toPath().getParent());
        File temporary = new File(file.getPath() + ".tmp");
        try (FileOutputStream out = new FileOutputStream(temporary)) {
            out.write(text.getBytes(StandardCharsets.UTF_8));
            out.getFD().sync();
        }
        try {
            Files.move(temporary.toPath(), file.toPath(), StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
        } catch (AtomicMoveNotSupportedException e) {
            Files.move(temporary.toPath(), file.toPath(), StandardCopyOption.REPLACE_EXISTING);
        }
    }

    static void preserveGameplay(File directory) throws IOException {
        File saved = new File(directory, "last-gameplay.log");
        // Examine both: upgrading from an older app may leave the useful run in previous-run.
        for (String name : new String[]{"previous-run.log", "last-run.log"}) {
            File source = new File(directory, name);
            if (!source.isFile() || (saved.isFile() && source.lastModified() <= saved.lastModified())) continue;
            String text = tail(source, LIMIT);
            if (!text.contains("[perf]")) continue;
            String firstLine;
            try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(source), StandardCharsets.UTF_8))) {
                firstLine = in.readLine();
            }
            write(saved, "Archived gameplay; source=" + name + "; modified=" + source.lastModified() + "\n"
                + firstLine + "\n" + text);
            // Compare session file dates, not the time at which this copy was made.
            saved.setLastModified(source.lastModified());
        }
    }

    static void capture(File directory, String text) throws IOException {
        File latest = new File(directory, "render-capture.log");
        if (latest.isFile()) write(new File(directory, "render-capture.previous.log"), tail(latest, LIMIT));
        write(latest, text.length() <= LIMIT ? text : text.substring(0, LIMIT) + "\n[truncated]\n");
    }
    private DiagnosticFiles() {}
}
