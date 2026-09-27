package com.ylports.cbfd;

import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;

/** Private, content-addressed ROM copies. Active ROM replacement is atomic. */
public final class RomVersions {
    // Same code interval as host/src/rom_versions.cpp in PC V0.1.1.
    private static final int CODE_START = 0x40, CODE_END = 0x1A37E0;
    private static final String US_CODE_SHA256 = "c0a5dc3ba972458d10eb7723777083f60ddf0880b23e09ad94295afda67d1848";
    private RomVersions() {}

    public static boolean hasRom(File directory) {
        File active = new File(directory, RomImporter.ROM_NAME);
        return active.isFile() && active.length() == RomImporter.ROM_SIZE;
    }

    static void validate(File rom) throws IOException {
        if (rom.length() != RomImporter.ROM_SIZE ||
            !digestRange(rom, "SHA-256", CODE_START, CODE_END).equals(US_CODE_SHA256)) {
            throw new IOException("ROM no compatible. Usa la versión USA original o una modificación que conserve su código y cambie solo textos, sonidos o gráficos.");
        }
    }

    static String digestRange(File file, String algorithm, long start, long end) throws IOException {
        MessageDigest digest;
        try { digest = MessageDigest.getInstance(algorithm); }
        catch (NoSuchAlgorithmException error) { throw new IOException(error); }
        try (RandomAccessFile input = new RandomAccessFile(file, "r")) {
            input.seek(start);
            byte[] buffer = new byte[65536];
            for (long remaining = end - start; remaining > 0;) {
                if (Thread.currentThread().isInterrupted()) throw new InterruptedIOException("Operación cancelada.");
                int count = input.read(buffer, 0, (int)Math.min(remaining, buffer.length));
                if (count < 0) throw new EOFException("La ROM está incompleta.");
                digest.update(buffer, 0, count);
                remaining -= count;
            }
        }
        return RomImporter.hex(digest.digest());
    }

    private static String hash(File rom) throws IOException {
        return digestRange(rom, "SHA-1", 0, rom.length());
    }

    private static void copyAtomic(Path from, Path to) throws IOException {
        Files.createDirectories(to.getParent());
        Path temporary = Files.createTempFile(to.getParent(), ".rom-", ".tmp");
        try {
            Files.copy(from, temporary, StandardCopyOption.REPLACE_EXISTING);
            try (RandomAccessFile copy = new RandomAccessFile(temporary.toFile(), "rw")) { copy.getFD().sync(); }
            Files.move(temporary, to, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        } finally { Files.deleteIfExists(temporary); }
    }

    private static void archive(File rom, File directory) throws IOException {
        Path kept = new File(new File(directory, "versions"), hash(rom) + ".z64").toPath();
        // Always use a verified copy. This also repairs a previously damaged archive.
        if (!Files.exists(kept) || Files.size(kept) != rom.length() || !hash(kept.toFile()).equals(hash(rom))) {
            copyAtomic(rom.toPath(), kept);
        }
    }

    static File commit(Path prepared, File directory) throws IOException {
        File active = new File(directory, RomImporter.ROM_NAME);
        if (hasRom(directory)) {
            boolean compatible = true;
            try { validate(active); } catch (IOException error) { compatible = false; }
            // A damaged old import must not prevent replacing it with a valid one.
            if (compatible) archive(active, directory);
        }
        archive(prepared.toFile(), directory);
        Files.move(prepared, active.toPath(), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        return active;
    }

    public static File[] list(File directory) throws IOException {
        if (hasRom(directory)) {
            File active = new File(directory, RomImporter.ROM_NAME);
            validate(active);
            archive(active, directory);
        }
        File[] files = new File(directory, "versions").listFiles((dir, name) -> name.matches("[0-9a-f]{40}\\.z64"));
        if (files == null) return new File[0];
        Arrays.sort(files, Comparator.comparing((File f) -> !f.getName().startsWith(RomImporter.ROM_SHA1))
            .thenComparing(File::getName));
        return files;
    }

    public static String label(File version) {
        String id = version.getName();
        return id.startsWith(RomImporter.ROM_SHA1) ? "USA · Original" : "USA · ROM modificada (" + id.substring(0, 8) + ")";
    }

    public static String currentLabel(File directory) throws IOException {
        if (!hasRom(directory)) return "Sin ROM importada";
        return label(new File(hash(new File(directory, RomImporter.ROM_NAME)) + ".z64"));
    }

    public static void select(File version, File directory) throws IOException {
        File versions = new File(directory, "versions").getCanonicalFile();
        if (!versions.equals(version.getCanonicalFile().getParentFile())) throw new IOException("Versión no válida.");
        validate(version);
        if (!version.getName().equals(hash(version) + ".z64")) throw new IOException("La copia de la ROM está dañada. Vuelve a importarla.");
        copyAtomic(version.toPath(), new File(directory, RomImporter.ROM_NAME).toPath());
    }
}
