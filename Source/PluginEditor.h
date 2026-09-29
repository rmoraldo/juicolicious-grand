#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PianoKeyboardDisplay.h"
#include "PianoDisplay.h"

class JuicoliciousGrandPianoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JuicoliciousGrandPianoEditor(JuicoliciousGrandPianoProcessor&);
    ~JuicoliciousGrandPianoEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    bool keyStateChanged(bool isKeyDown) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    JuicoliciousGrandPianoProcessor& processorRef;

    juce::Image logoImage;
    juce::Font titleFont;

    PianoDisplay pianoDisplay;

    juce::Slider attackSlider, releaseSlider;
    juce::Label attackLabel, releaseLabel;
    juce::Label attackValueLabel, releaseValueLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;

    juce::ToggleButton qwertyToggle;
    juce::ToggleButton allLabelsToggle;

    PianoKeyboardDisplay pianoKeyboardDisplay;

    // each entry maps one computer keyboard key to one midi note. currentlyDown tracks whether
    // that key is held right now, so an OS key repeat event does not retrigger the note.
    struct KeyNoteMapping
    {
        int keyCode;
        int midiNote;
        bool currentlyDown = false;
    };

    std::vector<KeyNoteMapping> keyNoteMappings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JuicoliciousGrandPianoEditor)
};