// The ROMs the game plays, and switching between versions of them.
//
// The recompiled code comes from the US ROM's code (up to the end of .debugger), so any
// ROM whose code is the same plays: the US ROM itself, or a ROM hack that only changes
// the game's assets, such as one that restores the words the original bleeps. The runtime
// keeps the ROM in play in the data folder (<game id>.z64). Each ROM loaded is also kept
// in rom_versions/ there, so the launcher can switch between them by copying one over it.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#define XXH_INLINE_ALL
#include "xxHash/xxhash.h"

#include "conker.hpp"

namespace {
    constexpr size_t code_start = 0x40;    // after the header, which a hack may rename
    constexpr size_t code_end = 0x1A37E0;  // the end of .debugger
    constexpr uint64_t us_code_hash = 0x3C7B12470FFFD01DULL; // XXH3-64 of the US ROM from code_start to code_end

    // Versions with a name of their own, by the XXH3-64 of the whole ROM (big-endian .z64).
    // Any other ROM the game accepts is a "ROM hack". Every one of them is the US version
    // (its code has to be the US ROM's), which the names say.
    struct KnownVersion {
        uint64_t hash;
        const char* slug;
        const char* name;
    };
    constexpr KnownVersion known_versions[] = {
        { conker::roms::us_rom_hash, "original", "US Original" },
        { 0xAC445026C8F77A94ULL, "uncensored", "US Uncensored" }, // the bleeped words restored
    };

    std::filesystem::path stored_path;   // the runtime's ROM in play
    std::filesystem::path versions_dir;  // every version loaded, as <slug>.z64
    std::filesystem::file_time_type stored_time{};
    uintmax_t stored_size = 0;
    std::string current_slug;            // the version in play; empty with no ROM

    std::string slug_of(uint64_t hash) {
        for (const KnownVersion& known : known_versions) {
            if (known.hash == hash) {
                return known.slug;
            }
        }
        char slug[32];
        std::snprintf(slug, sizeof(slug), "hack-%016llx", (unsigned long long)hash);
        return slug;
    }

    std::string name_of(const std::string& slug) {
        for (const KnownVersion& known : known_versions) {
            if (slug == known.slug) {
                return known.name;
            }
        }
        // hack-<hash>: the hash's first 8 digits tell hacks apart.
        return "US ROM hack (" + slug.substr(5, 8) + ")";
    }

    // The versions kept, named ones first in the order above, then hacks.
    std::vector<std::string> kept_slugs() {
        std::vector<std::string> slugs;
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(versions_dir, error)) {
            if (entry.path().extension() == ".z64") {
                slugs.push_back(entry.path().stem().string());
            }
        }
        auto rank = [](const std::string& slug) {
            for (size_t i = 0; i < std::size(known_versions); i++) {
                if (slug == known_versions[i].slug) {
                    return i;
                }
            }
            return std::size(known_versions);
        };
        std::sort(slugs.begin(), slugs.end(), [&](const std::string& a, const std::string& b) {
            return rank(a) != rank(b) ? rank(a) < rank(b) : a < b;
        });
        return slugs;
    }

    std::vector<uint8_t> read_file(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
}

bool conker::roms::accept(std::span<const uint8_t> rom) {
    if (XXH3_64bits(rom.data(), rom.size()) == us_rom_hash) {
        return true;
    }
    if (rom.size() < code_end || XXH3_64bits(rom.data() + code_start, code_end - code_start) != us_code_hash) {
        return false;
    }
    static std::atomic<bool> reported{ false };
    if (!reported.exchange(true)) {
        std::printf("[host] The ROM's assets differ from the US ROM's (a ROM hack): playing with them.\n");
    }
    return true;
}

namespace {
    // Takes in the stored ROM if it changed. settle: wait until it's been left alone for a moment.
    bool refresh(bool settle);
}

void conker::roms::init(const std::filesystem::path& stored_rom) {
    stored_path = stored_rom;
    versions_dir = stored_rom.parent_path() / "rom_versions";
    // Nothing is writing the ROM at startup, so there's no need to wait for it.
    refresh(false);
}

bool conker::roms::update() {
    return refresh(true);
}

namespace {
    bool refresh(bool settle) {
        std::error_code error;
        uintmax_t size = std::filesystem::file_size(stored_path, error);
        if (error) {
            bool changed = !current_slug.empty();
            current_slug.clear();
            stored_size = 0;
            stored_time = {};
            return changed;
        }
        std::filesystem::file_time_type time = std::filesystem::last_write_time(stored_path, error);
        if (error || (time == stored_time && size == stored_size)) {
            return false;
        }
        // Load ROM writes the ROM on another thread: wait until it's been left alone for a moment.
        if (settle && std::filesystem::file_time_type::clock::now() - time < std::chrono::seconds(1)) {
            return false;
        }
        stored_time = time;
        stored_size = size;
        std::vector<uint8_t> rom = read_file(stored_path);
        if (!conker::roms::accept(rom)) {
            // The runtime refuses it too (and removes it) when the game starts.
            bool changed = !current_slug.empty();
            current_slug.clear();
            return changed;
        }
        std::string slug = slug_of(XXH3_64bits(rom.data(), rom.size()));
        std::filesystem::path kept = versions_dir / (slug + ".z64");
        if (!std::filesystem::exists(kept, error)) {
            std::filesystem::create_directories(versions_dir, error);
            std::filesystem::path partial = kept;
            partial += ".part";
            std::ofstream out(partial, std::ios::binary);
            out.write(reinterpret_cast<const char*>(rom.data()), (std::streamsize)rom.size());
            out.close();
            if (out) {
                std::filesystem::rename(partial, kept, error);
            } else {
                std::filesystem::remove(partial, error);
            }
        }
        bool changed = slug != current_slug;
        current_slug = slug;
        return changed;
    }
}

size_t conker::roms::version_count() {
    return kept_slugs().size();
}

std::string conker::roms::current_name() {
    return current_slug.empty() ? std::string() : name_of(current_slug);
}

std::string conker::roms::region_of(const std::filesystem::path& rom_path) {
    uint8_t header[0x40];
    std::ifstream file(rom_path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(header), sizeof(header))) {
        return {};
    }
    // The country code is at 0x3E in a big-endian .z64. Byteswapped (.v64) and
    // little-endian (.n64) ROMs have it elsewhere in the word, told by the first bytes.
    uint8_t code;
    if (header[0] == 0x80) {
        code = header[0x3E];
    } else if (header[0] == 0x37) {
        code = header[0x3F];
    } else if (header[0] == 0x40) {
        code = header[0x3D];
    } else {
        return {};
    }
    switch (code) {
        case 'E': return "US";
        case 'P': case 'X': case 'Y': return "European";
        case 'D': return "German";
        case 'F': return "French";
        case 'J': return "Japanese";
        case 'U': return "Australian";
        default: return {};
    }
}

bool conker::roms::switch_to_next() {
    std::vector<std::string> slugs = kept_slugs();
    if (slugs.size() < 2) {
        return false;
    }
    auto it = std::find(slugs.begin(), slugs.end(), current_slug);
    const std::string& next = (it == slugs.end() || it + 1 == slugs.end()) ? slugs.front() : *(it + 1);
    std::error_code error;
    std::filesystem::copy_file(versions_dir / (next + ".z64"), stored_path,
        std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        std::fprintf(stderr, "[host] Couldn't switch to the %s ROM: %s\n", name_of(next).c_str(), error.message().c_str());
        return false;
    }
    current_slug = next;
    stored_time = std::filesystem::last_write_time(stored_path, error);
    stored_size = std::filesystem::file_size(stored_path, error);
    return true;
}
