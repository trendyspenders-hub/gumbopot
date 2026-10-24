#pragma once
#include "LayerPlayer.h"
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

class GumboPotAudioProcessor : public juce::AudioProcessor, private juce::Thread, private juce::Timer
{
  public:
    static constexpr int numLayers = 5;
    struct LayerInfo
    {
        juce::String name, path;
        double bpm = 120.0, duration = 0;
        std::array<float, 128> waveform{};
        bool active = false;
    };
    struct UiState
    {
        uint64_t revision = 0;
        juce::String title{"Your next recipe starts here"}, message{"Import audio or load the original demo pantry."};
        std::array<LayerInfo, 5> layers;
        juce::StringArray pantryNames, pantryPaths;
    };
    GumboPotAudioProcessor();
    ~GumboPotAudioProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Gumbo Pot"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    bool loadStack(const juce::File &);
    void importAudio(const juce::StringArray &, int target = -1);
    void loadDemo();
    void doReroll();
    void exportAudio(const juce::File &, bool stems, int bars);
    UiState getUiState() const;
    void setLocked(int i, bool b) { locked[(size_t)i].store(b); }
    bool isLocked(int i) const { return locked[(size_t)i].load(); }
    void setSolo(int i, bool b) { solo[(size_t)i].store(b); }
    bool isSolo(int i) const { return solo[(size_t)i].load(); }
    std::atomic<bool> audition{false}, loading{false};
    std::atomic<float> outputPeak{0};
    std::atomic<double> hostTempo{120.0};
    std::atomic<bool> hostPlaying{false};

  private:
    struct Bank
    {
        std::array<std::unique_ptr<GumboSample>, 5> samples;
        juce::String title;
        uint64_t id = 0;
        std::array<float, 5> gains{};
        bool applyGains = false;
    };
    struct Job
    {
        enum Type
        {
            stack,
            files,
            demo,
            restore,
            stir,
            render
        } type = files;
        juce::StringArray paths;
        std::array<double, 5> bpms{120, 120, 120, 120, 120};
        juce::File file;
        int target = -1, bars = 4;
        bool stems = false, applyGains = true;
        uint64_t id = 0;
    };
    void run() override;
    void timerCallback() override;
    void submit(Job);
    void perform(const Job &);
    void renderFile(const Job &);
    void publish(std::unique_ptr<Bank>);
    void reclaim();
    void setParamValue(const juce::String &, float);
    static std::unique_ptr<GumboSample> decode(const juce::File &, double);
    static void measure(GumboSample &);
    static std::unique_ptr<GumboSample> makeDemoSample(int);
    static double bpmFromName(const juce::String &);
    void bind(const Bank *) noexcept;
    const Bank *protectPublished() noexcept;
    mutable juce::CriticalSection uiMutex;
    UiState ui;
    uint64_t uiGainRevision = 0;
    std::array<float, 5> uiPendingGains{};
    juce::CriticalSection jobMutex;
    std::optional<Job> nextJob;
    std::atomic<uint64_t> requested{0};
    std::atomic<const Bank *> published{nullptr}, hazardCurrent{nullptr}, hazardCandidate{nullptr};
    const Bank *activeBank = nullptr;              // audio-thread only
    std::vector<std::unique_ptr<Bank>> ownedBanks; // loader only; hazards protect readers
    juce::StringArray pantry;                      // loader only
    std::array<LayerPlayer, 5> players;
    std::array<std::atomic<bool>, 5> locked{}, solo{};
    struct ParameterSet
    {
        std::atomic<float> *gain = nullptr;
        std::atomic<float> *pan = nullptr;
        std::atomic<float> *filter = nullptr;
    };
    std::array<ParameterSet, 5> parameters;
    std::atomic<float> *masterParam = nullptr;
    std::atomic<float> *rerollParam = nullptr;
    juce::SmoothedValue<float> masterGain;
    std::atomic<bool> rerollRequested{false};
    float lastReroll = 0, transitionGain = 0;
    int midiNotes = 0;
    uint64_t appliedGains = 0, rerollCount = 0; // message thread
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GumboPotAudioProcessor)
};
