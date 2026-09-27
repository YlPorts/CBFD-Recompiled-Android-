package com.ylports.cbfd;

import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.util.Locale;
import java.util.zip.*;

/** Streaming N64 byte-order conversion; never trusts a filename or content URI. */
public final class RomImporter {
    public static final long ROM_SIZE = 64L * 1024 * 1024;
    public static final String ROM_SHA1 = "4cbadd3c4e0729dec46af64ad018050eada4f47a";
    public static final String ROM_NAME = "conker.us.z64";
    private RomImporter() {}

    public static File importRom(InputStream input, File directory) throws IOException {
        return importContainerValidated(input, directory, ROM_SIZE, null);
    }

    // Package-private so synthetic tests need no copyrighted game data.
    static File importContainerValidated(InputStream input, File directory, long size, String hash) throws IOException {
        if (input == null) throw new IOException("No se pudo abrir la ROM.");
        PushbackInputStream source = new PushbackInputStream(new BufferedInputStream(input, 65536), 4);
        byte[] prefix = new byte[4];
        int count = 0;
        while (count < 4) {
            int value = source.read();
            if (value < 0) break;
            prefix[count++] = (byte) value;
        }
        source.unread(prefix, 0, count);
        if (count != 4 || prefix[0] != 'P' || prefix[1] != 'K' || prefix[2] != 3 || prefix[3] != 4) {
            return importValidated(source, directory, size, hash);
        }
        Path prepared = null;
        int entries = 0;
        long otherBytes = 0;
        try (ZipInputStream zip = new ZipInputStream(source)) {
            byte[] discard = new byte[8192];
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                if (++entries > 128) throw new IOException("El ZIP contiene demasiados archivos.");
                String name = entry.getName().toLowerCase(Locale.ROOT);
                boolean rom = !entry.isDirectory() && (name.endsWith(".z64") || name.endsWith(".n64") || name.endsWith(".v64"));
                if (rom) {
                    if (prepared != null) throw new IOException("El ZIP contiene varias ROMs. Importa solo una.");
                    if (entry.getSize() >= 0 && entry.getSize() != size) throw new IOException("La ROM dentro del ZIP no tiene el tamaño correcto.");
                    prepared = prepare(zip, directory, size, hash);
                } else {
                    int read;
                    while ((read = zip.read(discard)) != -1) {
                        if (Thread.currentThread().isInterrupted()) throw new InterruptedIOException("Importación cancelada.");
                        otherBytes += read;
                        if (otherBytes > 8L * 1024 * 1024) throw new IOException("El ZIP contiene demasiados datos ajenos a la ROM.");
                    }
                }
                // Reading every entry to EOF also verifies its CRC. Never extract entry paths.
                zip.closeEntry();
            }
            if (prepared == null) throw new IOException("El ZIP no contiene una ROM .z64, .v64 o .n64.");
            return hash == null ? RomVersions.commit(prepared, directory) : commit(prepared, directory);
        } finally {
            if (prepared != null) Files.deleteIfExists(prepared);
        }
    }

    static File importValidated(InputStream input, File directory, long size, String expectedHash) throws IOException {
        Path prepared = prepare(input, directory, size, expectedHash);
        try { return expectedHash == null ? RomVersions.commit(prepared, directory) : commit(prepared, directory); }
        finally { Files.deleteIfExists(prepared); }
    }

    private static File commit(Path prepared, File directory) throws IOException {
        Path destination = directory.toPath().resolve(ROM_NAME);
        Files.move(prepared, destination, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        return destination.toFile();
    }

    private static Path prepare(InputStream input, File directory, long size, String expectedHash) throws IOException {
        if (input == null) throw new IOException("No se pudo abrir la ROM.");
        Files.createDirectories(directory.toPath());
        Path temporary = Files.createTempFile(directory.toPath(), ".rom-", ".tmp");
        boolean valid = false;
        try {
            MessageDigest digest;
            try { digest = MessageDigest.getInstance("SHA-1"); }
            catch (NoSuchAlgorithmException e) { throw new IOException("SHA-1 no está disponible.", e); }
            long count;
            try (FileOutputStream file = new FileOutputStream(temporary.toFile());
                 BufferedOutputStream output = new BufferedOutputStream(file, 65536)) {
                count = normalize(input, output, digest, size);
                output.flush();
                file.getFD().sync();
            }
            if (count != size || (expectedHash != null && !hex(digest.digest()).equals(expectedHash))) {
                throw new IOException("ROM no compatible. Necesitas Conker's Bad Fur Day USA sin modificar.");
            }
            if (expectedHash == null) RomVersions.validate(temporary.toFile());
            valid = true;
            return temporary;
        } finally {
            if (!valid) Files.deleteIfExists(temporary);
        }
    }

    static long normalize(InputStream input, OutputStream output, MessageDigest digest, long limit) throws IOException {
        byte[] buffer = new byte[65536];
        int pending = 0, order = -1;
        long total = 0;
        for (;;) {
            if (Thread.currentThread().isInterrupted()) throw new InterruptedIOException("Importación cancelada.");
            int read = input.read(buffer, pending, buffer.length - pending);
            if (read < 0) break;
            if (read == 0) { // Some content providers return short or zero-length reads.
                int one = input.read();
                if (one < 0) break;
                buffer[pending] = (byte) one;
                read = 1;
            }
            total += read;
            if (total > limit) throw new IOException("El archivo es demasiado grande para esta ROM.");
            int available = pending + read;
            int whole = available & ~3;
            if (whole > 0) {
                if (order < 0) {
                    int magic = ((buffer[0] & 255) << 24) | ((buffer[1] & 255) << 16)
                        | ((buffer[2] & 255) << 8) | (buffer[3] & 255);
                    if (magic == 0x80371240) order = 0; // z64
                    else if (magic == 0x37804012) order = 1; // v64
                    else if (magic == 0x40123780) order = 2; // n64
                    else throw new IOException("No es una ROM N64 compatible.");
                }
                for (int i = 0; i < whole; i += 4) {
                    if (order == 1) { swap(buffer, i, i + 1); swap(buffer, i + 2, i + 3); }
                    else if (order == 2) { swap(buffer, i, i + 3); swap(buffer, i + 1, i + 2); }
                }
                output.write(buffer, 0, whole);
                digest.update(buffer, 0, whole);
            }
            pending = available - whole;
            System.arraycopy(buffer, whole, buffer, 0, pending);
        }
        if (pending != 0 || order < 0) throw new IOException("La ROM está incompleta.");
        return total;
    }

    private static void swap(byte[] b, int a, int c) { byte v = b[a]; b[a] = b[c]; b[c] = v; }
    static String hex(byte[] bytes) {
        char[] alphabet = "0123456789abcdef".toCharArray();
        char[] out = new char[bytes.length * 2];
        for (int i = 0; i < bytes.length; i++) {
            out[2 * i] = alphabet[(bytes[i] & 255) >>> 4];
            out[2 * i + 1] = alphabet[bytes[i] & 15];
        }
        return new String(out);
    }
}
