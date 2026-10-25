#include "core/sound.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
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

float SoundBank::distanceGain(float dist, float cutoff) {
    if (cutoff <= 0.0f) {
        return dist <= 0.0f ? 1.0f : 0.0f;
    }
    return std::clamp(1.0f - dist / cutoff, 0.0f, 1.0f);
}

std::pair<float, float> SoundBank::panGains(const Vec3& origin,
                                            const Vec3& listenerPos,
                                            float listenerYaw) {
    // Horizontal direction only (the height never pans).
    const float dx = origin.x - listenerPos.x;
    const float dz = origin.z - listenerPos.z;
    const float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= 0.0f) {
        return {0.5f, 0.5f}; // AT the listener: centered (no division)
    }
    // The camera's RIGHT vector (horizontal): (cos yaw, -sin yaw) — the
    // Camera::right convention (yaw 0 = facing -Z, so right = +X).
    // lateral = the rightward component of the direction, in [-1, 1]
    // (the distance normalizes it: the pan is directional only).
    const float lateral = std::clamp(
        (dx * std::cos(listenerYaw) - dz * std::sin(listenerYaw)) / dist,
        -1.0f, 1.0f);
    return {0.5f - 0.5f * lateral, 0.5f + 0.5f * lateral};
}

std::vector<std::int16_t> SoundBank::toStereo(
    const std::vector<std::int16_t>& pcm, float gainL, float gainR) {
    std::vector<std::int16_t> out(pcm.size() * 2);
    for (std::size_t i = 0; i < pcm.size(); ++i) {
        const float s = static_cast<float>(pcm[i]);
        out[2 * i] = static_cast<std::int16_t>(
            std::clamp(s * gainL, -32768.0f, 32767.0f));
        out[2 * i + 1] = static_cast<std::int16_t>(
            std::clamp(s * gainR, -32768.0f, 32767.0f));
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
    // CRAFT-3: stereo output — the bank queues interleaved stereo (the
    // 2D path duplicates the mono sample to both channels, the spatial
    // path pans it; the device output format is whatever SDL chose).
    spec.channels = 2;
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
    inSpec.channels = 2;
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

void SoundBank::setListener(Vec3 position, float yaw) {
    m_listenerPos = position;
    m_listenerYaw = yaw;
}

void SoundBank::play(const std::string& name) {
    play(name, std::nullopt);
}

void SoundBank::play(const std::string& name, std::optional<Vec3> origin) {
    // The disabled bank (no audio device) is a FULL no-op — the graceful
    // degradation path the unit tests pin (enabled() stays false, nothing
    // is queued, the game is otherwise unaffected).
    if (!m_enabled) {
        return;
    }
    const auto it = m_samples.find(name);
    if (it == m_samples.end()) {
        return; // unknown name: silent no-op (documented)
    }
    // CRAFT-3: spatialization. The 2D path (no origin) keeps the legacy
    // behavior: the volume-scaled mono sample DUPLICATED to both channels
    // (gainL = gainR = 1 after the volume — perceptually bit-equivalent
    // to the pre-stereo mono stream). With an origin: the linear distance
    // attenuation multiplies the volume gain, and the constant-gain
    // linear pan (against the listener, setListener) splits the channel
    // gains (gL + gR = 1).
    float gain = m_volume;
    float gainL = 1.0f;
    float gainR = 1.0f;
    if (origin.has_value()) {
        gain *= distanceGain(length(*origin - m_listenerPos),
                             kSoundCutoffBlocks);
        const auto [l, r] = panGains(*origin, m_listenerPos, m_listenerYaw);
        gainL = l;
        gainR = r;
    }
    const auto scaled = applyGain(it->second.pcm, gain);
    const auto stereo = toStereo(scaled, gainL, gainR);
    SDL_PutAudioStreamData(m_stream, stereo.data(), stereo.size() * 2);
}

} // namespace mc
