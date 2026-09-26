package com.ylports.cbfd;

import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.stream.Stream;

/** No ROM is included or needed: every fixture below is synthetic. */
public final class RomImporterTest {
    private static int passed;
    interface Check { void run() throws Exception; }
    private static void test(String name, Check action) throws Exception {
        action.run(); passed++; System.out.println("PASS " + name);
    }
    private static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    private static void fails(Check action) throws Exception {
        try { action.run(); } catch (IOException expected) { return; }
        throw new AssertionError("Expected IOException");
    }
    private static byte[] fixture(int length) {
        byte[] data = new byte[length];
        new Random(4319).nextBytes(data);
        data[0] = (byte)0x80; data[1] = 0x37; data[2] = 0x12; data[3] = 0x40;
        return data;
    }
    private static String sha(byte[] data) throws Exception {
        return RomImporter.hex(MessageDigest.getInstance("SHA-1").digest(data));
    }
    private static byte[] ordered(byte[] canonical, int order) {
        byte[] out = canonical.clone();
        for (int i = 0; i < out.length; i += 4) {
            if (order == 1) {
                out[i] = canonical[i+1]; out[i+1] = canonical[i];
                out[i+2] = canonical[i+3]; out[i+3] = canonical[i+2];
            } else if (order == 2) {
                for (int j = 0; j < 4; j++) out[i+j] = canonical[i+3-j];
            }
        }
        return out;
    }
    private static InputStream fragmented(byte[] bytes, int chunk, boolean zeroReads) {
        return new FilterInputStream(new ByteArrayInputStream(bytes)) {
            int calls;
            @Override public int read(byte[] b, int off, int len) throws IOException {
                if (zeroReads && ++calls % 3 == 0) return 0;
                return super.read(b, off, Math.min(chunk, len));
            }
        };
    }
    private static void verify(InputStream in, byte[] expected) throws Exception {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        MessageDigest digest = MessageDigest.getInstance("SHA-1");
        long count = RomImporter.normalize(in, out, digest, expected.length);
        require(count == expected.length, "Incorrect size");
        require(Arrays.equals(out.toByteArray(), expected), "Byte-order conversion changed content");
        require(RomImporter.hex(digest.digest()).equals(sha(expected)), "Digest not canonical");
    }
    private static void noTemporaryFiles(Path directory) throws IOException {
        try (Stream<Path> paths = Files.list(directory)) {
            require(paths.noneMatch(p -> p.getFileName().toString().startsWith(".rom-")), "Leaked temporary import");
        }
    }
    public static void main(String[] args) throws Exception {
        byte[] data = fixture(131084); // crosses two 64KiB conversion buffers
        for (int order = 0; order < 3; order++) {
            final int format = order;
            for (int chunk : new int[]{1, 2, 3, 5, 65535, 65536}) {
                test("order=" + order + " fragmented=" + chunk,
                    () -> verify(fragmented(ordered(data, format), chunk, false), data));
            }
            test("order=" + order + " zero-length reads",
                () -> verify(fragmented(ordered(data, format), 23, true), data));
        }
        test("empty input rejected", () -> fails(() -> verify(new ByteArrayInputStream(new byte[0]), data)));
        test("partial magic rejected", () -> fails(() -> verify(new ByteArrayInputStream(Arrays.copyOf(data, 3)), data)));
        test("partial final word rejected", () -> fails(() -> verify(new ByteArrayInputStream(Arrays.copyOf(data, data.length-1)), data)));
        test("ZIP/unknown magic rejected", () -> {
            byte[] invalid = data.clone(); invalid[0] = 0x50; invalid[1] = 0x4B;
            fails(() -> verify(new ByteArrayInputStream(invalid), data));
        });
        test("oversized stream rejected", () -> fails(() -> verify(new ByteArrayInputStream(fixture(data.length+4)), data)));
        test("cancellation rejected", () -> {
            Thread.currentThread().interrupt();
            try { fails(() -> verify(new ByteArrayInputStream(data), data)); }
            finally { Thread.interrupted(); }
        });
        Path directory = Files.createTempDirectory("conker-import-test-");
        try {
            test("canonical import committed", () -> {
                File result = RomImporter.importValidated(new ByteArrayInputStream(data), directory.toFile(), data.length, sha(data));
                require(Arrays.equals(Files.readAllBytes(result.toPath()), data), "Wrong saved bytes");
                noTemporaryFiles(directory);
            });
            test("valid replacement canonicalizes v64", () -> {
                RomImporter.importValidated(new ByteArrayInputStream(ordered(data, 1)), directory.toFile(), data.length, sha(data));
                require(Arrays.equals(Files.readAllBytes(directory.resolve(RomImporter.ROM_NAME)), data), "Wrong replacement");
                noTemporaryFiles(directory);
            });
            test("invalid replacement preserves previous ROM", () -> {
                byte[] invalid = data.clone(); invalid[12] ^= 1;
                fails(() -> RomImporter.importValidated(new ByteArrayInputStream(invalid), directory.toFile(), data.length, sha(data)));
                require(Arrays.equals(Files.readAllBytes(directory.resolve(RomImporter.ROM_NAME)), data), "Lost previous ROM");
                noTemporaryFiles(directory);
            });
            test("word-aligned truncated file rejected", () -> {
                byte[] partial = Arrays.copyOf(data, data.length-4);
                fails(() -> RomImporter.importValidated(new ByteArrayInputStream(partial), directory.toFile(), data.length, sha(data)));
                noTemporaryFiles(directory);
            });
            test("provider I/O failure cleaned up", () -> {
                InputStream broken = new InputStream() {
                    @Override public int read() throws IOException { throw new IOException("provider failed"); }
                };
                fails(() -> RomImporter.importValidated(broken, directory.toFile(), data.length, sha(data)));
                noTemporaryFiles(directory);
            });
            test("null stream rejected", () -> fails(() -> RomImporter.importValidated(null, directory.toFile(), data.length, sha(data))));
        } finally {
            try (Stream<Path> paths = Files.walk(directory)) {
                for (Path path : paths.sorted(Comparator.reverseOrder()).toList()) Files.delete(path);
            }
        }
        System.out.println("Passed " + passed + " synthetic tests. This is not a native runtime or FPS test.");
    }
}
