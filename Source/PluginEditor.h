#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class KitchenLookAndFeel final : public juce::LookAndFeel_V4
{
  public:
    KitchenLookAndFeel();
    void drawRotarySlider(juce::Graphics &, int, int, int, int, float, float, float, juce::Slider &) override;
    void drawButtonBackground(juce::Graphics &, juce::Button &, const juce::Colour &, bool, bool) override;
    juce::Font getTextButtonFont(juce::TextButton &, int height) override;
};
class GumboPotAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::FileDragAndDropTarget,
                                     private juce::Timer,
                                     private juce::ListBoxModel
{
  public:
    explicit GumboPotAudioProcessorEditor(GumboPotAudioProcessor &);
    ~GumboPotAudioProcessorEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress &) override;

  private:
    void timerCallback() override;
    bool isInterestedInFileDrag(const juce::StringArray &) override;
    void filesDropped(const juce::StringArray &, int, int) override;
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics &, int, int, bool) override;
    void listBoxItemDoubleClicked(int, const juce::MouseEvent &) override;
    void chooseAudio(int target = -1);
    void chooseRecipe();
    void saveRecipe();
    void chooseExport(bool stems);
    void addSlider(juce::Slider &, const juce::String &, juce::Colour);
    void drawPanel(juce::Graphics &, juce::Rectangle<float>, float alpha = 0.92f);
    void text(juce::Graphics &, const juce::String &, juce::Rectangle<float>, float, juce::Colour,
              int = juce::Justification::centredLeft, bool = false);
    GumboPotAudioProcessor &processor;
    KitchenLookAndFeel theme;
    juce::Image backdrop;
    GumboPotAudioProcessor::UiState ui;
    std::array<juce::Slider, 5> gains, pans, filters;
    std::array<juce::TextButton, 5> locks, solos, loads;
    juce::Slider master, gumbo, weirdness, seed;
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<Attachment>> attachments;
    juce::TextButton importButton{"IMPORT AUDIO"}, demoButton{"LOAD HOUSE DEMO"}, recipeButton{"OPEN RECIPE"},
        saveButton{"SAVE RECIPE"};
    juce::TextButton stirButton{"STIR THE POT"}, playButton{"AUDITION"}, mixButton{"EXPORT MIX  /  WAV"},
        stemButton{"EXPORT STEMS  /  ZIP"};
    juce::TextButton spliceButton{"OPEN SPLICE COMPANION"};
    juce::ComboBox bars;
    juce::ListBox pantryList{"Local pantry", this};
    juce::TooltipWindow tooltips{this, 500};
    juce::String lastMessage;
    float scale = 1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GumboPotAudioProcessorEditor)
};
