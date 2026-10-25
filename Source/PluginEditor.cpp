#include "PluginEditor.h"
#include <cmath>
namespace
{
const juce::Colour paper(0xffeee3ca), muted(0xffada699), gold(0xffed9c54), green(0xffa8bd83);
const std::array<juce::Colour, 5> accents{juce::Colour(0xffe77b55), juce::Colour(0xffd7b667), juce::Colour(0xff9eae74),
                                          juce::Colour(0xff7ca6a0), juce::Colour(0xffc39b81)};
const std::array<juce::String, 5> names{"ANDOUILLE", "SHRIMP", "OKRA", "FILÉ", "BAY LEAF"};
const std::array<juce::String, 5> roles{"DRUMS / PERCUSSION", "BASS", "MELODY / KEYS", "TEXTURES / FX",
                                        "VOCALS / CHOPS"};
} // namespace
KitchenLookAndFeel::KitchenLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, paper);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff30392f));
    setColour(juce::TextButton::buttonOnColourId, gold);
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
    const auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(6);
    const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * .5f;
    const auto c = bounds.getCentre();
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const float angle = start + position * (end - start);
    for (int n = 0; n <= 10; ++n)
    {
        const float a = start + n * (end - start) / 10.0f;
        g.setColour(n <= position * 10 ? accent : juce::Colour(0xff615b50));
        g.drawLine({c.getPointOnCircumference(r, a), c.getPointOnCircumference(r - 3, a)}, 1.4f);
    }
    auto body = juce::Rectangle<float>(c.x-r+7, c.y-r+7, 2*r-14, 2*r-14);
    g.setColour(juce::Colours::black.withAlpha(.45f));
    g.fillEllipse(body.translated(0, 3));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff55554d), c.x, body.getY(),
                                          juce::Colour(0xff181b19), c.x, body.getBottom(), false));
    g.fillEllipse(body);
    g.setColour(juce::Colour(0xff818071));
    g.drawEllipse(body, 1);
    for (int n = 0; n < 24; ++n)
    {
        const float a = n * juce::MathConstants<float>::twoPi / 24;
        g.setColour(juce::Colours::black.withAlpha(.45f));
        g.drawLine({c.getPointOnCircumference(r-8, a), c.getPointOnCircumference(r-11, a)}, 1);
    }
    juce::Path pointer;
    pointer.addRoundedRectangle(-3, -r+10, 6, juce::jmax(5.0f, r*.65f), 2);
    g.setColour(paper);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(c.x, c.y));
    g.setColour(accent.withAlpha(slider.isMouseOverOrDragging() ? .9f : .4f));
    g.fillEllipse(c.x-2, c.y-2, 4, 4);
}
void KitchenLookAndFeel::drawButtonBackground(juce::Graphics &g, juce::Button &button, const juce::Colour &, bool over,
                                              bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced(1);
    auto fill = button.findColour(juce::TextButton::buttonColourId);
    if (button.getToggleState()) fill = button.findColour(juce::TextButton::buttonOnColourId);
    if (over) fill = fill.brighter(.12f);
    if (down) fill = fill.darker(.2f);
    if (!button.isEnabled()) fill = fill.withMultipliedAlpha(.4f);
    g.setColour(juce::Colour(0xff080b0a));
    g.fillRoundedRectangle(b.translated(0, 2), 2);
    g.setColour(fill);
    g.fillRoundedRectangle(b, 2);
    g.setColour(button.getToggleState() ? paper.withAlpha(.65f) : juce::Colour(0xff777365));
    g.drawRoundedRectangle(b, 2, .8f);
    g.setColour(juce::Colours::white.withAlpha(down ? .02f : .12f));
    g.drawLine(b.getX()+2, b.getY()+1, b.getRight()-2, b.getY()+1);
    if (button.getToggleState())
    {
        g.setColour(juce::Colour(0xff292b24));
        g.fillEllipse(b.getX()+6, b.getCentreY()-1.5f, 3, 3);
    }
}
juce::Font KitchenLookAndFeel::getTextButtonFont(juce::TextButton &, int height)
{
    return juce::Font(juce::FontOptions(juce::jlimit(10.0f, 15.0f, height * .32f), juce::Font::bold));
}
GumboPotAudioProcessorEditor::GumboPotAudioProcessorEditor(GumboPotAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&theme);
    // Fixed, code-drawn enamel grain. No photographic or generated artwork.
    surface = juce::Image(juce::Image::RGB, 1440, 900, true);
    juce::Graphics enamel(surface);
    enamel.fillAll(juce::Colour(0xff292e29));
    juce::Random grain(0x47554d42);
    for (int n = 0; n < 42000; ++n)
    {
        enamel.setColour((n % 2 ? juce::Colours::white : juce::Colours::black).withAlpha(.025f));
        enamel.fillRect(grain.nextInt(1440), grain.nextInt(900), 1, 1);
    }
    lastFrameMs = juce::Time::getMillisecondCounterHiRes();
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
        gains[index].setTooltip("Level. Double-click to reset.");
        pans[index].setTooltip("Pan. Double-click for centre.");
        filters[index].setTooltip("Low-pass filter: 180 Hz to 18 kHz.");
        gains[index].setDoubleClickReturnValue(true, .9);
        pans[index].setDoubleClickReturnValue(true, 0);
        filters[index].setDoubleClickReturnValue(true, 1);
        for (auto *b : {&locks[index], &solos[index], &loads[index]})
        {
            addAndMakeVisible(*b);
            b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff30392f));
            b->setColour(juce::TextButton::buttonOnColourId, accents[index]);
        }
        locks[index].setButtonText("LOCK");
        locks[index].setClickingTogglesState(true);
        locks[index].onClick = [this, i] { processor.setLocked(i, locks[(size_t)i].getToggleState()); };
        solos[index].setButtonText("SOLO");
        solos[index].setClickingTogglesState(true);
        solos[index].onClick = [this, i] { processor.setSolo(i, solos[(size_t)i].getToggleState()); };
        loads[index].setButtonText("LOAD");
        loads[index].onClick = [this, i] { chooseAudio(i); };
    }
    for (auto *b : {&importButton, &demoButton, &recipeButton, &saveButton, &stirButton, &playButton, &mixButton,
                    &stemButton, &spliceButton})
    {
        addAndMakeVisible(*b);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff30392f));
        b->setColour(juce::TextButton::buttonOnColourId, gold);
    }
    stirButton.setColour(juce::TextButton::buttonColourId, gold);
    stirButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff211b11));
    importButton.onClick = [this] { chooseAudio(); };
    demoButton.onClick = [this] { processor.loadDemo(); };
    recipeButton.onClick = [this] { chooseRecipe(); };
    saveButton.onClick = [this] { saveRecipe(); };
    stirButton.onClick = [this] { stirEnergy = 1.0f; processor.doReroll(); };
    playButton.setClickingTogglesState(true);
    playButton.onClick = [this] { processor.audition.store(playButton.getToggleState()); };
    mixButton.onClick = [this] { chooseExport(false); };
    stemButton.onClick = [this] { chooseExport(true); };
    spliceButton.setTooltip("Start the optional included Splice companion, then open it in your browser. Account "
                            "authentication remains in the companion.");
    spliceButton.onClick = [] { juce::URL("http://127.0.0.1:5173").launchInDefaultBrowser(); };
    addAndMakeVisible(motionButton);
    motionButton.setColour(juce::TextButton::buttonOnColourId, green);
    motionButton.setClickingTogglesState(true);
    motionButton.setToggleState(true, juce::dontSendNotification);
    motionButton.setTooltip("Pause decorative motion. Audio and controls remain active.");
    motionButton.onClick = [this] { motionEnabled = motionButton.getToggleState(); repaint(); };
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
    startTimerHz(30);
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
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
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
void GumboPotAudioProcessorEditor::drawPanel(juce::Graphics &g, juce::Rectangle<float> b, float alpha)
{
    g.setColour(juce::Colour(0xff171e1c).withAlpha(alpha));
    g.fillRect(b);
    g.setColour(juce::Colour(0xff777365).withAlpha(.45f));
    g.drawHorizontalLine((int)b.getY(), b.getX(), b.getRight());
    g.setColour(juce::Colours::black.withAlpha(.6f));
    g.drawHorizontalLine((int)b.getBottom(), b.getX(), b.getRight());
}
void GumboPotAudioProcessorEditor::paint(juce::Graphics &raw)
{
    juce::Graphics::ScopedSaveState state(raw);
    raw.addTransform(juce::AffineTransform::scale(scale));
    auto &g = raw;
    g.drawImageAt(surface, 0, 0);
    // Porcelain fascia, oxidised iron chassis and machined screws.
    g.setColour(juce::Colour(0xffe5d9bf)); g.fillRect(0, 0, 1440, 90);
    g.setColour(juce::Colour(0xff723e32)); g.fillRect(0, 86, 1440, 4);
    auto screw = [&g](float x, float y)
    {
        g.setColour(juce::Colour(0xff111715)); g.fillEllipse(x-4, y-3, 8, 8);
        g.setColour(juce::Colour(0xff8f8a78)); g.fillEllipse(x-3, y-3, 6, 6);
        g.setColour(juce::Colour(0xff333c34)); g.drawLine(x-2, y+1, x+2, y-1, 1);
    };
    for (float x : {15.0f, 1425.0f}) for (float y : {16.0f, 74.0f, 881.0f}) screw(x,y);
    // Bespoke pot seal: looped handles, steam and a stamped five-channel mark.
    g.setColour(juce::Colour(0xff343d33));
    g.fillRoundedRectangle(42, 32, 62, 37, 13);
    g.drawRoundedRectangle(33, 38, 19, 16, 7, 3);
    g.drawRoundedRectangle(94, 38, 19, 16, 7, 3);
    g.drawLine(40, 29, 106, 29, 4);
    for (int i=0;i<3;++i)
    {
        juce::Path steam; const float x=58.0f+i*15;
        steam.startNewSubPath(x,23); steam.cubicTo(x-6,17,x+5,16,x+1,9);
        g.strokePath(steam,juce::PathStrokeType(1.8f));
    }
    g.setColour(juce::Colour(0xffe5d9bf));
    for (int i=0;i<5;++i) g.fillRect(55+i*8,44,3,11);
    g.setColour(juce::Colour(0xff30392f));
    g.setFont(juce::FontOptions("Avenir Next Condensed", 48, juce::Font::bold));
    g.drawText("GUMBO POT",132,12,420,63,juce::Justification::centredLeft);
    for (int i=0;i<5;++i)
    {
        g.setColour(accents[(size_t)i].darker(.3f));
        g.fillRoundedRectangle(566+i*18,33,10,27,3);
    }
    text(g,ui.title,{720,27,420,32},14,juce::Colour(0xff343d33),juce::Justification::centredRight);
    // One continuous mixer deck, rather than five floating cards.
    drawPanel(g,{250,110,932,680});
    drawPanel(g,{20,110,218,680});
    drawPanel(g,{1204,110,216,680});
    text(g,"SAMPLES",{38,128,180,25},14,paper,juce::Justification::centredLeft,true);
    text(g,juce::String(getNumRows())+" IN PANTRY",{38,260,180,18},10,muted);
    text(g,"Double-click to load",{38,642,180,24},11,muted);
    text(g,"WAV / AIFF / FLAC",{38,693,180,20},10,muted);
    text(g,"BPM from filename",{38,716,180,16},10,muted);
    text(g,"RECIPE",{272,128,660,24},11,muted);
    text(g,ui.title,{272,154,660,37},23,paper,juce::Justification::centredLeft,true);
    text(g,"MIX",{272,217,660,20},10,muted);
    // A five-stop route stitched into the fascia follows playback tempo.
    for (int i=0;i<32;++i)
    {
        const bool lit=motionEnabled && activity>.01f && i==(int)(beatPhase*8)%32;
        g.setColour(lit?gold:juce::Colour(0xff4b5145));
        g.fillRect(744+i*13,221,lit?7:3,lit?8:4);
    }
    for (int i=0;i<5;++i)
    {
        const auto k=(size_t)i; const float x=264.0f+i*184, cx=x+86;
        const auto &info=ui.layers[k]; const auto ink=accents[k];
        if(i>0) {g.setColour(juce::Colour(0xff55594d).withAlpha(.45f));g.drawLine(x-6,273,x-6,691);}
        text(g,juce::String(i+1).paddedLeft('0',2),{x+8,260,30,20},10,muted);
        text(g,names[k],{x+8,282,156,24},15,paper,juce::Justification::centred,true);
        text(g,roles[k],{x+4,309,164,17},9,ink,juce::Justification::centred);
        // Hand-built top-view cast iron pot. The soup contains the real waveform.
        const auto pot=juce::Rectangle<float>(cx-64,343,128,80);
        g.setColour(juce::Colours::black.withAlpha(.5f));g.fillEllipse(pot.translated(0,6));
        g.setColour(juce::Colour(0xff0b100e));
        g.drawRoundedRectangle(cx-82,370,28,24,9,5);g.drawRoundedRectangle(cx+54,370,28,24,9,5);
        g.fillEllipse(pot);g.setColour(juce::Colour(0xff666b5b));g.drawEllipse(pot,2);
        auto soup=pot.reduced(8,7);
        g.setColour(info.active?ink.darker(.65f):juce::Colour(0xff242d27));g.fillEllipse(soup);
        g.setColour(ink.withAlpha(info.active?.7f:.15f));g.drawEllipse(soup,1);
        {
            juce::Graphics::ScopedSaveState clip(g);juce::Path mask;mask.addEllipse(soup);g.reduceClipRegion(mask);
            g.setColour(ink.withAlpha(.85f));
            for(int n=0;n<128;++n)
            {
                const float h=info.active?juce::jlimit(1.0f,43.0f,info.waveform[(size_t)n]*68):1;
                const float xx=cx-54+n*108.0f/128;
                g.drawLine(xx,383-h*.5f,xx,383+h*.5f,.8f);
            }
            if(info.active && motionEnabled && activity>.01f)
            {
                const float sweep=cx-54+std::fmod(beatPhase/4.0f,1.0f)*108;
                g.setColour(paper.withAlpha(.55f*activity));g.drawLine(sweep,350,sweep,419,1);
                for(int n=0;n<7;++n)
                {
                    const float phase=std::fmod(animationSeconds*(.2f+n*.018f)+n*.143f+i*.17f,1.0f);
                    const float bx=cx+std::sin(n*3.7f+i)*42;
                    const float by=405-phase*53, r=1.5f+phase*2;
                    g.setColour(paper.withAlpha((1-phase)*.55f*activity));g.drawEllipse(bx-r,by-r,r*2,r*2,.8f);
                }
            }
        }
        // Steam rises only with playback; stirring adds a short swirl.
        if(info.active && motionEnabled && (activity>.01f || stirEnergy>.01f))
            for(int n=0;n<3;++n)
            {
                juce::Path steam;
                for(int step=0;step<20;++step)
                {
                    const float t=step/19.0f;
                    const float xx=cx+(n-1)*24+std::sin(t*7-animationSeconds*2+n+i)* (4+t*5+stirEnergy*9);
                    const float yy=362-t*32;
                    if(step==0)steam.startNewSubPath(xx,yy);else steam.lineTo(xx,yy);
                }
                g.setColour(paper.withAlpha(.18f*juce::jmax(activity,stirEnergy)));
                g.strokePath(steam,juce::PathStrokeType(1.2f));
            }
        // Burner teeth illuminate with real output energy; no perpetual fake meter.
        for(int n=0;n<9;++n)
        {
            g.setColour(ink.withAlpha(.12f+activity*.65f));
            g.fillRect(cx-36+n*9,428.0f,5.0f,3.0f+(n%2)*2);
        }
        text(g,info.active?info.name:"Empty",{x+12,443,148,27},12,paper,juce::Justification::centred,true);
        text(g,info.active?juce::String(info.bpm,0)+" BPM / "+juce::String(info.duration,1)+" s":"Drop audio",{x+9,472,154,18},10,muted,juce::Justification::centred);
        text(g,"LEVEL",{x+25,498,122,15},9,muted,juce::Justification::centred);
        text(g,"PAN",{x+10,590,75,15},9,muted,juce::Justification::centred);
        text(g,"FILTER",{x+87,590,75,15},9,muted,juce::Justification::centred);
    }
    // Main lever is surrounded by a rotating stir track on each activation.
    g.setColour(juce::Colour(0xff4b5145));g.drawLine(280,742,460,742);g.drawLine(980,742,1155,742);
    for(int i=0;i<5;++i)
    {
        const float xx=294+i*33;
        const float yy=742+(motionEnabled?std::sin(animationSeconds*9-i*.6f)*stirEnergy*9:0);
        g.setColour(accents[(size_t)i]);g.fillEllipse(xx,yy-2,4,4);
    }
    text(g,processor.loading.load()?"Loading...":statusText(),{264,778,910,22},11,muted,juce::Justification::centred);
    drawPanel(g,{250,808,932,80});
    text(g,"OUTPUT",{278,815,90,14},9,muted,juce::Justification::centred);
    text(g,"BLEND",{406,815,90,14},9,muted,juce::Justification::centred);
    text(g,"VARIATION",{534,815,90,14},9,muted,juce::Justification::centred);
    text(g,"SEED",{669,821,150,14},9,muted,juce::Justification::centred);
    text(g,"TEMPO",{1222,128,180,22},11,muted);
    text(g,juce::String(processor.hostTempo.load(),1),{1222,160,180,53},44,paper,juce::Justification::centred,true);
    text(g,"BPM",{1222,214,180,20},11,muted,juce::Justification::centred);
    const bool running=processor.hostPlaying.load()||processor.audition.load();
    g.setColour(running?green:juce::Colour(0xff64675b));g.fillEllipse(1252,260,6,6);
    text(g,running?"PLAYING":"STOPPED",{1270,251,115,24},11,muted);
    // Segmented signal meter.
    const float peak=processor.outputPeak.load();
    for(int n=0;n<24;++n)
    {
        const float db=-60+n*60.0f/23;
        const bool lit=peak>juce::Decibels::decibelsToGain(db);
        g.setColour(lit?(n>21?accents[0]:green):juce::Colour(0xff343f34));
        g.fillRect(1226+n*7,303,4,20);
    }
    text(g,peak>0?juce::String(juce::Decibels::gainToDecibels(peak),1)+" dBFS":"-inf dBFS",{1222,337,180,22},12,muted,juce::Justification::centred);
    g.setColour(juce::Colour(0xff55594d));g.drawLine(1222,403,1402,403);
    text(g,"EXPORT",{1222,431,180,23},13,paper,juce::Justification::centredLeft,true);
    text(g,"44.1 kHz / 16-bit WAV",{1222,610,180,20},10,muted);
    text(g,"Tempo changes pitch",{1222,635,180,17},10,muted);
    g.setColour(juce::Colour(0xff55594d));g.drawLine(1222,681,1402,681);
    text(g,"SPLICE",{1222,699,180,20},12,paper,juce::Justification::centredLeft,true);
    text(g,"Separate browser companion",{1222,725,180,18},10,muted);
    for(int i=0;i<2;++i)
    {
        const float x=91.0f+i*68;
        g.setColour(juce::Colour(0xff747565));g.fillEllipse(x-12,837,24,24);
        g.setColour(juce::Colour(0xff0c1410));g.fillEllipse(x-8,841,16,16);
        g.setColour(juce::Colour(0xff414c3e));g.drawEllipse(x-5,844,10,10,1);
        text(g,i==0?"L":"R",{x-12,866,24,15},9,muted,juce::Justification::centred);
    }
}
juce::String GumboPotAudioProcessorEditor::statusText() const
{
    // Preserve actionable errors; keep the idle interface free of slogans.
    if(ui.message.startsWithIgnoreCase("Ready") || ui.message.startsWithIgnoreCase("Loaded house")) return {};
    return ui.message;
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
    set(motionButton, 1215, 29, 185, 30);
    set(importButton, 38, 184, 180, 32);
    set(demoButton, 38, 223, 180, 30);
    set(pantryList, 32, 288, 194, 342);
    set(recipeButton, 38, 742, 86, 30);
    set(saveButton, 129, 742, 89, 30);
    for (int i = 0; i < 5; ++i)
    {
        const int x = 264 + i * 184;
        set(gains[(size_t)i], x + 43, 511, 86, 78);
        set(pans[(size_t)i], x + 13, 601, 65, 65);
        set(filters[(size_t)i], x + 94, 601, 65, 65);
        set(locks[(size_t)i], x + 13, 669, 68, 21);
        set(solos[(size_t)i], x + 91, 669, 68, 21);
        set(loads[(size_t)i], x + 25, 696, 122, 24);
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
    const bool changed = next.revision != ui.revision || next.pantryPaths != ui.pantryPaths;
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float)juce::jlimit(0.0, .1, (now-lastFrameMs)/1000.0);
    lastFrameMs = now;
    const bool running = processor.hostPlaying.load() || processor.audition.load() || processor.outputPeak.load()>.0001f;
    const float target = running ? juce::jlimit(.15f, 1.0f, processor.outputPeak.load()*2) : 0;
    activity += (target-activity)*(1-std::exp(-dt*8));
    stirEnergy *= std::exp(-dt*2);
    if(motionEnabled && (activity>.01f || stirEnergy>.01f))
    {
        animationSeconds = std::fmod(animationSeconds+dt, 1000.0f);
        beatPhase = std::fmod(beatPhase+dt*(float)processor.hostTempo.load()/60.0f, 4.0f);
    }
    ui = std::move(next);
    if (changed) pantryList.updateContent();
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
    motionButton.setButtonText(motionEnabled ? "MOTION ON" : "MOTION OFF");
    if (isShowing()) repaint();
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
        g.drawText("120 BPM", 10, 25, width - 20, 12, juce::Justification::centredLeft);
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
