// Tests only the shared storage policy. Does not execute Conker, RT64 or Vulkan.
#include "../native/rt64_storage.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
struct Configuration {
    fs::path dataPath;
    bool detectDataPath = true;
    bool useConfigurationFile = true;
};
int checks = 0;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
template <class F> void rejected(F&& f, const char* message) {
    bool caught = false;
    try { f(); } catch (const std::runtime_error&) { caught = true; }
    check(caught, message);
}
int main() {
    char pattern[] = "/tmp/conker-storage-test-XXXXXX";
    char* made = mkdtemp(pattern);
    if (!made) return 2;
    const fs::path tmp = made;
    try {
        const fs::path state = tmp / "app files" / "state";
        fs::create_directories(state);
        std::ofstream(state / "save.eep") << "preserve-me";
        // Reproduce the device's unsuitable desktop HOME, but never write to it.
        setenv("HOME", "/data", 1);
        Configuration config;
        conker::android::configure_rt64_storage(config, state);
        check(!config.detectDataPath, "Desktop HOME discovery remained enabled");
        check(!config.useConfigurationFile, "Fixed profile was not preserved");
        check(config.dataPath == fs::canonical(state) / "rt64", "Wrong private RT64 path");
        check(fs::is_directory(config.dataPath), "RT64 directory not created");
        check(config.dataPath != fs::path("/data/.rt64"), "Selected the forbidden desktop path");
        {
            std::ofstream log(config.dataPath / "rt64.log"); log << "test";
            check(log.good(), "Private RT64 log could not be written");
        }
        conker::android::configure_rt64_storage(config, state);
        check(fs::file_size(config.dataPath / "rt64.log") == 4, "Relaunch erased the RT64 log");
        check(fs::file_size(state / "save.eep") == 11, "Save data changed");
        unsetenv("HOME");
        Configuration no_home;
        conker::android::configure_rt64_storage(no_home, state);
        check(no_home.dataPath == config.dataPath, "Depends on HOME");
        rejected([&] { conker::android::rt64_data_path({}); }, "Accepted empty state path");
        rejected([&] { conker::android::rt64_data_path("state"); }, "Accepted relative state path");
        rejected([&] { conker::android::rt64_data_path(tmp / "missing"); }, "Accepted missing state");
        rejected([&] { conker::android::rt64_data_path(state / "save.eep"); }, "Accepted state file");
        const fs::path blocked = tmp / "blocked";
        fs::create_directory(blocked); std::ofstream(blocked / "rt64") << "file";
        rejected([&] { conker::android::rt64_data_path(blocked); }, "Accepted file instead of RT64 directory");
        const fs::path linked = tmp / "linked";
        fs::create_directory(linked); fs::create_directory_symlink(state, linked / "rt64");
        rejected([&] { conker::android::rt64_data_path(linked); }, "Accepted storage symlink escape");
        rejected([&] { conker::android::rt64_data_path("/proc"); }, "Ignored unwritable filesystem error");
        std::cout << "PASS: " << checks << " storage regression checks (no GPU/gameplay test)\n";
        fs::remove_all(tmp);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        fs::remove_all(tmp);
        return 1;
    }
}
