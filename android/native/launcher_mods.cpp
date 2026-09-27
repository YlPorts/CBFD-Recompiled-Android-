// JNI bridge for the PC librecomp mod scanner/config used by the Android launcher.
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <jni.h>
#include "librecomp/mods.hpp"
#include "json/json.hpp"

namespace {
std::mutex modMutex;
std::unique_ptr<recomp::mods::ModContext> modContext;
std::filesystem::path currentState;

std::string jstring_to_string(JNIEnv* env,jstring value){
    if(!value)return {};
    const char* chars=env->GetStringUTFChars(value,nullptr);
    std::string out=chars?chars:"";
    if(chars)env->ReleaseStringUTFChars(value,chars);
    return out;
}

void prepare(const std::filesystem::path& state,bool rescan){
    if(state.empty())throw std::runtime_error("Missing Android state directory.");
    std::filesystem::create_directories(state/"mods");
    std::filesystem::create_directories(state/"mod_config");
    if(!modContext || currentState!=state){
        modContext=std::make_unique<recomp::mods::ModContext>();
        modContext->register_game("conker");
        modContext->set_mods_config_path(state/"mods.json");
        modContext->set_mod_config_directory(state/"mod_config");
        currentState=state;
        rescan=true;
    }
    if(rescan){
        modContext->close_mods();
        const auto errors=modContext->scan_mod_folder(state/"mods");
        modContext->load_mods_config();
        if(!errors.empty()){
            std::fprintf(stderr,"[launcher-mods] %zu mod(s) could not be opened\n",errors.size());
        }
    }
}
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_ylports_cbfd_LauncherMods_nativeList(JNIEnv* env,jclass,jstring statePath){
    try{
        std::lock_guard lock(modMutex);
        prepare(jstring_to_string(env,statePath),true);
        nlohmann::json root=nlohmann::json::array();
        for(const auto& mod:modContext->get_all_mod_details("conker")){
            root.push_back({
                {"id",mod.mod_id},
                {"name",mod.display_name},
                {"description",mod.short_description.empty()?mod.description:mod.short_description},
                {"version",mod.version.to_string()},
                {"enabled",modContext->is_mod_enabled(mod.mod_id)},
                {"toggleable",mod.runtime_toggleable},
                {"defaultEnabled",mod.enabled_by_default},
                {"customGamemode",mod.custom_gamemode},
                {"file",modContext->get_mod_filename(mod.mod_id).filename().string()}
            });
        }
        const std::string result=root.dump();
        return env->NewStringUTF(result.c_str());
    }catch(const std::exception& error){
        nlohmann::json root;
        root["error"]=error.what();
        const std::string result=root.dump();
        return env->NewStringUTF(result.c_str());
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_ylports_cbfd_LauncherMods_nativeSetEnabled(JNIEnv* env,jclass,jstring statePath,jstring modId,jboolean enabled){
    try{
        std::lock_guard lock(modMutex);
        prepare(jstring_to_string(env,statePath),false);
        const std::string id=jstring_to_string(env,modId);
        modContext->enable_mod(id,enabled==JNI_TRUE,true);
        return modContext->is_mod_enabled(id)?JNI_TRUE:JNI_FALSE;
    }catch(const std::exception& error){
        std::fprintf(stderr,"[launcher-mods] toggle failed: %s\n",error.what());
        return JNI_FALSE;
    }
}
