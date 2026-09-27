package com.ylports.cbfd;

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.stream.Stream;

/** Optional integration test against the owner's private USA ROM; never bundles it. */
public final class RomVersionsTest {
    private static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    private static void reject(RunnableIO operation) throws Exception {
        try { operation.run(); } catch (IOException expected) { return; }
        throw new AssertionError("Unsupported ROM accepted");
    }
    interface RunnableIO { void run() throws Exception; }
    public static void main(String[] args) throws Exception {
        Path original = Path.of(args[0]);
        Path dir = Files.createTempDirectory("conker-versions-");
        try {
            try (InputStream in = Files.newInputStream(original)) { RomImporter.importRom(in, dir.toFile()); }
            require(RomVersions.list(dir.toFile()).length == 1, "Original not archived");
            require(RomVersions.currentLabel(dir.toFile()).equals("USA · Original"), "Original not recognized");
            byte[] modified = Files.readAllBytes(original);
            modified[0x20] ^= 1; // Asset hacks may rename the header.
            modified[0x200000] ^= 1; // Outside the executable code.
            for (int order = 0; order < 3; order++) {
                byte[] input = modified.clone();
                for (int i = 0; i < input.length; i += 4) {
                    if (order == 1) {
                        input[i] = modified[i + 1]; input[i + 1] = modified[i];
                        input[i + 2] = modified[i + 3]; input[i + 3] = modified[i + 2];
                    } else if (order == 2) {
                        for (int j = 0; j < 4; j++) input[i + j] = modified[i + 3 - j];
                    }
                }
                RomImporter.importRom(new ByteArrayInputStream(input), dir.toFile());
                require(Arrays.equals(modified, Files.readAllBytes(dir.resolve(RomImporter.ROM_NAME))), "Modified ROM not normalized");
                require(RomVersions.list(dir.toFile()).length == 2, "Repeated import duplicated a version");
            }
            File[] versions = RomVersions.list(dir.toFile());
            RomVersions.select(versions[0], dir.toFile());
            require(Files.mismatch(original, dir.resolve(RomImporter.ROM_NAME)) == -1, "Switch failed to restore original bytes");
            modified[0x1000] ^= 1;
            reject(() -> RomImporter.importRom(new ByteArrayInputStream(modified), dir.toFile()));
            require(Files.mismatch(original, dir.resolve(RomImporter.ROM_NAME)) == -1, "Rejected code modification replaced active ROM");
            try (RandomAccessFile file = new RandomAccessFile(versions[1], "rw")) { file.seek(0x200001); int value = file.read(); file.seek(0x200001); file.write(value ^ 1); }
            reject(() -> RomVersions.select(versions[1], dir.toFile()));
            require(Files.mismatch(original, dir.resolve(RomImporter.ROM_NAME)) == -1, "Corrupt archive replaced active ROM");
            reject(() -> RomVersions.select(original.toFile(), dir.toFile()));
            System.out.println("PASS ROM variants: 3 byte orders, asset/header edits, deduplication, atomic switching, executable and archive corruption rejection");
        } finally {
            try (Stream<Path> paths = Files.walk(dir)) {
                for (Path path : paths.sorted(Comparator.reverseOrder()).toList()) Files.delete(path);
            }
        }
    }
}
