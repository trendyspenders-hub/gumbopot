#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>

juce::AudioProcessorValueTreeState::ParameterLayout GumboPotAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    const auto unity = juce::NormalisableRange<float>(0.0f, 1.0f);
    const auto bipolar = juce::NormalisableRange<float>(-1.0f, 1.0f);

    params.push_back(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("master", 1), "Master", unity, 0.8f));
    params.push_back(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("gumbo", 1), "Gumbo Amount", unity, 0.5f));
    params.push_back(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("weirdness", 1), "Weirdness", unity, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID("seed", 1), "Seed", 0, 9999, 42));

    // Automation trigger: any 0->1 crossing pulls a new cook.
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("reroll", 1), "Re-Roll", false));

    for (int i = 1; i <= 5; ++i)
    {
        const auto n = juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("layer" + n + "gain", 1), "Layer " + n + " Gain",
            juce::NormalisableRange<float>(0.0f, 1.25f), 0.9f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("layer" + n + "pan", 1),
                                                                     "Layer " + n + " Pan", bipolar, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("layer" + n + "filter", 1),
                                                                     "Layer " + n + " Filter", unity, 1.0f));
    }

    return {params.begin(), params.end()};
}

GumboPotAudioProcessor::GumboPotAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      Thread("Gumbo sample loader"), apvts(*this, nullptr, "GUMBO", createParameterLayout())
{
    for (int i = 0; i < 5; ++i)
    {
        const auto prefix = "layer" + juce::String(i + 1);
        parameters[(size_t)i] = {apvts.getRawParameterValue(prefix + "gain"),
                                 apvts.getRawParameterValue(prefix + "pan"),
                                 apvts.getRawParameterValue(prefix + "filter")};
    }
    masterParam = apvts.getRawParameterValue("master");
    rerollParam = apvts.getRawParameterValue("reroll");
    setLatencySamples(0);
    startThread();
    startTimerHz(20);
}
GumboPotAudioProcessor::~GumboPotAudioProcessor()
{
    stopTimer();
    signalThreadShouldExit();
    notify();
    stopThread(-1);
}
bool GumboPotAudioProcessor::isBusesLayoutSupported(const BusesLayout &layout) const
{
    const auto out = layout.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}
void GumboPotAudioProcessor::prepareToPlay(double rate, int block)
{
    for (auto &player : players)
        player.prepare(rate, block);
    masterGain.reset(rate, 0.02);
    masterGain.setCurrentAndTargetValue(masterParam->load());
    midiNotes = 0;
    transitionGain = 0;
}
const GumboPotAudioProcessor::Bank *GumboPotAudioProcessor::protectPublished() noexcept
{
    const Bank *candidate;
    do
    {
        candidate = published.load();
        hazardCandidate.store(candidate);
    } while (candidate != published.load());
    return candidate;
}
void GumboPotAudioProcessor::bind(const Bank *bank) noexcept
{
    // Protect new ownership before releasing old ownership; no samples are deleted here.
    hazardCurrent.store(bank);
    activeBank = bank;
    for (int i = 0; i < 5; ++i)
        players[(size_t)i].setSample(bank ? bank->samples[(size_t)i].get() : nullptr);
}
void GumboPotAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const auto *candidate = protectPublished();
    if (activeBank == nullptr)
        bind(candidate);
    bool running = false;
    double bpm = 120.0;
    std::optional<double> beat;
    if (auto *head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            running = pos->getIsPlaying();
            if (auto value = pos->getBpm())
                if (std::isfinite(*value) && *value > 0)
                    bpm = *value;
            if (auto value = pos->getPpqPosition())
                beat = *value;
        }
    for (const auto metadata : midi)
    {
        // Inspect bytes directly: constructing a MidiMessage for SysEx could allocate.
        if (metadata.numBytes < 3)
            continue;
        const int type = metadata.data[0] & 0xf0;
        if (type == 0x90 && metadata.data[2] > 0)
            midiNotes = juce::jmin(2048, midiNotes + 1);
        if (type == 0x80 || (type == 0x90 && metadata.data[2] == 0))
            midiNotes = juce::jmax(0, midiNotes - 1);
        if (type == 0xb0 && (metadata.data[1] == 123 || metadata.data[1] == 120))
            midiNotes = 0;
    }
    midi.clear();
    hostTempo.store(bpm);
    hostPlaying.store(running);
    const float rr = rerollParam->load();
    if (rr > 0.5f && lastReroll <= 0.5f)
        rerollRequested.store(true);
    lastReroll = rr;
    bool anySolo = false;
    for (const auto &value : solo)
        anySolo = anySolo || value.load();
    const bool play = running || audition.load() || midiNotes > 0;
    if (play)
    {
        for (int i = 0; i < 5; ++i)
        {
            auto &p = players[(size_t)i];
            const auto &values = parameters[(size_t)i];
            p.setTempo(bpm);
            p.setGain(anySolo && !solo[(size_t)i].load() ? 0.0f : values.gain->load());
            p.setPan(values.pan->load());
            p.setFilter(values.filter->load());
            if (running && beat)
                p.seekBeats(*beat);
            p.process(buffer, 0, buffer.getNumSamples());
        }
    }
    masterGain.setTargetValue(masterParam->load());
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        // Fixed mixer headroom is part of both real-time and export paths.
        const float gain = masterGain.getNextValue() * 0.3f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, n, buffer.getSample(ch, n) * gain);
    }
    const bool swap = candidate != activeBank;
    buffer.applyGainRamp(0, buffer.getNumSamples(), transitionGain, swap ? 0.0f : 1.0f);
    transitionGain = swap ? 0.0f : 1.0f;
    if (swap)
        bind(candidate);
    hazardCandidate.store(nullptr);
    float peak = 0;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    outputPeak.store(peak);
}
GumboPotAudioProcessor::UiState GumboPotAudioProcessor::getUiState() const
{
    const juce::ScopedLock lock(uiMutex);
    return ui;
}
void GumboPotAudioProcessor::submit(Job job)
{
    if (job.type == Job::render && loading.load())
    {
        const juce::ScopedLock lock(uiMutex);
        ui.message = "Finish loading before exporting.";
        return;
    }
    job.id = requested.fetch_add(1) + 1;
    loading.store(true);
    {
        const juce::ScopedLock lock(jobMutex);
        nextJob = std::move(job);
    }
    notify();
}
bool GumboPotAudioProcessor::loadStack(const juce::File &file)
{
    if (!file.existsAsFile())
        return false;
    Job job;
    job.type = Job::stack;
    job.file = file;
    submit(std::move(job));
    return true;
}
void GumboPotAudioProcessor::importAudio(const juce::StringArray &paths, int target)
{
    if (paths.isEmpty())
        return;
    Job job;
    job.paths = paths;
    job.target = target;
    job.type = Job::files;
    submit(std::move(job));
}
void GumboPotAudioProcessor::loadDemo()
{
    Job job;
    job.type = Job::demo;
    submit(std::move(job));
}
void GumboPotAudioProcessor::exportAudio(const juce::File &file, bool stems, int bars)
{
    Job job;
    job.type = Job::render;
    job.file = file;
    job.stems = stems;
    job.bars = juce::jlimit(1, 16, bars);
    submit(std::move(job));
}
void GumboPotAudioProcessor::setParamValue(const juce::String &id, float value)
{
    if (auto *p = apvts.getParameter(id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(value));
        p->endChangeGesture();
    }
}
void GumboPotAudioProcessor::doReroll()
{
    juce::Random random((juce::int64)apvts.getRawParameterValue("seed")->load() * 7919 + (juce::int64)++rerollCount);
    const float weird = apvts.getRawParameterValue("weirdness")->load(),
                gumbo = apvts.getRawParameterValue("gumbo")->load();
    const auto state = getUiState();
    for (int i = 0; i < 5; ++i)
        if (state.layers[(size_t)i].active && !locked[(size_t)i].load())
        {
            const auto prefix = "layer" + juce::String(i + 1);
            setParamValue(prefix + "gain", 0.5f + random.nextFloat() * 0.6f);
            setParamValue(prefix + "pan", (random.nextFloat() * 2 - 1) * (0.3f + 0.7f * weird));
            setParamValue(prefix + "filter", juce::jlimit(0.0f, 1.0f, gumbo * (0.5f + random.nextFloat() * 0.6f)));
        }
    Job job;
    job.type = Job::stir;
    submit(std::move(job));
}
void GumboPotAudioProcessor::timerCallback()
{
    if (rerollRequested.exchange(false))
        doReroll();
    std::array<float, 5> gains{};
    uint64_t revision;
    {
        const juce::ScopedLock lock(uiMutex);
        revision = uiGainRevision;
        gains = uiPendingGains;
    }
    if (revision > appliedGains)
    {
        appliedGains = revision;
        for (int i = 0; i < 5; ++i)
            setParamValue("layer" + juce::String(i + 1) + "gain", gains[(size_t)i]);
    }
}
void GumboPotAudioProcessor::reclaim()
{
    // seq_cst hazard protocol: a reader publishes and rechecks before dereferencing.
    // A bank may be freed here only when neither publication nor either reader hazard refers to it.
    ownedBanks.erase(std::remove_if(ownedBanks.begin(), ownedBanks.end(),
                                    [this](const auto &bank)
                                    {
                                        return bank.get() != published.load() && bank.get() != hazardCurrent.load() &&
                                               bank.get() != hazardCandidate.load();
                                    }),
                     ownedBanks.end());
}
void GumboPotAudioProcessor::run()
{
    while (!threadShouldExit())
    {
        std::optional<Job> job;
        {
            const juce::ScopedLock lock(jobMutex);
            job.swap(nextJob);
        }
        if (!job)
        {
            reclaim();
            wait(100);
            continue;
        }
        try
        {
            if (job->type == Job::render)
                renderFile(*job);
            else
                perform(*job);
        }
        catch (const std::exception &error)
        {
            if (job->id == requested.load())
            {
                const juce::ScopedLock lock(uiMutex);
                ui.message = juce::String(error.what());
            }
        }
        if (job->id == requested.load())
            loading.store(false);
        reclaim();
    }
}
double GumboPotAudioProcessor::bpmFromName(const juce::String &name)
{
    const auto tokens =
        juce::StringArray::fromTokens(name.replaceCharacter('_', ' ').replaceCharacter('-', ' '), " ", "");
    for (auto token : tokens)
    {
        token = token.upToFirstOccurrenceOf("bpm", false, true);
        if (token.containsOnly("0123456789") && token.isNotEmpty())
        {
            const auto n = token.getDoubleValue();
            if (n >= 40 && n <= 240)
                return n;
        }
    }
    return 120;
}
void GumboPotAudioProcessor::measure(GumboSample &sample)
{
    const int length = sample.audio.getNumSamples(), channels = sample.audio.getNumChannels();
    double square = 0;
    for (int i = 0; i < 128; ++i)
    {
        const int a = (int)((int64_t)i * length / 128), b = juce::jmax(a + 1, (int)((int64_t)(i + 1) * length / 128));
        float peak = 0;
        for (int n = a; n < juce::jmin(b, length); ++n)
            for (int ch = 0; ch < channels; ++ch)
            {
                const float x = sample.audio.getSample(ch, n);
                peak = juce::jmax(peak, std::abs(x));
                square += x * x;
            }
        sample.waveform[(size_t)i] = peak;
    }
    sample.rms = (float)std::sqrt(square / juce::jmax(1, length * channels));
}
std::unique_ptr<GumboSample> GumboPotAudioProcessor::decode(const juce::File &file, double bpm)
{
    if (!file.existsAsFile())
        throw std::runtime_error(("Missing audio: " + file.getFileName()).toStdString());
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(file));
    if (!reader || reader->lengthInSamples < 4 || reader->sampleRate <= 0 ||
        reader->lengthInSamples > reader->sampleRate * 120.0 || reader->lengthInSamples > 12000000 ||
        reader->numChannels < 1 || reader->numChannels > 2)
        throw std::runtime_error(
            "Use mono/stereo WAV, AIFF or FLAC loops up to 120 seconds (12 million frames maximum).");
    auto sample = std::make_unique<GumboSample>();
    sample->audio.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
    if (!reader->read(&sample->audio, 0, sample->audio.getNumSamples(), 0, true, true))
        throw std::runtime_error("Audio decode failed.");
    for (int ch = 0; ch < sample->audio.getNumChannels(); ++ch)
        for (int n = 0; n < sample->audio.getNumSamples(); ++n)
            if (!std::isfinite(sample->audio.getSample(ch, n)))
                sample->audio.setSample(ch, n, 0);
    sample->sampleRate = reader->sampleRate;
    sample->bpm = std::isfinite(bpm) && bpm >= 20 && bpm <= 999 ? bpm : 120;
    sample->name = file.getFileNameWithoutExtension();
    sample->path = file.getFullPathName();
    measure(*sample);
    return sample;
}
void GumboPotAudioProcessor::publish(std::unique_ptr<Bank> bank)
{
    if (bank->id != requested.load())
        return;
    {
        const juce::ScopedLock lock(uiMutex);
        ui.revision = bank->id;
        ui.title = bank->title;
        ui.message = "Ready. Follow your DAW transport or use Audition.";
        for (int i = 0; i < 5; ++i)
        {
            auto &info = ui.layers[(size_t)i];
            info = {};
            if (auto *s = bank->samples[(size_t)i].get())
            {
                info.name = s->name;
                info.path = s->path;
                info.bpm = s->bpm;
                info.duration = s->audio.getNumSamples() / s->sampleRate;
                info.waveform = s->waveform;
                info.active = true;
            }
        }
        ui.pantryNames.clear();
        ui.pantryPaths = pantry;
        for (const auto &path : pantry)
            ui.pantryNames.add(juce::File(path).getFileNameWithoutExtension());
        if (bank->applyGains)
        {
            uiGainRevision = bank->id;
            uiPendingGains = bank->gains;
        }
    }
    const auto *ptr = bank.get();
    ownedBanks.push_back(std::move(bank));
    published.store(ptr);
}
void GumboPotAudioProcessor::perform(const Job &job)
{
    auto bank = std::make_unique<Bank>();
    bank->id = job.id;
    bank->title = "Imported mix";
    const auto *previous = published.load();
    std::array<juce::String, 5> paths;
    std::array<double, 5> bpms{120, 120, 120, 120, 120};
    if (previous)
        for (int i = 0; i < 5; ++i)
            if (auto *s = previous->samples[(size_t)i].get())
            {
                paths[(size_t)i] = s->path;
                bpms[(size_t)i] = s->bpm;
            }
    if (job.type == Job::demo)
    {
        for (int i = 0; i < 5; ++i)
        {
            bank->samples[(size_t)i] = makeDemoSample(i);
        }
        bank->title = "House mix";
        publish(std::move(bank));
        return;
    }
    if (job.type == Job::stack)
    {
        const auto json = juce::JSON::parse(job.file);
        const auto array = json["layers"];
        if (!array.isArray() || array.getArray()->isEmpty() || array.getArray()->size() > 5)
            throw std::runtime_error("Stack must contain one to five layers.");
        paths = {};
        int i = 0;
        bank->applyGains = job.applyGains;
        for (const auto &item : *array.getArray())
        {
            const auto p = item["path"].toString();
            if (p.isEmpty())
                throw std::runtime_error("Stack contains an empty sample path.");
            paths[(size_t)i] =
                juce::File::isAbsolutePath(p) ? p : job.file.getParentDirectory().getChildFile(p).getFullPathName();
            bpms[(size_t)i] = item.hasProperty("bpm") ? (double)item["bpm"] : 120;
            bank->gains[(size_t)i] = item.hasProperty("gain") ? juce::jlimit(0.0f, 1.25f, (float)(double)item["gain"])
                                                              : parameters[(size_t)i].gain->load();
            ++i;
        }
        for (; i < 5; ++i)
            bank->gains[(size_t)i] = parameters[(size_t)i].gain->load();
        bank->title = job.file.getFileNameWithoutExtension();
    }
    else if (job.type == Job::restore)
    {
        for (int i = 0; i < 5; ++i)
        {
            paths[(size_t)i] = job.paths[i];
            bpms[(size_t)i] = job.bpms[(size_t)i];
        }
        bank->title = "Recalled mix";
    }
    else if (job.type == Job::files)
    {
        for (const auto &path : job.paths)
            if (pantry.size() < 512)
                pantry.addIfNotAlreadyThere(path);
        if (job.target >= 0 && job.target < 5)
        {
            if (locked[(size_t)job.target].load())
                throw std::runtime_error("Unlock this bowl before replacing its sample.");
            paths[(size_t)job.target] = job.paths[0];
            bpms[(size_t)job.target] = bpmFromName(juce::File(job.paths[0]).getFileNameWithoutExtension());
        }
        else
        {
            int index = 0;
            for (int i = 0; i < 5 && index < job.paths.size(); ++i)
                if (!locked[(size_t)i].load())
                {
                    paths[(size_t)i] = job.paths[index++];
                    bpms[(size_t)i] = bpmFromName(juce::File(paths[(size_t)i]).getFileNameWithoutExtension());
                }
        }
    }
    else if (job.type == Job::stir)
    {
        juce::Random random((juce::int64)apvts.getRawParameterValue("seed")->load() * 7919 + (juce::int64)job.id);
        for (int i = 0; i < 5; ++i)
            if (!locked[(size_t)i].load())
            {
                juce::StringArray candidates;
                for (const auto &path : pantry)
                    if (path != paths[(size_t)i] && std::find(paths.begin(), paths.end(), path) == paths.end())
                        candidates.add(path);
                if (!candidates.isEmpty())
                {
                    paths[(size_t)i] = candidates[random.nextInt(candidates.size())];
                    bpms[(size_t)i] = bpmFromName(juce::File(paths[(size_t)i]).getFileNameWithoutExtension());
                }
            }
        bank->title = previous ? previous->title : "Imported mix";
    }
    // Demo slots are regenerated when another slot changes, without disk access.
    for (int i = 0; i < 5; ++i)
    {
        if (paths[(size_t)i].startsWith("demo:"))
        {
            const int demoIndex = paths[(size_t)i].fromFirstOccurrenceOf(":", false, false).getIntValue();
            bank->samples[(size_t)i] = makeDemoSample(juce::jlimit(0, 4, demoIndex));
        }
        else if (paths[(size_t)i].isNotEmpty())
        {
            bank->samples[(size_t)i] = decode(juce::File(paths[(size_t)i]), bpms[(size_t)i]);
            if (pantry.size() < 512)
                pantry.addIfNotAlreadyThere(paths[(size_t)i]);
        }
    }
    publish(std::move(bank));
}
void GumboPotAudioProcessor::renderFile(const Job &job)
{
    const auto *bank = published.load();
    if (bank == nullptr)
        throw std::runtime_error("Load samples before exporting.");
    const double bpm = hostTempo.load(), rate = 44100;
    const int total = (int)std::ceil(job.bars * 4 * 60.0 / bpm * rate);
    const int block = 512;
    std::array<float, 5> renderGain{}, renderPan{}, renderFilter{};
    std::array<bool, 5> renderSolo{};
    bool anySolo = false;
    for (int i = 0; i < 5; ++i)
    {
        renderGain[(size_t)i] = parameters[(size_t)i].gain->load();
        renderPan[(size_t)i] = parameters[(size_t)i].pan->load();
        renderFilter[(size_t)i] = parameters[(size_t)i].filter->load();
        renderSolo[(size_t)i] = solo[(size_t)i].load();
        anySolo = anySolo || renderSolo[(size_t)i];
    }
    const float renderMaster = masterParam->load() * .3f;
    auto render = [&](const juce::File &file, int stem)
    {
        if (file.existsAsFile() && !file.deleteFile())
            throw std::runtime_error("Could not replace output file.");
        juce::WavAudioFormat format;
        auto stream = file.createOutputStream();
        if (!stream)
            throw std::runtime_error("Cannot write to the selected folder.");
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), rate, 2, 16, {}, 0));
        if (!writer)
            throw std::runtime_error("Cannot create WAV writer.");
        std::array<LayerPlayer, 5> renderers;
        for (int i = 0; i < 5; ++i)
        {
            auto &p = renderers[(size_t)i];
            p.prepare(rate, block);
            p.setSample(bank->samples[(size_t)i].get());
            p.setTempo(bpm);
            p.setGain(renderGain[(size_t)i]);
            p.setPan(renderPan[(size_t)i]);
            p.setFilter(renderFilter[(size_t)i]);
        }
        juce::AudioBuffer<float> buffer(2, block);
        const float output = renderMaster;
        for (int done = 0; done < total; done += block)
        {
            const int n = juce::jmin(block, total - done);
            buffer.clear();
            for (int i = 0; i < 5; ++i)
                if ((stem < 0 && (!anySolo || renderSolo[(size_t)i])) || stem == i)
                    renderers[(size_t)i].process(buffer, 0, n);
            buffer.applyGain(output);
            if (!writer->writeFromAudioSampleBuffer(buffer, 0, n))
                throw std::runtime_error("WAV write failed.");
        }
    };
    if (!job.stems)
        render(job.file, -1);
    else
    {
        const auto temp =
            juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("GumboStems", "", false);
        if (temp.createDirectory().failed())
            throw std::runtime_error("Cannot create temporary stem folder.");
        struct Cleanup
        {
            juce::File folder;
            ~Cleanup() { folder.deleteRecursively(); }
        } cleanup{temp};
        juce::ZipFile::Builder zip;
        const char *names[] = {"1-Andouille-Drums.wav", "2-Shrimp-Bass.wav", "3-Okra-Keys.wav", "4-File-Texture.wav",
                               "5-BayLeaf-Vocals.wav"};
        for (int i = 0; i < 5; ++i)
        {
            auto file = temp.getChildFile(names[i]);
            render(file, i);
            zip.addFile(file, 6, names[i]);
        }
        if (job.file.existsAsFile() && !job.file.deleteFile())
            throw std::runtime_error("Cannot replace stem archive.");
        auto stream = job.file.createOutputStream();
        if (!stream || !zip.writeToStream(*stream, nullptr))
            throw std::runtime_error("Cannot write stem archive.");
    }
    const juce::ScopedLock lock(uiMutex);
    ui.message = "Exported: " + job.file.getFileName() + " (" + juce::String(job.bars) + " bars at " +
                 juce::String(bpm, 1) + " BPM).";
}
void GumboPotAudioProcessor::getStateInformation(juce::MemoryBlock &dest)
{
    auto tree = apvts.copyState();
    const auto state = getUiState();
    tree.removeChild(tree.getChildWithName("SAMPLES"), nullptr);
    juce::ValueTree samples("SAMPLES");
    for (int i = 0; i < 5; ++i)
    {
        juce::ValueTree layer("LAYER");
        layer.setProperty("path", state.layers[(size_t)i].path, nullptr);
        layer.setProperty("bpm", state.layers[(size_t)i].bpm, nullptr);
        layer.setProperty("locked", locked[(size_t)i].load(), nullptr);
        layer.setProperty("solo", solo[(size_t)i].load(), nullptr);
        samples.addChild(layer, -1, nullptr);
    }
    tree.addChild(samples, -1, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, dest);
}
void GumboPotAudioProcessor::setStateInformation(const void *data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml(*xml);
            apvts.replaceState(tree);
            {
                const juce::ScopedLock lock(uiMutex);
                uiGainRevision = 0;
            }
            const auto samples = tree.getChildWithName("SAMPLES");
            Job job;
            job.type = Job::restore;
            job.applyGains = false;
            if (samples.isValid())
                for (int i = 0; i < 5; ++i)
                {
                    const auto layer = samples.getChild(i);
                    job.paths.add(layer.getProperty("path").toString());
                    job.bpms[(size_t)i] = (double)layer.getProperty("bpm", 120);
                    locked[(size_t)i].store((bool)layer.getProperty("locked", false));
                    solo[(size_t)i].store((bool)layer.getProperty("solo", false));
                }
            else
            {
                const auto path = tree.getProperty("stackPath").toString();
                if (path.isEmpty())
                    return;
                job.type = Job::stack;
                job.file = juce::File(path);
            }
            submit(std::move(job));
        }
}
juce::AudioProcessorEditor *GumboPotAudioProcessor::createEditor() { return new GumboPotAudioProcessorEditor(*this); }
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new GumboPotAudioProcessor(); }

std::unique_ptr<GumboSample> GumboPotAudioProcessor::makeDemoSample(int i)
{
    static const char *names[] = {"Cast iron drums", "Deep roux bass", "Sunday keys", "Bayou atmosphere",
                                  "Synthetic call"};
    auto sample = std::make_unique<GumboSample>();
    sample->sampleRate = 44100;
    sample->bpm = 120;
    sample->name = names[i];
    sample->path = "demo:" + juce::String(i);
    sample->audio.setSize(1, 352800);
    juce::Random random(100 + i);
    for (int n = 0; n < 352800; ++n)
    {
        const double t = n / 44100.0, beat = std::fmod(t, .5);
        float x = 0;
        if (i == 0)
            x = (float)(.5 *
                        std::sin(2 * juce::MathConstants<double>::pi * (48 * beat + 3 * (1 - std::exp(-30 * beat)))) *
                        std::exp(-20 * beat)) +
                (random.nextFloat() * 2 - 1) * .04f * (float)std::exp(-80 * std::fmod(t, .25));
        if (i == 1)
            x = (float)(.3 * std::sin(2 * juce::MathConstants<double>::pi * 65.406 * t) * std::exp(-4 * beat));
        if (i == 2)
            x = (float)(.12 *
                        (std::sin(2 * juce::MathConstants<double>::pi * 261.626 * t) +
                         std::sin(2 * juce::MathConstants<double>::pi * 311.127 * t) +
                         std::sin(2 * juce::MathConstants<double>::pi * 391.995 * t)) *
                        std::exp(-3 * std::fmod(t, 2.0)));
        if (i == 3)
            x = (float)(.05 * (std::sin(2 * juce::MathConstants<double>::pi * 523.252 * t) +
                               std::sin(2 * juce::MathConstants<double>::pi * 784 * t)));
        if (i == 4)
            x = (float)(.16 * std::sin(2 * juce::MathConstants<double>::pi * 311.127 * t) *
                        std::sin(2 * juce::MathConstants<double>::pi * 2 * t) * std::exp(-6 * std::fmod(t, 1.0)));
        sample->audio.setSample(0, n, x);
    }
    measure(*sample);
    return sample;
}
