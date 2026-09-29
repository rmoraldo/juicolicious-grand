#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PianoKeyboardDisplay : public juce::Component, private juce::Timer
{
public:
    explicit PianoKeyboardDisplay(JuicoliciousGrandPianoProcessor& processorToUse);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    // these two control whether the computer keyboard letter and the note name are drawn on
    // each key. both can be on at once, since they show different information.
    void setShowQwertyLabels(bool shouldShow);
    void setShowAllKeyLabels(bool shouldShow);

    // this pairs one computer keyboard key with the midi note it plays.
    struct KeyboardKeyMapping
    {
        int keyCode;
        int midiNote;
    };

    // this returns the single shared list of keyboard keys and the notes they play, so the
    // editor's input handling and this display's labels always agree with each other.
    static std::vector<KeyboardKeyMapping> getKeyboardKeyMappings();

private:
    void timerCallback() override;
    void layoutKeys();
    static bool isWhiteNote(int midiNote);
    static juce::String noteNameFor(int midiNote, bool includeOctave = true);
    static juce::String qwertyKeyFor(int midiNote);

    // this returns the midi note under the given position, or negative one if the position is
    // not over any key. black keys are checked first since they are drawn on top of white keys.
    int findKeyAtPosition(juce::Point<float> position) const;

    JuicoliciousGrandPianoProcessor& processor;

    // this holds the note currently held down by the mouse, or negative one if the mouse is not
    // currently pressing a key.
    int mouseHeldNote = -1;

    bool showQwertyLabels = false;
    bool showAllKeyLabels = false;

    static constexpr int lowestNote = 21;
    static constexpr int highestNote = 108;

    struct KeyInfo
    {
        int midiNote;
        juce::Rectangle<float> bounds;
    };

    std::vector<KeyInfo> whiteKeys;
    std::vector<KeyInfo> blackKeys;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoKeyboardDisplay)
};