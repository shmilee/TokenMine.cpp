#include "core/sound.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <fstream>

#include "core/logger.hpp"

namespace mc {

namespace {

constexpr int kSampleRate = 44100;

std::uint32_t readU32(const std::vector<std::uint8_t>& d, std::size_t off) {
    return static_cast<std::uint32_t>(d[off]) |
           (static_cast<std::uint32_t>(d[off + 1]) << 8) |
           (static_cast<std::uint32_t>(d[off + 2]) << 16) |
           (static_cast<std::uint32_t>(d[off + 3]) << 24);
}

std::uint16_t readU16(const std::vector<std::uint8_t>& d, std::size_t off) {
    return static_cast<std::uint16_t>(d[off] | (d[off + 1] << 8));
}

bool tagAt(const std::vector<std::uint8_t>& d, std::size_t off,
           const char* tag) {
    return d.size() >= off + 4 &&
           std::string_view(reinterpret_cast<const char*>(d.data() + off), 4) ==
               std::string_view(tag, 4);
}

} // namespace

Result<WavSample> SoundBank::parseWav(const std::vector<std::uint8_t>& data) {
    if (data.size() < 12 || !tagAt(data, 0, "RIFF") || !tagAt(data, 8, "WAVE")) {
        return MC_FAIL("not a RIFF/WAVE file");
    }

    WavSample out;
    bool hasFmt = false;
    std::size_t off = 12;
    while (off + 8 <= data.size()) {
        const std::uint32_t chunkSize = readU32(data, off + 4);
        const std::size_t body = off + 8;
        if (body > data.size()) {
            return MC_FAIL("truncated chunk at offset " +
                           std::to_string(off));
        }
        if (tagAt(data, off, "fmt ")) {
            if (chunkSize < 16) {
                return MC_FAIL("'fmt' chunk too small");
            }
            const std::uint16_t format = readU16(data, body);
            const std::uint16_t channels = readU16(data, body + 2);
            const std::uint32_t rate = readU32(data, body + 4);
            const std::uint16_t bits = readU16(data, body + 14);
            if (format != 1) {
                return MC_FAIL("unsupported WAV audio format " +
                               std::to_string(format) + " (need PCM)");
            }
            if (channels != 1) {
                return MC_FAIL("unsupported channel count " +
                               std::to_string(channels) + " (need mono)");
            }
            if (bits != 16) {
                return MC_FAIL("unsupported bit depth " +
                               std::to_string(bits) + " (need 16)");
            }
            if (rate != static_cast<std::uint32_t>(kSampleRate)) {
                return MC_FAIL("unsupported sample rate " +
                               std::to_string(rate) +
                               " (need " + std::to_string(kSampleRate) +
                               " — regenerate with tools/generate_sounds.py)");
            }
            out.rate = kSampleRate;
            hasFmt = true;
        } else if (tagAt(data, off, "data")) {
            if (body + chunkSize > data.size()) {
                return MC_FAIL("truncated 'data' chunk");
            }
            if (chunkSize % 2 != 0) {
                return MC_FAIL("'data' chunk has an odd byte count");
            }
            out.pcm.resize(chunkSize / 2);
            for (std::size_t i = 0; i < out.pcm.size(); ++i) {
                out.pcm[i] = static_cast<std::int16_t>(
                    static_cast<std::uint16_t>(data[body + 2 * i] |
                                               (data[body + 2 * i + 1] << 8)));
            }
        }
        // Unknown chunks (LIST, fact, ...) are skipped; chunks are 2-byte
        // aligned per the RIFF spec.
        off = body + chunkSize + (chunkSize & 1);
    }
    if (!hasFmt) {
        return MC_FAIL("missing 'fmt' chunk");
    }
    if (out.pcm.empty()) {
        return MC_FAIL("missing or empty 'data' chunk");
    }
    return out;
}

std::vector<std::int16_t> SoundBank::applyGain(
    const std::vector<std::int16_t>& pcm, float gain) {
    std::vector<std::int16_t> out(pcm.size());
    for (std::size_t i = 0; i < pcm.size(); ++i) {
        const float v = static_cast<float>(pcm[i]) * gain;
        out[i] = static_cast<std::int16_t>(
            std::clamp(v, -32768.0f, 32767.0f));
    }
    return out;
}

// ---------------------------------------------------------------------------

SoundBank::~SoundBank() {
    shutdown();
}

void SoundBank::init(AssetManager& assets, const std::string& dir,
                     float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        MC_LOG_WARN("Audio subsystem unavailable (" +
                    std::string(SDL_GetError()) + ") — running silent");
        return;
    }
    m_ownsAudioSubSystem = true;

    SDL_AudioSpec spec {};
    spec.freq = kSampleRate;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 1;
    // NULL callback: the game queues samples with SDL_PutAudioStreamData
    // (SDL mixes the queued data and closes the device when the stream is
    // destroyed).
    m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                         &spec, nullptr, nullptr);
    if (m_stream == nullptr) {
        MC_LOG_WARN("No audio device (" + std::string(SDL_GetError()) +
                    ") — running silent");
        shutdown();
        return;
    }
    // The generated sounds all share one format; fix the stream input
    // format explicitly (the device output format is whatever SDL chose).
    SDL_AudioSpec inSpec {};
    inSpec.freq = kSampleRate;
    inSpec.format = SDL_AUDIO_S16;
    inSpec.channels = 1;
    SDL_SetAudioStreamFormat(m_stream, &inSpec, nullptr);

    // Load every .wav in the sound directory (sorted: deterministic order).
    auto dirPath = assets.resolve(dir);
    if (!dirPath.ok()) {
        MC_LOG_WARN("Sound directory not found: '" + dir + "' — running "
                    "silent");
        shutdown();
        return;
    }
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(*dirPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".wav") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            MC_LOG_WARN("Cannot open sound file: '" + file.string() + "'");
            continue;
        }
        std::string bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
        std::vector<std::uint8_t> raw(bytes.begin(), bytes.end());
        auto sample = parseWav(raw);
        if (!sample.ok()) {
            MC_LOG_WARN("Rejected sound '" + file.filename().string() + "': " +
                        sample.error().describe());
            continue;
        }
        m_samples.emplace(file.stem().string(), std::move(*sample));
    }

    m_enabled = true;
    MC_LOG_INFO("Audio ready: " + std::to_string(m_samples.size()) +
                " samples, volume " + std::to_string(m_volume));
}

void SoundBank::shutdown() {
    // SDL_DestroyAudioStream also closes the device it was opened with.
    if (m_stream != nullptr) {
        SDL_DestroyAudioStream(m_stream);
        m_stream = nullptr;
    }
    if (m_ownsAudioSubSystem) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_ownsAudioSubSystem = false;
    }
    m_samples.clear();
    m_enabled = false;
}

void SoundBank::play(const std::string& name) {
    if (!m_enabled) {
        return;
    }
    const auto it = m_samples.find(name);
    if (it == m_samples.end()) {
        return; // unknown name: silent no-op (documented)
    }
    const auto scaled = applyGain(it->second.pcm, m_volume);
    SDL_PutAudioStreamData(m_stream, scaled.data(),
                           static_cast<std::size_t>(scaled.size()) * 2);
}

} // namespace mc
