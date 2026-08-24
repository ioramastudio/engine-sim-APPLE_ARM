#include "../include/convolution_filter.h"

#include <assert.h>
#include <string.h>

#if defined(__APPLE__)
#include <Accelerate/Accelerate.h>
#endif

ConvolutionFilter::ConvolutionFilter() {
    m_shiftRegister = nullptr;
    m_impulseResponse = nullptr;

    m_shiftOffset = 0;
    m_sampleCount = 0;
}

ConvolutionFilter::~ConvolutionFilter() {
    assert(m_shiftRegister == nullptr);
    assert(m_impulseResponse == nullptr);
}

void ConvolutionFilter::initialize(int samples) {
    m_sampleCount = samples;
    m_shiftOffset = 0;
    m_shiftRegister = new float[samples];
    m_impulseResponse = new float[samples];

    memset(m_shiftRegister, 0, sizeof(float) * samples);
    memset(m_impulseResponse, 0, sizeof(float) * samples);
}

void ConvolutionFilter::destroy() {
    delete[] m_shiftRegister;
    delete[] m_impulseResponse;

    m_shiftRegister = nullptr;
    m_impulseResponse = nullptr;
}

float ConvolutionFilter::f(float sample) {
    // A channel without an impulse response is a dry passthrough. This also
    // keeps partially configured synthesizers safe during startup.
    if (m_sampleCount == 0) return sample;

    m_shiftRegister[m_shiftOffset] = sample;

    float result = 0;
#if defined(__APPLE__)
    const int firstCount = m_sampleCount - m_shiftOffset;
    vDSP_dotpr(
        m_impulseResponse, 1,
        m_shiftRegister + m_shiftOffset, 1,
        &result, static_cast<vDSP_Length>(firstCount));
    if (m_shiftOffset != 0) {
        float wrappedResult = 0;
        vDSP_dotpr(
            m_impulseResponse + firstCount, 1,
            m_shiftRegister, 1,
            &wrappedResult, static_cast<vDSP_Length>(m_shiftOffset));
        result += wrappedResult;
    }
#else
    for (int i = 0; i < m_sampleCount - m_shiftOffset; ++i) {
        result += m_impulseResponse[i] * m_shiftRegister[i + m_shiftOffset];
    }

    for (int i = m_sampleCount - m_shiftOffset; i < m_sampleCount; ++i) {
        result += m_impulseResponse[i] * m_shiftRegister[i - (m_sampleCount - m_shiftOffset)];
    }
#endif

    if (--m_shiftOffset < 0) m_shiftOffset = m_sampleCount - 1;

    return result;
}
