#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <cmath>
#if defined(__linux__)
#include <execinfo.h>
#include <csignal>
#include <unistd.h>
#endif
#include <cstdlib>
#include <iostream>
#include <new>
static thread_local bool realtimeGuard = false;
static thread_local size_t allocations = 0, deallocations = 0;
void *operator new(std::size_t n)
{
    if (realtimeGuard)
        ++allocations;
    if (auto *p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept
{
    if (realtimeGuard && p)
        ++deallocations;
    std::free(p);
}
void operator delete[](void *p) noexcept { ::operator delete(p); }
void operator delete(void *p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void *p, std::size_t) noexcept { ::operator delete(p); }
void *operator new(std::size_t n, std::align_val_t alignment)
{
    if (realtimeGuard)
        ++allocations;
    void *p = nullptr;
    if (posix_memalign(&p, (size_t)alignment, n ? n : 1))
        throw std::bad_alloc();
    return p;
}
void *operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void *p, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete[](void *p, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete(void *p, std::size_t, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete[](void *p, std::size_t, std::align_val_t) noexcept { ::operator delete(p); }
int failures = 0, checks = 0;
void check(bool ok, const char *message)
{
    ++checks;
    std::cout << (ok ? "PASS " : "FAIL ") << message << std::endl;
    if (!ok)
        ++failures;
}
void pump() { juce::MessageManager::getInstance()->runDispatchLoopUntil(10); }
bool wait(GumboPotAudioProcessor &p)
{
    for (int i = 0; i < 1500; ++i)
    {
        pump();
        if (!p.loading.load())
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(75);
            return true;
        }
    }
    return false;
}
void set(GumboPotAudioProcessor &p, const juce::String &name, float value)
{
    auto *a = p.apvts.getParameter(name);
    a->setValueNotifyingHost(a->convertTo0to1(value));
}
GumboSample tone(int frames = 44100)
{
    GumboSample s;
    s.audio.setSize(1, frames);
    for (int i = 0; i < frames; ++i)
        s.audio.setSample(0, i, (float)(.3 * std::sin(2 * juce::MathConstants<double>::pi * 100 * i / 44100.0)));
    return s;
}
int crossings(const juce::AudioBuffer<float> &b)
{
    int count = 0;
    for (int n = 5000; n < b.getNumSamples(); ++n)
        if (b.getSample(0, n - 1) < 0 && b.getSample(0, n) >= 0)
            ++count;
    return count;
}
struct Head : juce::AudioPlayHead
{
    PositionInfo info;
    juce::Optional<PositionInfo> getPosition() const override { return info; }
};
int main(int argc, char **argv)
{
#if defined(__linux__)
    std::signal(SIGSEGV, +[](int){void* frames[40];int count=backtrace(frames,40);backtrace_symbols_fd(frames,count,2);_exit(139);});
#endif
    juce::ScopedJuceInitialiser_GUI init;
    const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getNonexistentChildFile("GumboNativeTests", "", false);
    temp.createDirectory();
    struct Cleanup
    {
        juce::File f;
        ~Cleanup() { f.deleteRecursively(); }
    } cleanup{temp};
    {
        auto sample = tone();
        LayerPlayer normal, slow;
        normal.prepare(44100, 512);
        slow.prepare(44100, 512);
        normal.setSample(&sample);
        slow.setSample(&sample);
        normal.setGain(1);
        slow.setGain(1);
        normal.setTempo(120);
        slow.setTempo(60);
        juce::AudioBuffer<float> a(2, 88200), b(2, 88200);
        a.clear();
        b.clear();
        normal.process(a, 0, 88200);
        slow.process(b, 0, 88200);
        check(a.getMagnitude(0, 0, 88200) > .1f && a.getMagnitude(1, 0, 88200) > .1f,
              "mono sample produces centred stereo output");
        check(std::abs(crossings(a) - 2 * crossings(b)) < 3,
              "host BPM ratio is correct (60 BPM halves sample frequency)");
        LayerPlayer left;
        left.prepare(44100, 512);
        left.setSample(&sample);
        left.setGain(1);
        left.setPan(-1);
        a.clear();
        left.process(a, 0, 88200);
        check(a.getMagnitude(1, 4000, 84200) < .0001f && a.getMagnitude(0, 4000, 84200) > .1f,
              "hard-left pan silences the opposite channel");
        auto tiny = tone(4);
        tiny.sampleRate = 96000;
        tiny.bpm = 20;
        LayerPlayer loop;
        loop.prepare(8000, 512);
        loop.setSample(&tiny);
        loop.setGain(1);
        loop.setTempo(999);
        loop.setFilter(1);
        b.clear();
        loop.process(b, 0, 88200);
        bool finite = true;
        for (int n = 0; n < 88200; ++n)
            finite = finite && std::isfinite(b.getSample(0, n));
        check(finite, "very short loops, large tempo ratios and low-rate filters stay finite");
        auto stereo = tone();
        stereo.audio.setSize(2, 44100, true, true);
        stereo.audio.clear(1, 0, 44100);
        LayerPlayer preserve;
        preserve.prepare(44100, 512);
        preserve.setSample(&stereo);
        preserve.setGain(1);
        a.clear();
        preserve.process(a, 0, 88200);
        check(a.getMagnitude(1, 4000, 84200) < .0001f,
              "stereo source channels are preserved rather than collapsed to mono");
    }
    GumboPotAudioProcessor p;
    p.prepareToPlay(44100, 512);
    const juce::StringArray ids{"master",       "gumbo",        "weirdness",    "seed",         "reroll",
                                "layer1gain",   "layer1pan",    "layer1filter", "layer2gain",   "layer2pan",
                                "layer2filter", "layer3gain",   "layer3pan",    "layer3filter", "layer4gain",
                                "layer4pan",    "layer4filter", "layer5gain",   "layer5pan",    "layer5filter"};
    bool frozen = p.getParameters().size() == ids.size();
    for (int i = 0; i < ids.size(); ++i)
        frozen = frozen && p.apvts.getParameter(ids[i]) == p.getParameters()[i];
    check(frozen, "all 20 parameter IDs and ordering are preserved");
    p.loadDemo();
    check(wait(p), "worker completes synthetic demo generation");
    check(p.getUiState().layers[4].active, "all five demo layers load");
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    p.processBlock(audio, midi);
    check(audio.getMagnitude(0, 0, 512) == 0, "plugin is silent with stopped transport and no audition/MIDI");
    p.audition.store(true);
    for (int i = 0; i < 5; ++i)
        p.processBlock(audio, midi);
    check(audio.getMagnitude(0, 0, 512) > .001, "audition renders actual native audio");
    const juce::uint8 sysex[] = {0xf0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0xf7};
    midi.addEvent(sysex, (int)sizeof(sysex), 0);
    allocations = deallocations = 0;
    realtimeGuard = true;
    for (int i = 0; i < 300; ++i)
        p.processBlock(audio, midi);
    realtimeGuard = false;
    check(allocations == 0 && deallocations == 0,
          "300 callbacks including SysEx perform no C++ new/delete on the calling audio thread");
    p.setLocked(0, true);
    const auto before = p.apvts.getRawParameterValue("layer1gain")->load();
    p.doReroll();
    wait(p);
    check(p.apvts.getRawParameterValue("layer1gain")->load() == before, "locked bowl gain survives reroll");
    const auto mix = temp.getChildFile("mix.wav");
    p.exportAudio(mix, false, 4);
    check(wait(p) && mix.existsAsFile(), "native full-mix WAV export completes");
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(fm.createReaderFor(mix));
    check(reader && reader->numChannels == 2 && reader->lengthInSamples == 352800 && reader->sampleRate == 44100,
          "export has the expected stereo format and four-bar duration");
    const auto stems = temp.getChildFile("stems.zip");
    p.exportAudio(stems, true, 4);
    wait(p);
    juce::ZipFile zip(stems);
    check(zip.getNumEntries() == 5, "native stem export contains exactly five WAV files");
    set(p, "layer1gain", .123f);
    juce::MemoryBlock state;
    p.getStateInformation(state);
    GumboPotAudioProcessor restored;
    restored.prepareToPlay(44100, 512);
    restored.setStateInformation(state.getData(), (int)state.getSize());
    wait(restored);
    check(restored.getUiState().layers[0].active && restored.getUiState().layers[4].active,
          "demo sample state recalls without external files");
    check(std::abs(restored.apvts.getRawParameterValue("layer1gain")->load() - .123f) < .0001f && restored.isLocked(0),
          "state recall preserves saved gains and locks");
    const auto source = temp.getChildFile("tone_120.wav");
    {
        juce::WavAudioFormat format;
        auto stream = source.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 44100, 1, 16, {}, 0));
        auto s = tone();
        writer->writeFromAudioSampleBuffer(s.audio, 0, s.audio.getNumSamples());
    }
    const auto stack = temp.getChildFile("stack.json");
    stack.replaceWithText("{\"layers\":[{\"path\":\"tone_120.wav\",\"bpm\":120,\"gain\":0.6}]}");
    p.setLocked(0, false);
    p.loadStack(stack);
    wait(p);
    pump();
    const auto loaded = p.getUiState();
    check(loaded.layers[0].active && !loaded.layers[1].active,
          "relative stack paths load and shorter stacks clear old slots");
    check(std::abs(p.apvts.getRawParameterValue("layer1gain")->load() - .6f) < .0001f,
          "explicit stack loading applies JSON gain on the message thread");
    stack.replaceWithText("{\"layers\":[{\"path\":\"missing.wav\"}]}");
    p.loadStack(stack);
    wait(p);
    check(p.getUiState().layers[0].path == loaded.layers[0].path && p.getUiState().message.contains("Missing audio"),
          "failed loading leaves the previous bank intact and reports the missing file");
    // Publish/retire while callbacks run, exercising the exact ownership handoff used in hosts.
    for (int j = 0; j < 20; ++j)
    {
        p.importAudio({source.getFullPathName()}, 0);
        for (int n = 0; n < 20; ++n)
        {
            realtimeGuard = true;
            p.processBlock(audio, midi);
            realtimeGuard = false;
        }
        pump();
    }
    wait(p);
    check(allocations == 0 && deallocations == 0,
          "sample replacement does not allocate or destroy C++ objects in the audio callback");
    Head head;
    head.info.setBpm(90);
    head.info.setIsPlaying(true);
    head.info.setPpqPosition(0);
    p.audition.store(false);
    p.setPlayHead(&head);
    p.processBlock(audio, midi);
    check(p.hostPlaying.load() && p.hostTempo.load() == 90, "host transport and tempo are followed");
    p.setPlayHead(nullptr);
    if (argc > 1)
    {
        for (auto* parameter : restored.getParameters())
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
        for (int i = 0; i < 5; ++i) { restored.setLocked(i, false); restored.setSolo(i, false); }
        restored.loadDemo();
        wait(restored);
        std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
        editor->setSize(1440, 900);
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        juce::FileOutputStream output{juce::File(argv[1])};
        juce::PNGImageFormat png;
        check(png.writeImageToStream(image, output), "native JUCE interface PNG rendered from the actual editor");
    }
    std::cout << checks << " checks, " << failures << " failures" << std::endl;
    return failures ? 1 : 0;
}
