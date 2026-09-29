#include "PianoKeyboardDisplay.h"

namespace
{
    // a mouse click cannot convey how hard a key was struck, so every mouse triggered note uses
    // this fixed velocity, matching the value used for computer keyboard input.
    constexpr int mouseClickVelocity = 74;

    // this mirrors the computer keyboard mapping set up in the editor, used only to draw the
    // matching key letter on the piano when that display option is switched on.
    struct QwertyMapping
    {
        int midiNote;
        char keyLetter;
    };

    const QwertyMapping qwertyMappings[] =
    {
        { 59, 'Q' },
        { 60, 'A' }, { 61, 'W' },
        { 62, 'S' }, { 63, 'E' },
        { 64, 'D' },
        { 65, 'F' }, { 66, 'T' },
        { 67, 'G' }, { 68, 'Y' },
        { 69, 'H' }, { 70, 'U' },
        { 71, 'J' },
        { 72, 'K' }, { 73, 'O' },
        { 74, 'L' }, { 75, 'P' },
    };

    const char* const noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

juce::String PianoKeyboardDisplay::noteNameFor(int midiNote, bool includeOctave)
{
    // this splits the note into its letter name and its octave number, using the same octave
    // numbering as the rest of the plugin, where midi note 60 is C4.
    int pitchClass = midiNote % 12;
    int octave = midiNote / 12 - 1;
    juce::String name = noteNames[pitchClass];
    return includeOctave ? name + juce::String(octave) : name;
}

juce::String PianoKeyboardDisplay::qwertyKeyFor(int midiNote)
{
    for (const auto& mapping : qwertyMappings)
        if (mapping.midiNote == midiNote)
            return juce::String::charToString((juce::juce_wchar)mapping.keyLetter);

    return {};
}

std::vector<PianoKeyboardDisplay::KeyboardKeyMapping> PianoKeyboardDisplay::getKeyboardKeyMappings()
{
    std::vector<KeyboardKeyMapping> result;

    for (const auto& mapping : qwertyMappings)
        result.push_back({ mapping.keyLetter, mapping.midiNote });

    return result;
}

void PianoKeyboardDisplay::setShowQwertyLabels(bool shouldShow)
{
    showQwertyLabels = shouldShow;
    repaint();
}

void PianoKeyboardDisplay::setShowAllKeyLabels(bool shouldShow)
{
    showAllKeyLabels = shouldShow;
    repaint();
}

PianoKeyboardDisplay::PianoKeyboardDisplay(JuicoliciousGrandPianoProcessor& processorToUse)
    : processor(processorToUse)
{
    startTimerHz(30);
}

void PianoKeyboardDisplay::timerCallback()
{
    repaint();
}

// this function reduces the note to its position within one octave, so the same 12 values cover
// the whole keyboard range.
bool PianoKeyboardDisplay::isWhiteNote(int midiNote)
{
    int pitchClass = midiNote % 12;
    return pitchClass == 0 || pitchClass == 2 || pitchClass == 4 || pitchClass == 5
        || pitchClass == 7 || pitchClass == 9 || pitchClass == 11;
}

void PianoKeyboardDisplay::resized()
{
    layoutKeys();
}

int PianoKeyboardDisplay::findKeyAtPosition(juce::Point<float> position) const
{
    for (const auto& key : blackKeys)
        if (key.bounds.contains(position))
            return key.midiNote;

    for (const auto& key : whiteKeys)
        if (key.bounds.contains(position))
            return key.midiNote;

    return -1;
}

void PianoKeyboardDisplay::mouseDown(const juce::MouseEvent& event)
{
    // clicking a key should still hand keyboard focus to the editor, the same way clicking
    // anywhere else in the window already does, so computer keyboard playing keeps working.
    if (auto* parent = getParentComponent())
        parent->grabKeyboardFocus();

    int note = findKeyAtPosition(event.position);

    if (note < 0)
        return;

    mouseHeldNote = note;
    processor.addKeyboardNoteOn(note, mouseClickVelocity);
}

void PianoKeyboardDisplay::mouseDrag(const juce::MouseEvent& event)
{
    // this lets a single mouse drag glide across several keys, releasing the previous key and
    // pressing the new one each time the mouse crosses into a different key.
    int note = findKeyAtPosition(event.position);

    if (note == mouseHeldNote)
        return;

    if (mouseHeldNote >= 0)
        processor.addKeyboardNoteOff(mouseHeldNote);

    mouseHeldNote = note;

    if (note >= 0)
        processor.addKeyboardNoteOn(note, mouseClickVelocity);
}

void PianoKeyboardDisplay::mouseUp(const juce::MouseEvent&)
{
    if (mouseHeldNote >= 0)
    {
        processor.addKeyboardNoteOff(mouseHeldNote);
        mouseHeldNote = -1;
    }
}

void PianoKeyboardDisplay::layoutKeys()
{
    whiteKeys.clear();
    blackKeys.clear();

    int totalWhiteKeys = 0;

    for (int note = lowestNote; note <= highestNote; ++note)
        if (isWhiteNote(note))
            ++totalWhiteKeys;

    if (totalWhiteKeys == 0)
        return;

    float whiteKeyWidth = (float)getWidth() / (float)totalWhiteKeys;
    float whiteKeyHeight = (float)getHeight();
    float blackKeyWidth = whiteKeyWidth * 0.62f;
    float blackKeyHeight = whiteKeyHeight * 0.62f;

    int whiteIndex = 0;

    for (int note = lowestNote; note <= highestNote; ++note)
    {
        if (isWhiteNote(note))
        {
            KeyInfo info;
            info.midiNote = note;
            info.bounds = juce::Rectangle<float>((float)whiteIndex * whiteKeyWidth, 0.0f, whiteKeyWidth, whiteKeyHeight);
            whiteKeys.push_back(info);
            ++whiteIndex;
        }
        else
        {
            // a black key sits centred on the boundary between the white key before it and the
            // white key after it, so it is placed using whiteIndex before that boundary is crossed.
            KeyInfo info;
            info.midiNote = note;
            float boundaryX = (float)whiteIndex * whiteKeyWidth;
            info.bounds = juce::Rectangle<float>(boundaryX - blackKeyWidth * 0.5f, 0.0f, blackKeyWidth, blackKeyHeight);
            blackKeys.push_back(info);
        }
    }
}

void PianoKeyboardDisplay::paint(juce::Graphics& g)
{
    juce::Colour whiteKeyColour(0xfff2f7fa);
    juce::Colour whiteKeyPressedColour(0xffd0d4d8);
    juce::Colour blackKeyColour(0xff1c1f24);
    juce::Colour blackKeyPressedColour(0xffb0b6bc);
    juce::Colour keyOutline(0xff2a3540);

    float whiteCornerSize = 4.0f;
    float blackCornerSize = 2.5f;

    for (const auto& key : whiteKeys)
    {
        // only the bottom two corners are rounded here, so the key reads as a real piano key
        // rather than a plain rounded box.
        juce::Path keyPath;
        keyPath.addRoundedRectangle(key.bounds.getX(), key.bounds.getY(), key.bounds.getWidth(), key.bounds.getHeight(),
            whiteCornerSize, whiteCornerSize, false, false, true, true);

        bool pressed = processor.isKeyDown(key.midiNote);
        g.setColour(pressed ? whiteKeyPressedColour : whiteKeyColour);
        g.fillPath(keyPath);

        g.setColour(keyOutline);
        g.strokePath(keyPath, juce::PathStrokeType(1.0f));

        if (key.midiNote % 12 == 0 || showAllKeyLabels)
        {
            auto labelArea = key.bounds.withTrimmedTop(key.bounds.getHeight() - 22.0f).toNearestInt();
            g.setColour(juce::Colour(0xff4a5f70));
            g.setFont(juce::Font(13.0f, juce::Font::plain));
            g.drawFittedText(noteNameFor(key.midiNote), labelArea, juce::Justification::centred, 1);
        }

        if (showQwertyLabels)
        {
            juce::String qwertyLabel = qwertyKeyFor(key.midiNote);

            if (qwertyLabel.isNotEmpty())
            {
                auto qwertyArea = key.bounds.withTrimmedTop(key.bounds.getHeight() - 46.0f).withHeight(20.0f).toNearestInt();
                g.setColour(juce::Colour(0xff2f6f8f));
                g.setFont(juce::Font(15.0f, juce::Font::bold));
                g.drawFittedText(qwertyLabel, qwertyArea, juce::Justification::centred, 1);
            }
        }
    }

    for (const auto& key : blackKeys)
    {
        juce::Path keyPath;
        keyPath.addRoundedRectangle(key.bounds.getX(), key.bounds.getY(), key.bounds.getWidth(), key.bounds.getHeight(),
            blackCornerSize, blackCornerSize, false, false, true, true);

        bool pressed = processor.isKeyDown(key.midiNote);
        g.setColour(pressed ? blackKeyPressedColour : blackKeyColour);
        g.fillPath(keyPath);

        if (showAllKeyLabels)
        {
            auto labelArea = key.bounds.withTrimmedTop(key.bounds.getHeight() - 18.0f).toNearestInt();
            g.setColour(juce::Colour(0xffb8c4cc));
            g.setFont(juce::Font(11.0f, juce::Font::plain));
            g.drawFittedText(noteNameFor(key.midiNote, false), labelArea, juce::Justification::centred, 1);
        }

        if (showQwertyLabels)
        {
            juce::String qwertyLabel = qwertyKeyFor(key.midiNote);

            if (qwertyLabel.isNotEmpty())
            {
                auto qwertyArea = key.bounds.withTrimmedTop(key.bounds.getHeight() - 40.0f).withHeight(18.0f).toNearestInt();
                g.setColour(juce::Colour(0xff9fd8f0));
                g.setFont(juce::Font(13.0f, juce::Font::bold));
                g.drawFittedText(qwertyLabel, qwertyArea, juce::Justification::centred, 1);
            }
        }
    }
}