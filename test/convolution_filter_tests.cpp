#include <gtest/gtest.h>

#include "../include/convolution_filter.h"

#include <vector>

TEST(ConvolutionFilterTests, MatchesScalarCircularConvolution) {
    constexpr int tapCount = 17;
    ConvolutionFilter filter;
    filter.initialize(tapCount);

    std::vector<float> history(tapCount, 0.0f);
    std::vector<float> impulse(tapCount);
    for (int i = 0; i < tapCount; ++i) {
        impulse[i] = (i + 1) * 0.003f;
        filter.getImpulseResponse()[i] = impulse[i];
    }

    for (int sampleIndex = 0; sampleIndex < 100; ++sampleIndex) {
        const float sample = static_cast<float>((sampleIndex * 13) % 29 - 14);
        history.insert(history.begin(), sample);
        history.pop_back();

        float expected = 0.0f;
        for (int i = 0; i < tapCount; ++i) expected += impulse[i] * history[i];
        EXPECT_NEAR(filter.f(sample), expected, 1e-4f);
    }

    filter.destroy();
}

TEST(ConvolutionFilterTests, MissingImpulseResponseIsDryPassthrough) {
    ConvolutionFilter filter;
    EXPECT_FLOAT_EQ(filter.f(12.5f), 12.5f);
}
