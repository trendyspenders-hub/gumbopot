#include "LayerPlayer.h"
#include <cmath>
void LayerPlayer::prepare(double sampleRate, int block)
{
    rate = sampleRate > 0 ? sampleRate : 44100.0;
    for (auto &f : filters)
    {
        f.prepare({rate, (juce::uint32)juce::jmax(1, block), 1});
        f.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        f.setResonance(0.7071f);
        f.setCutoffFrequency((float)juce::jmin(18000.0, rate * 0.45));
    }
    gain.reset(rate, 0.015);
    pan.reset(rate, 0.015);
    cutoff.reset(rate, 0.015);
    gain.setCurrentAndTargetValue(0.0f);
    pan.setCurrentAndTargetValue(0.0f);
    cutoff.setCurrentAndTargetValue((float)juce::jmin(18000.0, rate * 0.45));
    position = 0;
}
void LayerPlayer::setSample(const GumboSample *next) noexcept
{
    sample = next;
    position = 0;
    for (auto &f : filters)
        f.reset();
}
void LayerPlayer::setTempo(double bpm) noexcept { tempo = std::isfinite(bpm) ? juce::jlimit(20.0, 999.0, bpm) : 120.0; }
void LayerPlayer::setGain(float v) noexcept { gain.setTargetValue(juce::jlimit(0.0f, 1.25f, v)); }
void LayerPlayer::setPan(float v) noexcept { pan.setTargetValue(juce::jlimit(-1.0f, 1.0f, v)); }
void LayerPlayer::setFilter(float v) noexcept
{
    cutoff.setTargetValue(juce::jmin((float)rate * 0.45f, 180.0f * std::pow(100.0f, juce::jlimit(0.0f, 1.0f, v))));
}
void LayerPlayer::seekBeats(double ppq) noexcept
{
    if (sample == nullptr || !std::isfinite(ppq) || sample->audio.getNumSamples() < 2)
        return;
    const auto length = (double)sample->audio.getNumSamples();
    position = std::fmod(ppq * 60.0 / sample->bpm * sample->sampleRate, length);
    if (position < 0)
        position += length;
}
void LayerPlayer::process(juce::AudioBuffer<float> &out, int start, int count) noexcept
{
    if (sample == nullptr || sample->audio.getNumSamples() < 2)
        return;
    const int length = sample->audio.getNumSamples(), sourceChannels = sample->audio.getNumChannels();
    const double ratio = sample->sampleRate / rate * tempo / sample->bpm;
    float previousCutoff = -1.0f;
    for (int n = 0; n < count; ++n)
    {
        const int a = (int)position, b = (a + 1) % length;
        const float fraction = (float)(position - a), g = gain.getNextValue(), p = pan.getNextValue(),
                    hz = cutoff.getNextValue();
        if (std::abs(hz - previousCutoff) > 0.001f)
        {
            for (auto &filter : filters)
                filter.setCutoffFrequency(hz);
            previousCutoff = hz;
        }
        // Balance preserves stereo. Centre remains unity; panning attenuates the opposite side.
        const float angle = (p + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
        const float left = juce::jmin(1.0f, std::cos(angle) * juce::MathConstants<float>::sqrt2);
        const float right = juce::jmin(1.0f, std::sin(angle) * juce::MathConstants<float>::sqrt2);
        for (int ch = 0; ch < out.getNumChannels(); ++ch)
        {
            float v;
            if (out.getNumChannels() == 1 && sourceChannels > 1)
                v = ((sample->audio.getSample(0, a) + sample->audio.getSample(1, a)) * (1.0f - fraction) +
                     (sample->audio.getSample(0, b) + sample->audio.getSample(1, b)) * fraction) *
                    0.5f;
            else
            {
                const int src = juce::jmin(ch, sourceChannels - 1);
                v = sample->audio.getSample(src, a) * (1.0f - fraction) + sample->audio.getSample(src, b) * fraction;
            }
            auto &f = filters[(size_t)juce::jmin(ch, 1)];
            out.addSample(ch, start + n,
                          f.processSample(0, v) * g *
                              (out.getNumChannels() == 1 ? 1.0f
                               : ch == 0                 ? left
                                                         : right));
        }
        position = std::fmod(position + ratio, (double)length);
    }
    for (auto &f : filters)
        f.snapToZero();
}
