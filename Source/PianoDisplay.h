#pragma once
#include <JuceHeader.h>

class PianoDisplay : public juce::LookAndFeel_V4
{
public:
    PianoDisplay();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
        juce::Slider& slider) override;
};