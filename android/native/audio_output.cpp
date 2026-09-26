// Android SDL audio, independent of RecompFrontend settings.
// Preserves the Conker host's stereo word order and one-buffer AI_LEN correction.
#include <cstdio>
#include <limits>
#include <mutex>
#include <vector>
#include <SDL.h>
#include "conker.hpp"

namespace {
std::mutex mutex;
SDL_AudioDeviceID device = 0;
std::vector<float> converted;
constexpr size_t channels = 2;
}
void conker::audio::set_frequency(uint32_t frequency) {
    std::lock_guard lock(mutex);
    if (device) { SDL_CloseAudioDevice(device); device = 0; }
    if (!frequency || frequency > 192000) return;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
    SDL_AudioSpec wanted{};
    wanted.freq = static_cast<int>(frequency);
    wanted.format = AUDIO_F32SYS;
    wanted.channels = channels;
    wanted.samples = 256;
    device = SDL_OpenAudioDevice(nullptr, 0, &wanted, nullptr, 0);
    if (!device) { std::fprintf(stderr, "[audio] %s\n", SDL_GetError()); return; }
    SDL_PauseAudioDevice(device, 0);
}
void conker::audio::queue_samples(int16_t* samples, size_t count) {
    std::lock_guard lock(mutex);
    if (!device || !samples || count < 2 || count > std::numeric_limits<Uint32>::max()/sizeof(float)) return;
    count &= ~size_t{1};
    converted.resize(count);
    for (size_t i = 0; i < count; i += channels) {
        converted[i] = samples[i+1] / 32768.0f;
        converted[i+1] = samples[i] / 32768.0f;
    }
    if (SDL_QueueAudio(device, converted.data(), static_cast<Uint32>(count*sizeof(float))) != 0)
        std::fprintf(stderr, "[audio] %s\n", SDL_GetError());
}
size_t conker::audio::get_frames_remaining() {
    std::lock_guard lock(mutex);
    if (!device) return 0;
    const size_t queued = SDL_GetQueuedAudioSize(device)/(channels*sizeof(float));
    constexpr size_t one_buffer = 736;
    return queued > one_buffer ? queued-one_buffer : 0;
}
