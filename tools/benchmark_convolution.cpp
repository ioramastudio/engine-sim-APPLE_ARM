#include "convolution_filter.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>

int main(int argc, char **argv) {
    const int taps = argc > 1 ? std::atoi(argv[1]) : 10000;
    const int samples = argc > 2 ? std::atoi(argv[2]) : 48000;

    ConvolutionFilter filter;
    filter.initialize(taps);
    for (int i = 0; i < taps; ++i) {
        filter.getImpulseResponse()[i] = std::exp(-i / 1500.0f) / taps;
    }

    float checksum = 0.0f;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < samples; ++i) {
        checksum += filter.f(static_cast<float>((i * 17) & 255) - 127.0f);
    }
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();

    std::cout << "taps=" << taps << " samples=" << samples
              << " seconds=" << elapsed
              << " samples_per_second=" << samples / elapsed
              << " checksum=" << checksum << '\n';
    filter.destroy();
}
