#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>
namespace
{
const juce::Colour paper(0xffead5b2), muted(0xffaeb9b8), gold(0xffe7b361), green(0xff60c5a2);
const std::array<juce::Colour, 5> accents{juce::Colour(0xffec7156), juce::Colour(0xffe4b858), juce::Colour(0xff78bd83),
                                          juce::Colour(0xff5cbadd), juce::Colour(0xffd6bd97)};
const std::array<juce::String, 5> names{"ANDOUILLE", "SHRIMP", "OKRA", "FILE", "BAY LEAF"};
const std::array<juce::String, 5> roles{"DRUMS / PERCUSSION", "BASS", "MELODY / KEYS", "TEXTURES / FX",
                                        "VOCALS / CHOPS"};
} // namespace
KitchenLookAndFeel::KitchenLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, paper);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::textColourOffId, paper);
    setColour(juce::TextButton::textColourOnId, juce::Colour(0xff121715));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff151d1d));
    setColour(juce::ComboBox::textColourId, paper);
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff465149));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff11191a));
    setColour(juce::PopupMenu::textColourId, paper);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff172222));
    setColour(juce::TooltipWindow::textColourId, paper);
}
void KitchenLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height, float position,
                                          float start, float end, juce::Slider &slider)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(7);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * .5f, cx = bounds.getCentreX(),
                cy = bounds.getCentreY();
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const float angle = start + position * (end - start);
    juce::Path rail;
    rail.addCentredArc(cx, cy, radius, radius, 0, start, end, true);
    g.setColour(juce::Colour(0xff3d4641));
    g.strokePath(rail, juce::PathStrokeType(2.4f));
    juce::Path arc;
    arc.addCentredArc(cx, cy, radius, radius, 0, start, angle, true);
    g.setColour(accent);
    g.strokePath(arc, juce::PathStrokeType(2.4f));
    auto knob = juce::Rectangle<float>(cx - radius + 5, cy - radius + 5, 2 * radius - 10, 2 * radius - 10);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff49504a), cx, knob.getY(), juce::Colour(0xff101616), cx,
                                           knob.getBottom(), false));
    g.fillEllipse(knob);
    g.setColour(juce::Colour(0xff6c7365));
    g.drawEllipse(knob, 1);
    g.setColour(juce::Colour(0xff070b0b));
    g.drawEllipse(knob.reduced(3), 1);
    const auto a = juce::Point<float>(cx, cy).getPointOnCircumference(radius - 9, angle),
               b = juce::Point<float>(cx, cy).getPointOnCircumference(radius * .36f, angle);
    g.setColour(paper);
    g.drawLine({a, b}, 2.2f);
}
void KitchenLookAndFeel::drawButtonBackground(juce::Graphics &g, juce::Button &button, const juce::Colour &, bool over,
                                              bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(.6f);
    auto fill = button.findColour(juce::TextButton::buttonColourId);
    if (button.getToggleState())
        fill = button.findColour(juce::TextButton::buttonOnColourId);
    if (over)
        fill = fill.brighter(.12f);
    if (down)
        fill = fill.darker(.12f);
    if (!button.isEnabled())
        fill = fill.withMultipliedAlpha(.4f);
    g.setGradientFill(
        juce::ColourGradient(fill.brighter(.07f), 0, bounds.getY(), fill.darker(.12f), 0, bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 5);
    g.setColour(button.getToggleState() ? gold : juce::Colour(0xff65685a).withAlpha(button.isEnabled() ? .8f : .35f));
    g.drawRoundedRectangle(bounds, 5, 1);
}
juce::Font KitchenLookAndFeel::getTextButtonFont(juce::TextButton &, int height)
{
    return juce::Font(juce::FontOptions(juce::jlimit(10.0f, 15.0f, height * .32f), juce::Font::bold));
}
GumboPotAudioProcessorEditor::GumboPotAudioProcessorEditor(GumboPotAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&theme);
    backdrop = juce::ImageCache::getFromMemory(BinaryData::kitchenbackdrop_jpg, BinaryData::kitchenbackdrop_jpgSize);
    ui = processor.getUiState();
    addSlider(master, "master", gold);
    addSlider(gumbo, "gumbo", gold);
    addSlider(weirdness, "weirdness", green);
    seed.setSliderStyle(juce::Slider::IncDecButtons);
    seed.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 85, 28);
    seed.setTooltip("Seed. The same seed and action sequence reproduce the same mix choices.");
    addAndMakeVisible(seed);
    attachments.push_back(std::make_unique<Attachment>(processor.apvts, "seed", seed));
    for (int i = 0; i < 5; ++i)
    {
        const auto index = (size_t)i;
        const auto prefix = "layer" + juce::String(i + 1);
        addSlider(gains[index], prefix + "gain", accents[index]);
        addSlider(pans[index], prefix + "pan", accents[index]);
        addSlider(filters[index], prefix + "filter", accents[index]);
        gains[index].setTooltip("Scoop: layer gain. Double-click to reset.");
        pans[index].setTooltip("Place: stereo balance. Double-click for centre.");
        filters[index].setTooltip("Strain: low-pass cutoff, 180 Hz to 18 kHz (limited by sample rate).");
        gains[index].setDoubleClickReturnValue(true, .9);
        pans[index].setDoubleClickReturnValue(true, 0);
        filters[index].setDoubleClickReturnValue(true, 1);
        for (auto *b : {&locks[index], &solos[index], &loads[index]})
        {
            addAndMakeVisible(*b);
            b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff142021));
            b->setColour(juce::TextButton::buttonOnColourId, accents[index]);
        }
        locks[index].setButtonText("LOCK");
        locks[index].setClickingTogglesState(true);
        locks[index].onClick = [this, i] { processor.setLocked(i, locks[(size_t)i].getToggleState()); };
        solos[index].setButtonText("SOLO");
        solos[index].setClickingTogglesState(true);
        solos[index].onClick = [this, i] { processor.setSolo(i, solos[(size_t)i].getToggleState()); };
        loads[index].setButtonText("LOAD SOUND");
        loads[index].onClick = [this, i] { chooseAudio(i); };
    }
    for (auto *b : {&importButton, &demoButton, &recipeButton, &saveButton, &stirButton, &playButton, &mixButton,
                    &stemButton, &spliceButton})
    {
        addAndMakeVisible(*b);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff182122));
        b->setColour(juce::TextButton::buttonOnColourId, gold);
    }
    stirButton.setColour(juce::TextButton::buttonColourId, gold);
    stirButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff211b11));
    importButton.onClick = [this] { chooseAudio(); };
    demoButton.onClick = [this] { processor.loadDemo(); };
    recipeButton.onClick = [this] { chooseRecipe(); };
    saveButton.onClick = [this] { saveRecipe(); };
    stirButton.onClick = [this] { processor.doReroll(); };
    playButton.setClickingTogglesState(true);
    playButton.onClick = [this] { processor.audition.store(playButton.getToggleState()); };
    mixButton.onClick = [this] { chooseExport(false); };
    stemButton.onClick = [this] { chooseExport(true); };
    spliceButton.setTooltip("Start the optional included Splice companion, then open it in your browser. Account "
                            "authentication remains in the companion.");
    spliceButton.onClick = [] { juce::URL("http://127.0.0.1:5173").launchInDefaultBrowser(); };
    bars.addItem("4 bars", 4);
    bars.addItem("8 bars", 8);
    bars.addItem("16 bars", 16);
    bars.setSelectedId(4);
    addAndMakeVisible(bars);
    pantryList.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    pantryList.setRowHeight(42);
    addAndMakeVisible(pantryList);
    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(1100, 688, 1920, 1200);
    getConstrainer()->setFixedAspectRatio(1.6);
    setSize(1440, 900);
    startTimerHz(15);
}
GumboPotAudioProcessorEditor::~GumboPotAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}
void GumboPotAudioProcessorEditor::addSlider(juce::Slider &s, const juce::String &id, juce::Colour colour)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    s.setColour(juce::Slider::rotarySliderFillColourId, colour);
    s.setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(s);
    attachments.push_back(std::make_unique<Attachment>(processor.apvts, id, s));
    // Attachments supply parameter formatting first; override only the editor display/entry units.
    // Automation IDs, ranges, defaults and state values remain unchanged.
    const bool isGain = id == "master" || id.endsWith("gain");
    const bool isPan = id.endsWith("pan"), isFilter = id.endsWith("filter");
    s.textFromValueFunction = [isGain, isPan, isFilter](double value)
    {
        if (isGain)
            return value <= 0.0001 ? juce::String("-inf dB")
                                  : juce::String(juce::Decibels::gainToDecibels(value), 1) + " dB";
        if (isPan)
            return std::abs(value) < 0.005 ? juce::String("C")
                                         : juce::String((int)std::round(std::abs(value) * 100)) + (value < 0 ? "% L" : "% R");
        if (isFilter)
        {
            const double hz = 180.0 * std::pow(100.0, value);
            return hz >= 1000 ? juce::String(hz / 1000.0, 1) + " kHz" : juce::String((int)std::round(hz)) + " Hz";
        }
        return juce::String((int)std::round(value * 100)) + "%";
    };
    s.valueFromTextFunction = [isGain, isPan, isFilter](const juce::String &input)
    {
        if (isGain)
            return input.containsIgnoreCase("inf") ? 0.0 : juce::Decibels::decibelsToGain(input.getDoubleValue());
        if (isPan)
        {
            if (input.trim().equalsIgnoreCase("C")) return 0.0;
            const double magnitude = input.getDoubleValue() / 100.0;
            return input.containsIgnoreCase("L") ? -std::abs(magnitude)
                 : input.containsIgnoreCase("R") ? std::abs(magnitude) : magnitude;
        }
        if (isFilter)
        {
            const double hz = input.getDoubleValue() * (input.containsIgnoreCase("k") ? 1000.0 : 1.0);
            return std::log(juce::jlimit(180.0, 18000.0, hz) / 180.0) / std::log(100.0);
        }
        return input.getDoubleValue() / 100.0;
    };
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff3b4942));
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff10191a));
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    s.updateText();
}
void GumboPotAudioProcessorEditor::text(juce::Graphics &g, const juce::String &value, juce::Rectangle<float> bounds,
                                        float size, juce::Colour colour, int justification, bool bold)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    g.drawFittedText(value, bounds.toNearestInt(), juce::Justification(justification), 2);
}
void GumboPotAudioProcessorEditor::drawPanel(juce::Graphics &g, juce::Rectangle<float> bounds, float alpha)
{
    g.setColour(juce::Colour(0xff0c1416).withAlpha(alpha));
    g.fillRoundedRectangle(bounds, 9);
    g.setColour(juce::Colour(0xff727462).withAlpha(.5f));
    g.drawRoundedRectangle(bounds, 9, 1);
}
void GumboPotAudioProcessorEditor::paint(juce::Graphics &raw)
{
    juce::Graphics::ScopedSaveState state(raw);
    raw.addTransform(juce::AffineTransform::scale(scale));
    auto &g = raw;
    g.fillAll(juce::Colour(0xff0d1415));
    if (backdrop.isValid())
        g.drawImage(backdrop, {0, 0, 1440, 900}, juce::RectanglePlacement::fillDestination);
    g.setColour(juce::Colour(0xff050c0e).withAlpha(.36f));
    g.fillAll();
    g.setColour(juce::Colour(0xff0b1113).withAlpha(.95f));
    g.fillRect(0, 0, 1440, 90);
    g.setColour(gold.withAlpha(.5f));
    g.drawHorizontalLine(90, 0, 1440);
    g.setFont(juce::FontOptions("Georgia", 38, juce::Font::bold));
    g.setColour(paper);
    g.drawText("GUMBO POT", 28, 15, 320, 46, juce::Justification::centredLeft);
    text(g, "A N A L O G   B E A T   K I T C H E N", {31, 58, 310, 16}, 10, gold);
    text(g, "FIVE INGREDIENTS.\nENDLESS POSSIBILITIES.", {363, 24, 250, 43}, 13, muted);
    text(g, "NATIVE INSTRUMENT   /   VST3 + AU", {1080, 32, 330, 24}, 11, paper, juce::Justification::centredRight,
         true);
    drawPanel(g, {20, 110, 218, 680});
    drawPanel(g, {1204, 110, 216, 300});
    drawPanel(g, {1204, 428, 216, 232});
    drawPanel(g, {1204, 678, 216, 112});
    text(g, "THE PANTRY", {38, 129, 180, 24}, 15, paper, juce::Justification::centredLeft, true);
    text(g, "YOUR SOUNDS. YOUR RECIPE.", {38, 154, 180, 18}, 9, gold);
    text(g, ui.pantryNames.isEmpty() && ui.layers[0].active ? "5 HOUSE INGREDIENTS" : juce::String(ui.pantryNames.size()) + " LOCAL SAMPLES", {38, 259, 180, 20}, 10, muted);
    text(g, "Double-click a pantry sound to\nfill the first unlocked bowl.", {38, 641, 178, 38}, 11, muted);
    text(g, "WAV / AIFF / FLAC\nBPM from filename; default 120.", {38, 695, 180, 38}, 10, muted);
    text(g, "WHAT'S COOKIN'?", {270, 119, 650, 44}, 30, paper, juce::Justification::centredLeft, true);
    text(g, "Layer your leftovers. Lock a favourite. Stir something new.", {272, 165, 690, 26}, 14, paper);
    text(g, ui.title, {272, 201, 690, 24}, 12, gold);
    g.setColour(gold.withAlpha(.5f));
    g.drawLine(266, 246, 1177, 246, 1);
    for (int i = 0; i < 5; ++i)
    {
        const auto index = (size_t)i;
        const float x = 264.0f + i * 184;
        auto bounds = juce::Rectangle<float>(x, 274, 172, 420);
        drawPanel(g, bounds, .89f);
        g.setColour(accents[index]);
        g.fillRoundedRectangle(x + 10, 286, 152, 56, 4);
        text(g, names[index], {x + 12, 292, 148, 23}, i == 0 ? 16.0f : 18.0f, juce::Colour(0xff101c1b),
             juce::Justification::centred, true);
        text(g, roles[index], {x + 12, 319, 148, 14}, 8.5f, juce::Colour(0xff142321), juce::Justification::centred);
        const auto &info = ui.layers[index];
        g.setColour(accents[index].withAlpha(.85f));
        for (int n = 0; n < 128; ++n)
        {
            const float h = info.active ? juce::jlimit(1.0f, 48.0f, info.waveform[(size_t)n] * 78) : 1.0f;
            const float xx = x + 15 + n * 142 / 128.0f;
            g.drawLine(xx, 383 - h * .5f, xx, 383 + h * .5f, 1);
        }
        text(g, info.active ? info.name : "ADD AN INGREDIENT", {x + 15, 420, 142, 34}, 12, info.active ? paper : muted,
             juce::Justification::centred, true);
        text(g,
             info.active ? juce::String(info.bpm, 0) + " BPM  /  " + juce::String(info.duration, 1) + " s"
                         : "DROP AUDIO HERE",
             {x + 14, 455, 144, 20}, 10, muted, juce::Justification::centred);
        text(g, "SCOOP", {x + 25, 481, 122, 16}, 10, muted, juce::Justification::centred);
        text(g, "PLACE", {x + 10, 585, 75, 15}, 9, muted, juce::Justification::centred);
        text(g, "STRAIN", {x + 87, 585, 75, 15}, 9, muted, juce::Justification::centred);
    }
    text(g, "LOCK WHAT YOU LOVE. STIR THE REST.", {358, 705, 720, 20}, 10, paper, juce::Justification::centred);
    text(g, processor.loading.load() ? "PREPARING YOUR INGREDIENTS..." : ui.message, {264, 781, 910, 22}, 11, paper,
         juce::Justification::centred);
    drawPanel(g, {250, 808, 932, 80}, .96f);
    text(g, "MASTER", {278, 815, 90, 14}, 9, muted, juce::Justification::centred);
    text(g, "GUMBO", {406, 815, 90, 14}, 9, muted, juce::Justification::centred);
    text(g, "CAYENNE", {534, 815, 90, 14}, 9, muted, juce::Justification::centred);
    text(g, "SEED", {669, 821, 150, 14}, 9, muted, juce::Justification::centred);
    text(g, "ON THE STOVE", {1222, 128, 180, 24}, 14, paper, juce::Justification::centredLeft, true);
    int active = 0;
    for (const auto &info : ui.layers)
        if (info.active)
            ++active;
    text(g, juce::String(active) + " / 5", {1222, 166, 180, 58}, 42, paper, juce::Justification::centred, true);
    text(g, "INGREDIENTS LOADED", {1222, 225, 180, 18}, 10, gold, juce::Justification::centred);
    text(g, juce::String(processor.hostTempo.load(), 1) + " BPM", {1222, 258, 180, 30}, 23, paper,
         juce::Justification::centred, true);
    text(g,
         processor.hostPlaying.load() ? "DAW TRANSPORT RUNNING"
         : processor.audition.load()  ? "AUDITION RUNNING"
                                      : "READY FOR YOUR DAW",
         {1222, 294, 180, 18}, 9, green, juce::Justification::centred);
    const float peak = processor.outputPeak.load();
    g.setColour(juce::Colour(0xff263c34));
    g.fillRoundedRectangle(1225, 335, 174, 8, 4);
    g.setColour(peak >= 1 ? accents[0] : green);
    g.fillRoundedRectangle(1225, 335, 174 * juce::jlimit(0.0f, 1.0f, peak), 8, 4);
    text(g, peak > 0 ? juce::String(juce::Decibels::gainToDecibels(peak), 1) + " dBFS" : "-inf dBFS",
         {1222, 352, 180, 18}, 11, muted, juce::Justification::centred);
    text(g, "0 SAMPLES LATENCY", {1222, 382, 180, 18}, 9, muted, juce::Justification::centred);
    text(g, "SERVE TO YOUR DAW", {1222, 447, 180, 22}, 12, paper, juce::Justification::centredLeft, true);
    text(g, "44.1 kHz / 16-bit stereo WAV\nTempo sync resamples pitch.", {1222, 600, 180, 38}, 10, muted);
    text(g, "SPLICE", {1222, 693, 180, 20}, 12, paper, juce::Justification::centredLeft, true);
    text(g, "Optional companion login\nRuns separately from the plugin.", {1222, 718, 180, 32}, 10, muted);
    text(g, "GOOD SOUNDS. MAKE GOOD DAYS.", {30, 839, 200, 25}, 10, paper, juce::Justification::centred, true);
}
void GumboPotAudioProcessorEditor::resized()
{
    scale = getWidth() / 1440.0f;
    auto set = [this](juce::Component &c, int x, int y, int w, int h)
    {
        c.setBounds(juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h)
                        .transformedBy(juce::AffineTransform::scale(scale))
                        .toNearestInt());
    };
    set(importButton, 38, 184, 180, 32);
    set(demoButton, 38, 223, 180, 30);
    set(pantryList, 32, 288, 194, 342);
    set(recipeButton, 38, 742, 86, 30);
    set(saveButton, 129, 742, 89, 30);
    for (int i = 0; i < 5; ++i)
    {
        const int x = 264 + i * 184;
        set(gains[(size_t)i], x + 43, 499, 86, 84);
        set(pans[(size_t)i], x + 13, 601, 65, 65);
        set(filters[(size_t)i], x + 94, 601, 65, 65);
        set(locks[(size_t)i], x + 13, 669, 68, 21);
        set(solos[(size_t)i], x + 91, 669, 68, 21);
        set(loads[(size_t)i], x + 14, 394, 144, 24);
    }
    set(stirButton, 478, 733, 484, 44);
    set(master, 277, 831, 98, 55);
    set(gumbo, 405, 831, 98, 55);
    set(weirdness, 533, 831, 98, 55);
    set(seed, 669, 849, 150, 25);
    set(playButton, 975, 832, 186, 36);
    set(bars, 1222, 479, 180, 30);
    set(mixButton, 1222, 521, 180, 32);
    set(stemButton, 1222, 562, 180, 32);
    set(spliceButton, 1219, 752, 186, 25);
}
void GumboPotAudioProcessorEditor::timerCallback()
{
    auto next = processor.getUiState();
    const bool changed = next.revision != ui.revision || next.message != ui.message || next.title != ui.title ||
                         next.pantryPaths != ui.pantryPaths;
    ui = std::move(next);
    pantryList.updateContent();
    playButton.setToggleState(processor.audition.load(), juce::dontSendNotification);
    playButton.setButtonText(processor.audition.load() ? "STOP AUDITION" : "AUDITION");
    for (int i = 0; i < 5; ++i)
    {
        locks[(size_t)i].setToggleState(processor.isLocked(i), juce::dontSendNotification);
        solos[(size_t)i].setToggleState(processor.isSolo(i), juce::dontSendNotification);
        loads[(size_t)i].setEnabled(!processor.loading.load() && !processor.isLocked(i));
    }
    for (auto *b : {&importButton, &demoButton, &recipeButton, &saveButton, &stirButton, &mixButton, &stemButton})
        b->setEnabled(!processor.loading.load());
    if (changed)
        repaint();
    else
        repaint(juce::Rectangle<float>(1204, 110, 216, 300)
                    .transformedBy(juce::AffineTransform::scale(scale))
                    .toNearestInt());
}
void GumboPotAudioProcessorEditor::chooseAudio(int target)
{
    auto chooser =
        std::make_shared<juce::FileChooser>("Choose your ingredients", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles |
                             (target < 0 ? juce::FileBrowserComponent::canSelectMultipleItems : 0),
                         [safe = juce::Component::SafePointer<GumboPotAudioProcessorEditor>(this), chooser,
                          target](const juce::FileChooser &c)
                         {
                             if (!safe)
                                 return;
                             juce::StringArray paths;
                             for (const auto &f : c.getResults())
                                 paths.add(f.getFullPathName());
                             if (!paths.isEmpty())
                                 safe->processor.importAudio(paths, target);
                         });
}
void GumboPotAudioProcessorEditor::chooseRecipe()
{
    auto chooser = std::make_shared<juce::FileChooser>("Open Gumbo recipe or stack", juce::File(), "*.gumbo;*.json");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<GumboPotAudioProcessorEditor>(this), chooser](const juce::FileChooser &c)
        {
            if (!safe || !c.getResult().existsAsFile())
                return;
            const auto f = c.getResult();
            if (f.hasFileExtension("json"))
                safe->processor.loadStack(f);
            else
            {
                juce::MemoryBlock data;
                if (f.loadFileAsData(data))
                    safe->processor.setStateInformation(data.getData(), (int)data.getSize());
            }
        });
}
void GumboPotAudioProcessorEditor::saveRecipe()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save recipe", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("My Cook.gumbo"),
        "*.gumbo");
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<GumboPotAudioProcessorEditor>(this), chooser](const juce::FileChooser &c)
        {
            if (!safe || c.getResult() == juce::File())
                return;
            juce::MemoryBlock data;
            safe->processor.getStateInformation(data);
            c.getResult().withFileExtension("gumbo").replaceWithData(data.getData(), data.getSize());
        });
}
void GumboPotAudioProcessorEditor::chooseExport(bool stems)
{
    auto chooser =
        std::make_shared<juce::FileChooser>(stems ? "Serve five stems" : "Serve full mix",
                                            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                .getChildFile(stems ? "Gumbo Stems.zip" : "Gumbo Mix.wav"),
                                            stems ? "*.zip" : "*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                             juce::FileBrowserComponent::warnAboutOverwriting,
                         [safe = juce::Component::SafePointer<GumboPotAudioProcessorEditor>(this), chooser,
                          stems](const juce::FileChooser &c)
                         {
                             if (safe && c.getResult() != juce::File())
                                 safe->processor.exportAudio(c.getResult().withFileExtension(stems ? "zip" : "wav"),
                                                             stems, safe->bars.getSelectedId());
                         });
}
bool GumboPotAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray &files) { return !files.isEmpty(); }
void GumboPotAudioProcessorEditor::filesDropped(const juce::StringArray &files, int x, int y)
{
    if (files.size() == 1 && juce::File(files[0]).hasFileExtension("json"))
    {
        processor.loadStack(juce::File(files[0]));
        return;
    }
    const int target =
        (y / scale >= 274 && y / scale <= 694 && x / scale >= 264 && x / scale < 1184 && files.size() == 1)
            ? juce::jlimit(0, 4, (int)((x / scale - 264) / 184))
            : -1;
    processor.importAudio(files, target);
}
bool GumboPotAudioProcessorEditor::keyPressed(const juce::KeyPress &key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        processor.audition.store(!processor.audition.load());
        return true;
    }
    return false;
}
int GumboPotAudioProcessorEditor::getNumRows()
{
    return ui.pantryNames.isEmpty() && ui.layers[0].active ? 5 : ui.pantryNames.size();
}
void GumboPotAudioProcessorEditor::paintListBoxItem(int row, juce::Graphics &g, int width, int height, bool selected)
{
    const bool house = ui.pantryNames.isEmpty() && ui.layers[0].active;
    if (row < 0 || row >= (house ? 5 : ui.pantryNames.size()))
        return;
    if (selected)
    {
        g.setColour(gold.withAlpha(.15f));
        g.fillRect(0, 0, width, height);
    }
    g.setColour(juce::Colour(0xff46544b).withAlpha(.6f));
    g.drawHorizontalLine(height - 1, 8, (float)width - 8);
    g.setColour(paper);
    g.setFont(juce::FontOptions(11.0f * scale));
    const auto name = house ? ui.layers[(size_t)row].name : ui.pantryNames[row];
    g.drawFittedText(name, 10, 5, width - 20, house ? 19 : height - 10, juce::Justification::centredLeft, 2);
    if (house)
    {
        g.setColour(muted); g.setFont(juce::FontOptions(9.0f * scale));
        g.drawText("HOUSE DEMO / 120 BPM", 10, 25, width - 20, 12, juce::Justification::centredLeft);
    }
}
void GumboPotAudioProcessorEditor::listBoxItemDoubleClicked(int row, const juce::MouseEvent &)
{
    if (row >= ui.pantryPaths.size())
        return;
    for (int i = 0; i < 5; ++i)
        if (!processor.isLocked(i))
        {
            processor.importAudio({ui.pantryPaths[row]}, i);
            break;
        }
}
