// Android owns the storage root. Never let RT64 discover a desktop HOME.
#pragma once
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace conker::android {
inline std::filesystem::path rt64_data_path(const std::filesystem::path& state) {
    namespace fs = std::filesystem;
    if (state.empty() || !state.is_absolute()) {
        throw std::runtime_error("RT64: falta una ruta privada absoluta de Android.");
    }
    std::error_code error;
    const fs::path root = fs::canonical(state, error);
    if (error || !fs::is_directory(root, error)) {
        throw std::runtime_error("RT64: no se puede acceder a la carpeta privada: " + state.string());
    }
    const fs::path requested = root / "rt64";
    fs::create_directories(requested, error);
    if (error) {
        throw std::runtime_error("RT64: no se puede crear " + requested.string() + ": " + error.message());
    }
    const fs::path resolved = fs::canonical(requested, error);
    if (error || resolved.parent_path() != root) {
        throw std::runtime_error("RT64: la carpeta del motor sale del almacenamiento privado.");
    }
    return resolved;
}

// Kept generic so the same policy can be tested without a ROM, SDL or a GPU.
// The production call instantiates this with RT64::ApplicationConfiguration.
template <class ApplicationConfiguration>
void configure_rt64_storage(ApplicationConfiguration& config, const std::filesystem::path& state) {
    // useConfigurationFile=false alone DOES NOT disable RT64's path detection.
    config.detectDataPath = false;
    config.useConfigurationFile = false;
    config.dataPath = rt64_data_path(state);
}
} // namespace conker::android
