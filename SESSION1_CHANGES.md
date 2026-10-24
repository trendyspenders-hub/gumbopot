# Exact Session 1 edits

Each diff records FILE / LINE / BEFORE → AFTER. Parameter IDs, order, ranges and defaults are frozen and unchanged.

## CMakeLists.txt

WHY: Declare the existing output-only generator as an instrument for host discovery.

```diff
--- before/CMakeLists.txt
+++ after/CMakeLists.txt
@@ -10,6 +10,7 @@
 
 juce_add_plugin(GumboPot
     COMPANY_NAME "GumboPot"
+    IS_SYNTH TRUE
     PLUGIN_MANUFACTURER_CODE Gmbo
     PLUGIN_CODE Gpot
     FORMATS VST3 AU Standalone
```

## Source/PluginEditor.h

WHY: Fixed arrays avoid copying/moving non-movable JUCE Slider and Label controls.

```diff
--- before/Source/PluginEditor.h
+++ after/Source/PluginEditor.h
@@ -2,6 +2,7 @@
 
 #include <juce_gui_extra/juce_gui_extra.h>
 #include "PluginProcessor.h"
+#include <array>
 
 class GumboPotAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::FileDragAndDropTarget,
@@ -26,8 +27,8 @@
     using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
 
     juce::Slider master, gumbo, weirdness, seedSlider;
-    std::vector<juce::Slider> layerGains, layerPans, layerFilters;
-    std::vector<juce::Label> layerLabels;
+    std::array<juce::Slider, 5> layerGains, layerPans, layerFilters;
+    std::array<juce::Label, 5> layerLabels;
     std::vector<std::unique_ptr<SliderAttachment>> attachments;
 
     juce::TextButton loadStackButton { "Load Stack..." };
```

## Source/PluginEditor.cpp

WHY: Use those arrays without reserve/emplace; guard asynchronous chooser access with SafePointer.

```diff
--- before/Source/PluginEditor.cpp
+++ after/Source/PluginEditor.cpp
@@ -3,7 +3,6 @@
 GumboPotAudioProcessorEditor::GumboPotAudioProcessorEditor (GumboPotAudioProcessor& p)
     : AudioProcessorEditor (&p), processor (p)
 {
-    layerGains.reserve (5); layerPans.reserve (5); layerFilters.reserve (5); layerLabels.reserve (5);
     attachments.reserve (20);
 
     addSlider (master,     "master",    juce::Slider::RotaryVerticalDrag);
@@ -14,19 +13,15 @@
     for (int i = 0; i < 5; ++i)
     {
         const auto n = juce::String (i + 1);
-        layerLabels.emplace_back();
-        layerLabels.back().setText (n, juce::dontSendNotification);
-        layerLabels.back().setJustificationType (juce::Justification::centred);
-        addAndMakeVisible (layerLabels.back());
+        layerLabels[(size_t) i].setText (n, juce::dontSendNotification);
+        layerLabels[(size_t) i].setJustificationType (juce::Justification::centred);
+        addAndMakeVisible (layerLabels[(size_t) i]);
 
-        layerGains.emplace_back();
-        addSlider (layerGains.back(), "layer" + n + "gain", juce::Slider::LinearVertical);
+        addSlider (layerGains[(size_t) i], "layer" + n + "gain", juce::Slider::LinearVertical);
 
-        layerPans.emplace_back();
-        addSlider (layerPans.back(), "layer" + n + "pan", juce::Slider::RotaryVerticalDrag);
+        addSlider (layerPans[(size_t) i], "layer" + n + "pan", juce::Slider::RotaryVerticalDrag);
 
-        layerFilters.emplace_back();
-        addSlider (layerFilters.back(), "layer" + n + "filter", juce::Slider::LinearVertical);
+        addSlider (layerFilters[(size_t) i], "layer" + n + "filter", juce::Slider::LinearVertical);
     }
 
     addAndMakeVisible (loadStackButton);
@@ -35,10 +30,10 @@
         auto chooser = std::make_shared<juce::FileChooser> ("Open a Gumbo stack (.json)",
                                                             processor.currentStackFile, "*.json");
         chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
-                              [this, chooser] (const juce::FileChooser& fc)
+                              [safeThis = juce::Component::SafePointer<GumboPotAudioProcessorEditor> (this), chooser] (const juce::FileChooser& fc)
                               {
-                                  if (processor.loadStack (fc.getResult()))
-                                      statusLabel.setText (processor.getStackName(), juce::dontSendNotification);
+                                  if (safeThis != nullptr && safeThis->processor.loadStack (fc.getResult()))
+                                      safeThis->statusLabel.setText (safeThis->processor.getStackName(), juce::dontSendNotification);
                               });
     };
 
```

## Source/LayerPlayer.cpp

WHY: Fix inverted tempo ratio, pan angle, one-channel filter indexing, cutoff above Nyquist and loop wrapping; preserve resampling and mono mix behavior.

```diff
--- before/Source/LayerPlayer.cpp
+++ after/Source/LayerPlayer.cpp
@@ -14,7 +14,7 @@
         f.prepare (spec);
         f.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
         f.setResonance (0.7f);
-        f.setCutoffFrequency (18000.0f);
+        f.setCutoffFrequency (juce::jmin (18000.0f, (float) outputSampleRate * 0.49f));
     }
 }
 
@@ -62,7 +62,7 @@
     // 180 Hz .. 18 kHz, exponential feel
     const float cutoff = 180.0f * std::pow (100.0f, juce::jlimit (0.0f, 1.0f, normalized));
     for (auto& f : filter)
-        f.setCutoffFrequency (cutoff);
+        f.setCutoffFrequency (juce::jmin (cutoff, (float) outputSampleRate * 0.49f));
 }
 
 void LayerPlayer::process (juce::AudioBuffer<float>& output, int startSample, int numSamples)
@@ -74,9 +74,9 @@
 
     double ratio = sourceSampleRate / juce::jmax (1.0, outputSampleRate);
     if (sync)
-        ratio *= sampleBpm / hostBpm;
+        ratio *= hostBpm / sampleBpm;
 
-    const float angle = (pan + 1.0f) * juce::MathConstants<float>::halfPi;
+    const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
     const float leftGain  = gain * std::cos (angle) * juce::MathConstants<float>::sqrt2;
     const float rightGain = gain * std::sin (angle) * juce::MathConstants<float>::sqrt2;
 
@@ -84,7 +84,7 @@
     {
         const int index0 = (int) position;
         const float frac = (float) (position - (double) index0);
-        const int index1 = juce::jmin (index0 + 1, sampleLength - 1);
+        const int index1 = (index0 + 1) % sampleLength;
 
         float monoSample = 0.0f;
         for (int ch = 0; ch < sampleData.getNumChannels(); ++ch)
@@ -95,12 +95,11 @@
         for (int outCh = 0; outCh < output.getNumChannels(); ++outCh)
         {
             float v = monoSample * (outCh == 0 ? leftGain : rightGain);
-            v = filter[juce::jmin (outCh, maxChannels - 1)].processSample (outCh, v);
+            v = filter[juce::jmin (outCh, maxChannels - 1)].processSample (0, v);
             output.addSample (outCh, startSample + s, v);
         }
 
         position += ratio;
-        if (position >= (double) (sampleLength - 1))
-            position -= (double) (sampleLength - 1);   // loop the layer
+        position = std::fmod (position, (double) sampleLength);   // wrap even at high ratios
     }
 }
```

## Source/PluginProcessor.cpp

WHY: JUCE 8.0.4 AudioChannelSet has no isMono/isStereo methods: compare with mono()/stereo(). Reject disabled output because the processor requires an output bus.

```diff
--- before/Source/PluginProcessor.cpp
+++ after/Source/PluginProcessor.cpp
@@ -41,7 +41,7 @@
 bool GumboPotAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
 {
     const auto& out = layouts.getChannelSet (false, 0);
-    return out.isDisabled() || out.isMono() || out.isStereo();
+    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
 }
 
 void GumboPotAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
```

## Deferred choices

Stereo samples: A (recommended now), preserve existing mono downmix; B, add stereo balance later. Reroll: A (recommended now), retain clock-mixed seed; B, implement deterministic sequence later. No later-session features added.
