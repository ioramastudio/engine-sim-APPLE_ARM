#include "../include/engine_sim_unity.h"

#include "../include/engine.h"
#include "../include/impulse_response.h"
#include "../include/simulator.h"
#include "../include/transmission.h"
#include "../include/vehicle.h"
#include "../scripting/include/compiler.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

struct Context {
    Simulator simulator;
    Engine *engine = nullptr;
    Vehicle *vehicle = nullptr;
    Transmission *transmission = nullptr;
    std::string error;
    std::mutex stateMutex;
    bool loaded = false;
    bool simulatorInitialized = false;
};

void unload(Context *context) {
    if (context->simulatorInitialized) context->simulator.releaseSimulation();
    if (context->engine != nullptr) {
        context->engine->destroy();
        delete context->engine;
    }
    delete context->vehicle;
    delete context->transmission;
    context->engine = nullptr;
    context->vehicle = nullptr;
    context->transmission = nullptr;
    context->loaded = false;
    context->simulatorInitialized = false;
}

uint16_t readU16(std::istream &stream) {
    uint8_t bytes[2]{};
    stream.read(reinterpret_cast<char *>(bytes), 2);
    return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
}

uint32_t readU32(std::istream &stream) {
    uint8_t bytes[4]{};
    stream.read(reinterpret_cast<char *>(bytes), 4);
    return static_cast<uint32_t>(bytes[0] | (bytes[1] << 8) |
        (bytes[2] << 16) | (bytes[3] << 24));
}

bool loadPcm16Wave(const fs::path &path, std::vector<int16_t> *samples, std::string *error) {
    std::ifstream stream(path, std::ios::binary);
    char id[4]{};
    stream.read(id, 4);
    (void)readU32(stream);
    char wave[4]{};
    stream.read(wave, 4);
    if (!stream || std::memcmp(id, "RIFF", 4) != 0 || std::memcmp(wave, "WAVE", 4) != 0) {
        *error = "Invalid WAV file: " + path.string();
        return false;
    }

    uint16_t format = 0, channels = 0, bits = 0;
    std::vector<uint8_t> data;
    while (stream && (!format || data.empty())) {
        stream.read(id, 4);
        if (!stream) break;
        const uint32_t size = readU32(stream);
        if (std::memcmp(id, "fmt ", 4) == 0) {
            format = readU16(stream);
            channels = readU16(stream);
            (void)readU32(stream);
            (void)readU32(stream);
            (void)readU16(stream);
            bits = readU16(stream);
            if (size > 16) stream.seekg(size - 16, std::ios::cur);
        }
        else if (std::memcmp(id, "data", 4) == 0) {
            data.resize(size);
            stream.read(reinterpret_cast<char *>(data.data()), size);
        }
        else {
            stream.seekg(size, std::ios::cur);
        }
        if (size & 1) stream.seekg(1, std::ios::cur);
    }

    if (format != 1 || channels == 0 || bits != 16 || data.empty()) {
        *error = "Only PCM 16-bit WAV impulse responses are supported: " + path.string();
        return false;
    }
    const auto *input = reinterpret_cast<const int16_t *>(data.data());
    const size_t frameCount = data.size() / (sizeof(int16_t) * channels);
    samples->resize(frameCount);
    for (size_t frame = 0; frame < frameCount; ++frame) (*samples)[frame] = input[frame * channels];
    return true;
}

Context *get(es_engine_handle handle) { return static_cast<Context *>(handle); }
}

extern "C" {

es_engine_handle es_engine_create(void) {
    try { return new Context; }
    catch (...) { return nullptr; }
}

void es_engine_destroy(es_engine_handle handle) {
    Context *context = get(handle);
    if (context == nullptr) return;
    {
        std::lock_guard<std::mutex> lock(context->stateMutex);
        unload(context);
    }
    delete context;
}

int es_engine_load(es_engine_handle handle, const char *scriptPath,
    const char *assetRoot, int audioSampleRate) {
    Context *context = get(handle);
    if (context == nullptr || scriptPath == nullptr || assetRoot == nullptr) return 0;
    std::lock_guard<std::mutex> lock(context->stateMutex);
    unload(context);
    context->error.clear();

    try {
        const fs::path root = fs::absolute(assetRoot);
        es_script::Compiler compiler;
        compiler.initialize();
        compiler.addSearchPath((root / "es").string());
        compiler.addSearchPath((root / "assets").string());
        if (!compiler.compile(fs::absolute(scriptPath).string())) {
            compiler.destroy();
            context->error = "Engine script compilation failed; see error_log.log";
            return 0;
        }
        const es_script::Compiler::Output output = compiler.execute();
        compiler.destroy();
        context->engine = output.engine;
        context->vehicle = output.vehicle;
        context->transmission = output.transmission;
        if (context->engine == nullptr || context->vehicle == nullptr || context->transmission == nullptr) {
            context->error = "Engine script did not produce an engine, vehicle, and transmission";
            unload(context);
            return 0;
        }

        context->engine->calculateDisplacement();
        context->simulator.setFluidSimulationSteps(8);
        context->simulator.setSimulationFrequency(static_cast<int>(context->engine->getSimulationFrequency()));
        context->simulator.setAudioSampleRate(std::max(8000, audioSampleRate));
        Simulator::Parameters simulatorParameters = output.simulatorParameters;
        simulatorParameters.SystemType = Simulator::SystemType::NsvOptimized;
        context->simulator.initialize(simulatorParameters);
        context->simulatorInitialized = true;
        context->simulator.loadSimulation(context->engine, context->vehicle, context->transmission);

        Synthesizer::AudioParameters audio = context->simulator.getSynthesizer()->getAudioParameters();
        audio.InputSampleNoise = static_cast<float>(context->engine->getInitialJitter());
        audio.AirNoise = static_cast<float>(context->engine->getInitialNoise());
        audio.dF_F_mix = static_cast<float>(context->engine->getInitialHighFrequencyGain());
        context->simulator.getSynthesizer()->setAudioParameters(audio);

        for (int i = 0; i < context->engine->getExhaustSystemCount(); ++i) {
            ImpulseResponse *response = context->engine->getExhaustSystem(i)->getImpulseResponse();
            std::vector<int16_t> impulse;
            const std::string responseFilename = response->getFilename();
            fs::path wavePath = root / responseFilename;
            if (!fs::exists(wavePath)) {
                wavePath = root / "assets" / responseFilename;
            }
            if (!fs::exists(wavePath) && responseFilename.rfind("es/", 0) == 0) {
                wavePath = root / "assets" / responseFilename.substr(3);
            }
            if (!fs::exists(wavePath)) {
                wavePath = root / "assets" / "sound-library" / response->getFilename();
            }
            if (!loadPcm16Wave(wavePath, &impulse, &context->error)) {
                unload(context);
                return 0;
            }
            context->simulator.getSynthesizer()->initializeImpulseResponse(
                impulse.data(), static_cast<unsigned int>(impulse.size()),
                static_cast<float>(response->getVolume()), i);
        }
        context->loaded = true;
        context->simulator.startAudioRenderingThread();
        return 1;
    }
    catch (const std::exception &exception) {
        context->error = exception.what();
        unload(context);
        return 0;
    }
    catch (...) {
        context->error = "Unknown native error while loading engine";
        unload(context);
        return 0;
    }
}

const char *es_engine_get_last_error(es_engine_handle handle) {
    Context *context = get(handle);
    return context == nullptr ? "Invalid engine handle" : context->error.c_str();
}

void es_engine_set_throttle(es_engine_handle handle, float throttle) {
    Context *c = get(handle);
    if (c && c->loaded) {
        // Drive the configured linkage/governor. Engine::setThrottle() is an
        // internal plate value and is overwritten by Engine::update().
        c->engine->setSpeedControl(std::clamp(throttle, 0.0f, 1.0f));
    }
}
void es_engine_set_ignition(es_engine_handle handle, int enabled) {
    Context *c = get(handle); if (c && c->loaded) c->engine->getIgnitionModule()->m_enabled = enabled != 0;
}
void es_engine_set_starter(es_engine_handle handle, int enabled) {
    Context *c = get(handle); if (c && c->loaded) c->simulator.m_starterMotor.m_enabled = enabled != 0;
}
void es_engine_set_clutch(es_engine_handle handle, float pressure) {
    Context *c = get(handle); if (c && c->loaded) c->transmission->setClutchPressure(std::clamp(pressure, 0.0f, 1.0f));
}
void es_engine_set_gear(es_engine_handle handle, int gear) {
    Context *c = get(handle); if (c && c->loaded) c->transmission->changeGear(gear);
}
void es_engine_set_rpm(es_engine_handle handle, float rpm) {
    Context *c = get(handle);
    if (c && c->loaded) c->simulator.setExternalRpm(std::max(0.0f, rpm));
}

int es_engine_update(es_engine_handle handle, double dt) {
    Context *c = get(handle);
    if (c == nullptr || !c->loaded) return 0;
    dt = std::clamp(dt, 1.0 / 1000.0, 1.0 / 15.0);
    c->simulator.startFrame(dt);
    while (c->simulator.simulateStep()) {}
    c->simulator.endFrame();
    return 1;
}

int es_engine_render_audio(es_engine_handle handle, float *output, int frames, int channels) {
    Context *c = get(handle);
    if (output == nullptr || frames <= 0 || channels <= 0) return 0;
    std::fill(output, output + static_cast<size_t>(frames) * channels, 0.0f);
    if (c == nullptr || !c->loaded) return 0;
    thread_local std::vector<int16_t> mono;
    mono.resize(frames);
    const int read = c->simulator.readAudioOutput(frames, mono.data());
    for (int frame = 0; frame < frames; ++frame) {
        const float sample = mono[frame] / 32768.0f;
        for (int channel = 0; channel < channels; ++channel) output[frame * channels + channel] = sample;
    }
    return read;
}

float es_engine_get_rpm(es_engine_handle handle) {
    Context *c = get(handle); return c && c->loaded ? static_cast<float>(c->engine->getRpm()) : 0.0f;
}
float es_engine_get_torque(es_engine_handle handle) {
    Context *c = get(handle); return c && c->loaded ? static_cast<float>(c->simulator.getFilteredDynoTorque()) : 0.0f;
}
float es_engine_get_power(es_engine_handle handle) {
    Context *c = get(handle); return c && c->loaded ? static_cast<float>(c->simulator.getDynoPower()) : 0.0f;
}

}
