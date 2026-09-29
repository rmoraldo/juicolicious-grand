#include "PianoDisplay.h"

PianoDisplay::PianoDisplay()
{
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff1c3347));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, juce::Colour(0xff1c3347));
}

void PianoDisplay::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider&)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(6.0f);
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    float centreX = bounds.getCentreX();
    float centreY = bounds.getCentreY();
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Colour knobTop(0xff35586e);
    juce::Colour knobBottom(0xff1e3646);
    juce::Colour accentColour(0xff8fd6f0);
    juce::Colour trackColour(0xffaac4d4);

    // this gradient makes the flat circle read as a slightly domed knob.
    juce::ColourGradient knobGradient(knobTop, centreX, centreY - radius, knobBottom, centreX, centreY + radius, false);
    g.setGradientFill(knobGradient);
    g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    g.setColour(knobBottom);
    g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    float trackRadius = radius + 6.0f;

    juce::Path trackArc;
    trackArc.addCentredArc(centreX, centreY, trackRadius, trackRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(trackColour);
    g.strokePath(trackArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(centreX, centreY, trackRadius, trackRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(accentColour);
    g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // this path is built pointing straight up, then rotated into place around the knob centre.
    juce::Path pointer;
    float pointerLength = radius * 0.75f;
    float pointerThickness = 3.0f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(accentColour);
    g.fillPath(pointer);
}