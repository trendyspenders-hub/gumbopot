#pragma once
#include <array>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

// Immutable after publication. Allocation, decoding and destruction belong to the loader.
struct GumboSample
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0, bpm = 120.0;
    juce::String name, path;
    std::array<float, 128> waveform{};
    float rms = 0.0f;
};

class LayerPlayer
{
  public:
    void prepare(double sampleRate, int maxBlockSize);
    void setSample(const GumboSample *sample) noexcept;
    void setTempo(double bpm) noexcept;
    void setGain(float value) noexcept;
    void setPan(float value) noexcept;
    void setFilter(float value) noexcept;
    void seekBeats(double ppq) noexcept;
    void process(juce::AudioBuffer<float> &output, int start, int count) noexcept;

  private:
    const GumboSample *sample = nullptr;
    double rate = 44100.0, tempo = 120.0, position = 0.0;
    juce::SmoothedValue<float> gain, pan, cutoff;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> filters;
};
