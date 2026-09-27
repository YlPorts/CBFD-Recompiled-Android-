package com.ylports.cbfd;

import java.io.*;
import java.nio.file.*;

public final class DiagnosticsTest {
    private static void require(boolean value, String why) { if (!value) throw new AssertionError(why); }
    public static void main(String[] args) throws IOException {
        Path directory = Files.createTempDirectory("conker-evidence-");
        try {
            File dir = directory.toFile(), current = new File(dir, "last-run.log"), previous = new File(dir, "previous-run.log");
            DiagnosticFiles.write(previous, "[startup] older-build\n[perf] gpuMs=41.2\n");
            previous.setLastModified(1000);
            DiagnosticFiles.write(current, "[startup] short launch\n"); current.setLastModified(2000);
            DiagnosticFiles.preserveGameplay(dir);
            File saved = new File(dir, "last-gameplay.log");
            require(DiagnosticFiles.tail(saved, 100000).contains("older-build"), "upgrade must rescue the previous measured session");
            for (int n = 0; n < 4; ++n) {
                DiagnosticFiles.write(previous, "[startup] quick restart\n"); previous.setLastModified(3000 + n);
                DiagnosticFiles.write(current, "[startup] quick restart\n"); current.setLastModified(4000 + n);
                DiagnosticFiles.preserveGameplay(dir);
                require(DiagnosticFiles.tail(saved, 100000).contains("gpuMs=41.2"), "short launches must not overwrite gameplay");
            }
            DiagnosticFiles.write(current, "[startup] new-build\n" + "x".repeat(100000) + "\n[perf] gpuMs=30.0\n");
            current.setLastModified(5000); DiagnosticFiles.preserveGameplay(dir);
            String archived = DiagnosticFiles.tail(saved, 100000);
            require(archived.contains("new-build") && archived.contains("gpuMs=30.0"), "new measured session must replace old and retain build");
            require(saved.length() < DiagnosticFiles.LIMIT + 512, "retention must be bounded");
            DiagnosticFiles.preserveGameplay(dir);
            require(archived.equals(DiagnosticFiles.tail(saved, 100000)), "repeated reports must not rewrite evidence");
            DiagnosticFiles.capture(dir, "visible-water"); DiagnosticFiles.capture(dir, "missing-water");
            require(DiagnosticFiles.tail(new File(dir, "render-capture.previous.log"), 100).equals("visible-water"), "retain both comparison captures");
            require(DiagnosticFiles.tail(new File(dir, "render-capture.log"), 100).equals("missing-water"), "retain current capture");
            require(!new File(dir, "render-capture.log.tmp").exists(), "atomic write must finish");

            StartGesture s = new StartGesture();
            s.update(0x1000, 1, true, false);
            require(s.waiting() && s.filter(0x1000) == 0, "START must not pause before a diagnostic hold");
            require(s.capture() && !s.capture(), "one capture per hold");
            s.update(0x1000, 1, false, false);
            require(s.filter(0x1000) == 0, "long hold stays consumed");
            s.update(0, 0, false, true); require(s.filter(0) == 0, "long release must not pause");
            s.update(0x1000, 1, true, false); s.update(0, 0, false, true);
            require(s.pulse() && s.filter(0) == 0x1000, "tap must deliver START long enough for the game's input polling");
            s.endPulse(); require(s.filter(0) == 0, "tap pulse must release");
            s.update(0x1000, 1, true, false); s.update(0x9000, 2, true, false);
            require(!s.capture() && s.filter(0x9000) == 0x9000, "combined inputs must remain usable");
            s.cancel(); s.update(0x1000, 1, true, false); s.update(0, 1, false, false); s.update(0, 0, false, true);
            require(!s.pulse() && !s.capture(), "sliding off is cancellation, not a tap");
            s.update(0x1000, 1, true, false); s.cancel();
            require(!s.capture() && s.filter(0) == 0, "focus loss cancels delayed input");
            System.out.println("Diagnostic retention, comparison captures and START gesture regressions passed.");
        } finally {
            try (var files = Files.list(directory)) { for (Path file : files.toList()) Files.delete(file); }
            Files.delete(directory);
        }
    }
}
