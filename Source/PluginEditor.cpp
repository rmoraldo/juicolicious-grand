#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

JuicoliciousGrandPianoEditor::JuicoliciousGrandPianoEditor(JuicoliciousGrandPianoProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), pianoKeyboardDisplay(p)
{
    setSize(1000, 753);
    startTimerHz(15);

    logoImage = juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);

    // this loads the actual font file we embedded, so the title renders identically on every
    // machine regardless of what fonts happen to be installed on it.
    juce::Typeface::Ptr titleTypeface = juce::Typeface::createSystemTypefaceFor(BinaryData::titlefont_ttf, BinaryData::titlefont_ttfSize);
    titleFont = juce::Font(titleTypeface).withHeight(44.0f);

    setLookAndFeel(&pianoDisplay);

    auto setupKnob = [this](juce::Slider& slider, juce::Label& titleLabel, juce::Label& valueLabel, const juce::String& text)
        {
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible(slider);

            titleLabel.setText(text, juce::dontSendNotification);
            titleLabel.setJustificationType(juce::Justification::centred);
            titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff1c3347));
            titleLabel.setFont(juce::Font(15.0f, juce::Font::bold));
            addAndMakeVisible(titleLabel);

            valueLabel.setJustificationType(juce::Justification::centred);
            valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xff1c3347));
            valueLabel.setFont(juce::Font(13.0f, juce::Font::plain));
            addAndMakeVisible(valueLabel);

            // this keeps the value text in step with the knob for every kind of change, not just
            // dragging it by hand, since a saved state or host automation also calls setValue.
            slider.onValueChange = [&slider, &valueLabel]
                {
                    valueLabel.setText(juce::String(slider.getValue(), 3), juce::dontSendNotification);
                };
        };

    setupKnob(attackSlider, attackLabel, attackValueLabel, "Attack");
    setupKnob(releaseSlider, releaseLabel, releaseValueLabel, "Release");

    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "attack", attackSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "release", releaseSlider);

    // this sets the very first value shown, since onValueChange only fires on a later change.
    attackValueLabel.setText(juce::String(attackSlider.getValue(), 3), juce::dontSendNotification);
    releaseValueLabel.setText(juce::String(releaseSlider.getValue(), 3), juce::dontSendNotification);

    addAndMakeVisible(pianoKeyboardDisplay);

    qwertyToggle.setButtonText("Toggle QWERTY Display");
    qwertyToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff1c3347));
    qwertyToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff2f6f8f));
    qwertyToggle.onClick = [this] { pianoKeyboardDisplay.setShowQwertyLabels(qwertyToggle.getToggleState()); };
    addAndMakeVisible(qwertyToggle);

    allLabelsToggle.setButtonText("Show All Key Labels");
    allLabelsToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff1c3347));
    allLabelsToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff2f6f8f));
    allLabelsToggle.onClick = [this] { pianoKeyboardDisplay.setShowAllKeyLabels(allLabelsToggle.getToggleState()); };
    addAndMakeVisible(allLabelsToggle);

    // this builds the editor's own input handling list from the display's shared mapping, so
    // the two never disagree about which key plays which note.
    for (const auto& mapping : PianoKeyboardDisplay::getKeyboardKeyMappings())
        keyNoteMappings.push_back({ mapping.keyCode, mapping.midiNote, false });

    setWantsKeyboardFocus(true);
}

JuicoliciousGrandPianoEditor::~JuicoliciousGrandPianoEditor()
{
    setLookAndFeel(nullptr);
}

void JuicoliciousGrandPianoEditor::mouseDown(const juce::MouseEvent&)
{
    grabKeyboardFocus();
}

// the OS repeats key events for a held key, which would retrigger the note over and over. this
// instead checks each mapped key's actual current down or up state and only reacts to a real change.
bool JuicoliciousGrandPianoEditor::keyStateChanged(bool)
{
    constexpr int keyboardVelocity = 74;

    for (auto& mapping : keyNoteMappings)
    {
        bool isDown = juce::KeyPress::isKeyCurrentlyDown(mapping.keyCode);

        if (isDown == mapping.currentlyDown)
            continue;

        mapping.currentlyDown = isDown;

        if (isDown)
            processorRef.addKeyboardNoteOn(mapping.midiNote, keyboardVelocity);
        else
            processorRef.addKeyboardNoteOff(mapping.midiNote);
    }

    return true;
}

void JuicoliciousGrandPianoEditor::paint(juce::Graphics& g)
{
    juce::Colour topColour(0xffbfe0f0);
    juce::Colour bottomColour(0xff8ec2dc);

    juce::ColourGradient backgroundGradient(topColour, 0.0f, 0.0f, bottomColour, 0.0f, (float)getHeight(), false);
    g.setGradientFill(backgroundGradient);
    g.fillAll();

    auto bounds = getLocalBounds().reduced(20);
    auto topArea = bounds.removeFromTop(230);

    int logoSize = 180;
    auto logoRow = topArea.removeFromTop(logoSize);

    // this centres the logo horizontally by giving it a fixed size square in the middle of the
    // header width, rather than measuring it against the title text below.
    auto logoBounds = juce::Rectangle<int>(logoRow.getX() + (logoRow.getWidth() - logoSize) / 2, logoRow.getY(), logoSize, logoSize);

    if (logoImage.isValid())
        g.drawImageWithin(logoImage, logoBounds.getX(), logoBounds.getY(), logoBounds.getWidth(), logoBounds.getHeight(), juce::RectanglePlacement::centred);

    g.setColour(juce::Colour(0xff1c3347));
    g.setFont(titleFont);
    g.drawFittedText("Juicolicious Grand", topArea.removeFromTop(48), juce::Justification::centred, 1);
}

void JuicoliciousGrandPianoEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    bounds.removeFromTop(230);

    auto knobArea = bounds.removeFromTop(110);
    int knobWidth = 100;
    int spacing = 30;
    int totalKnobWidth = knobWidth * 2 + spacing;
    int startX = knobArea.getX() + (knobArea.getWidth() - totalKnobWidth) / 2;

    auto attackArea = juce::Rectangle<int>(startX, knobArea.getY(), knobWidth, knobArea.getHeight());
    auto releaseArea = juce::Rectangle<int>(startX + knobWidth + spacing, knobArea.getY(), knobWidth, knobArea.getHeight());

    attackLabel.setBounds(attackArea.removeFromTop(24));
    attackValueLabel.setBounds(attackArea.removeFromBottom(20));
    attackSlider.setBounds(attackArea);

    releaseLabel.setBounds(releaseArea.removeFromTop(24));
    releaseValueLabel.setBounds(releaseArea.removeFromBottom(20));
    releaseSlider.setBounds(releaseArea);

    bounds.removeFromTop(15);

    auto toggleArea = bounds.removeFromTop(28);
    int toggleWidth = 220;
    int toggleSpacing = 30;
    int totalToggleWidth = toggleWidth * 2 + toggleSpacing;
    int toggleStartX = toggleArea.getX() + (toggleArea.getWidth() - totalToggleWidth) / 2;

    qwertyToggle.setBounds(toggleStartX, toggleArea.getY(), toggleWidth, toggleArea.getHeight());
    allLabelsToggle.setBounds(toggleStartX + toggleWidth + toggleSpacing, toggleArea.getY(), toggleWidth, toggleArea.getHeight());

    bounds.removeFromTop(15);
    pianoKeyboardDisplay.setBounds(bounds);
}

void JuicoliciousGrandPianoEditor::timerCallback()
{
    repaint();
}