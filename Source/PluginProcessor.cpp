#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>
#include <algorithm>

namespace
{
    // this struct holds one root note's info: its pitch, the label used in the filename, and
    // which velocity layers actually exist for it. bit0 is v1, bit1 is v2, bit2 is v3, bit3 is v4.
    struct RootAvailability
    {
        int midiNote;
        const char* label;
        int velocityMask;
    };

    const RootAvailability sustainRootTable[] =
    {
        { 22,  "A#-1", 0b1110 },
        { 24,  "C0",   0b1111 },
        { 27,  "D#0",  0b1111 },
        { 30,  "F#0",  0b1111 },
        { 33,  "A0",   0b1111 },
        { 36,  "C1",   0b1111 },
        { 39,  "D#1",  0b1111 },
        { 42,  "F#1",  0b1111 },
        { 45,  "A1",   0b1111 },
        { 48,  "C2",   0b1111 },
        { 50,  "D2",   0b1111 },
        { 52,  "E2",   0b1111 },
        { 54,  "F#2",  0b1111 },
        { 56,  "G#2",  0b1111 },
        { 58,  "A#2",  0b1111 },
        { 60,  "C3",   0b1111 },
        { 62,  "D3",   0b1111 },
        { 64,  "E3",   0b1111 },
        { 66,  "F#3",  0b1111 },
        { 68,  "G#3",  0b1111 },
        { 70,  "A#3",  0b1111 },
        { 72,  "C4",   0b1111 },
        { 74,  "D4",   0b1111 },
        { 76,  "E4",   0b1111 },
        { 78,  "F#4",  0b1111 },
        { 80,  "G#4",  0b1111 },
        { 82,  "A#4",  0b0111 },
        { 83,  "B4",   0b1100 },
        { 84,  "C5",   0b1111 },
        { 87,  "D#5",  0b1111 },
        { 90,  "F#5",  0b1111 },
        { 93,  "A5",   0b1111 },
        { 96,  "C6",   0b1111 },
        { 99,  "D#6",  0b1111 },
        { 102, "F#6",  0b1100 },
        { 105, "A6",   0b1111 },
        { 108, "C7",   0b1111 },
        { 103, "G6",   0b0011 },
    };

    // the release table stops well before the top of the keyboard on purpose. real grand pianos
    // have no dampers on the highest strings, so there is nothing to sample up there.
    const RootAvailability releaseRootTable[] =
    {
        { 22, "A#-1", 0b1110 },
        { 24, "C0",   0b1110 },
        { 27, "D#0",  0b1110 },
        { 30, "F#0",  0b1110 },
        { 33, "A0",   0b1110 },
        { 36, "C1",   0b1110 },
        { 39, "D#1",  0b1110 },
        { 42, "F#1",  0b1110 },
        { 45, "A1",   0b1110 },
        { 48, "C2",   0b1110 },
        { 50, "D2",   0b0110 },
        { 51, "D#2",  0b1100 },
        { 52, "E2",   0b0110 },
        { 54, "F#2",  0b1110 },
        { 56, "G#2",  0b0110 },
        { 57, "A2",   0b1000 },
        { 58, "A#2",  0b0110 },
        { 60, "C3",   0b1110 },
        { 62, "D3",   0b0110 },
        { 63, "D#3",  0b1000 },
        { 64, "E3",   0b0110 },
        { 66, "F#3",  0b1110 },
        { 68, "G#3",  0b0110 },
        { 69, "A3",   0b1000 },
        { 70, "A#3",  0b0110 },
        { 72, "C4",   0b1110 },
        { 74, "D4",   0b1110 },
        { 76, "E4",   0b1110 },
        { 78, "F#4",  0b1110 },
        { 80, "G#4",  0b1110 },
        { 82, "A#4",  0b0010 },
        { 83, "B4",   0b1100 },
        { 84, "C5",   0b1110 },
        { 87, "D#5",  0b1110 },
    };

    // this function returns the folder the installer puts the samples in. it uses the machine
    // wide shared data folder rather than the current user's own folder, since the installer runs
    // with admin rights and installs for every user account. on Windows this is
    // C:\ProgramData\JuicoliciousGrand\Samples. on Mac JUCE's shared folder is /Library itself,
    // so Application Support is added to match where Mac apps keep their data, giving
    // /Library/Application Support/JuicoliciousGrand/Samples.
    juce::File getSampleFolder(const juce::String& subFolderName)
    {
        juce::File base = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);

#if JUCE_MAC
        base = base.getChildFile("Application Support");
#endif

        return base.getChildFile("JuicoliciousGrand")
            .getChildFile("Samples")
            .getChildFile(subFolderName);
    }

    // this constant sets how long fade ins run for sustain notes at the lowest Attack setting.
    // it is just long enough to prevent a click when a sample starts.
    constexpr double crossfadeSeconds = 0.0015;

    // this is the level, about 60 decibels below full volume, at which a releasing voice is
    // treated as silent and switched off. the release knob sets how long it takes to get here.
    constexpr float releaseSilenceLevel = 0.001f;

    // when a key is struck again while its previous note is still sounding, the previous note
    // dies away over this many seconds instead of stopping instantly, so the two overlap briefly
    // the way a real re-struck string does.
    constexpr double retriggerFadeSeconds = 0.08;

    // this constant sets how long a sample fades to silence as it reaches the end of its own
    // recording, so it tapers off naturally instead of getting chopped the instant the file runs out.
    constexpr double naturalEndFadeSeconds = 0.015;

    // releases are a background sound on a real piano, not a competing note. this constant keeps
    // them from being loud enough to mask whatever gets played next.
    constexpr float releaseGainScale = 0.15f;

    // this softens the very start of a release sample so its damper thud blends in underneath the
    // decaying note instead of arriving as a sharp new attack.
    constexpr double releaseFadeInSeconds = 0.02;
}

// this function declares the two host automatable parameters this plugin exposes. Attack is how
// many seconds a note takes to fade in. Release is how many seconds a note takes to die away to
// silence after its key is lifted.
juce::AudioProcessorValueTreeState::ParameterLayout JuicoliciousGrandPianoProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack", 1), "Attack", juce::NormalisableRange<float>(0.0f, 2.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release", 1), "Release", juce::NormalisableRange<float>(0.05f, 3.0f), 0.2f));

    return { params.begin(), params.end() };
}

JuicoliciousGrandPianoProcessor::JuicoliciousGrandPianoProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    loadSampleFiles();
    loadReleaseFiles();

    attackParam = apvts.getRawParameterValue("attack");
    releaseParam = apvts.getRawParameterValue("release");
}

JuicoliciousGrandPianoProcessor::~JuicoliciousGrandPianoProcessor()
{
}

void JuicoliciousGrandPianoProcessor::loadSampleFiles()
{
    juce::File folder = getSampleFolder("Sustains");

    for (const auto& root : sustainRootTable)
    {
        for (int v = 1; v <= 4; ++v)
        {
            // this skips velocities that this root was not actually recorded at.
            if ((root.velocityMask & (1 << (v - 1))) == 0)
                continue;

            juce::String fileName = "GPiano_sus_" + juce::String(root.label) + "_v" + juce::String(v) + "_rr1_Player.flac";
            juce::File file = folder.getChildFile(fileName);
            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

            if (reader != nullptr)
            {
                SampleFile sf;
                sf.midiNote = root.midiNote;
                sf.velocity = v;
                sf.buffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
                reader->read(&sf.buffer, 0, (int)reader->lengthInSamples, 0, true, true);
                sampleFiles.push_back(std::move(sf));
            }
        }
    }
}

void JuicoliciousGrandPianoProcessor::loadReleaseFiles()
{
    juce::File folder = getSampleFolder("Releases");

    for (const auto& root : releaseRootTable)
    {
        for (int v = 1; v <= 4; ++v)
        {
            if ((root.velocityMask & (1 << (v - 1))) == 0)
                continue;

            juce::String fileName = "GPiano_rel_" + juce::String(root.label) + "_v" + juce::String(v) + "_rr1_Player.flac";
            juce::File file = folder.getChildFile(fileName);
            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

            if (reader != nullptr)
            {
                SampleFile sf;
                sf.midiNote = root.midiNote;
                sf.velocity = v;
                sf.buffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
                reader->read(&sf.buffer, 0, (int)reader->lengthInSamples, 0, true, true);
                releaseFiles.push_back(std::move(sf));
                highestReleaseNote = std::max(highestReleaseNote, root.midiNote);
            }
        }
    }
}

// perceived loudness is roughly logarithmic, not linear, so the curve is built in decibels. this
// keeps equal velocity steps feeling like equal loudness steps across the whole range, instead of
// small changes near the soft end sounding disproportionately dramatic.
float JuicoliciousGrandPianoProcessor::velocityToGain(int midiVelocity) const
{
    float normalized = (float)midiVelocity / 127.0f;
    float minDb = -8.0f;
    float decibels = minDb * (1.0f - normalized);
    return juce::Decibels::decibelsToGain(decibels);
}

// this function finds the nearest root note first, then treats the four velocity layers as a
// continuous line from 1 to 4 instead of four hard buckets. this is what lets two nearby
// velocities blend smoothly instead of snapping between two different recordings.
JuicoliciousGrandPianoProcessor::LayerMatch JuicoliciousGrandPianoProcessor::findLayerMatch(const std::vector<SampleFile>& files, int targetNote, int midiVelocity) const
{
    LayerMatch result;

    if (files.empty())
        return result;

    int bestNoteDistance = std::numeric_limits<int>::max();

    for (const auto& f : files)
        bestNoteDistance = std::min(bestNoteDistance, std::abs(f.midiNote - targetNote));

    float desiredLayer = 1.0f + ((float)midiVelocity / 127.0f) * 3.0f;

    int belowIndex = -1;
    int aboveIndex = -1;

    // belowLayer and aboveLayer start at sentinel values outside the real 1 to 4 range, so the
    // first real candidate found always replaces them.
    float belowLayer = -1.0f;
    float aboveLayer = 100.0f;

    for (int i = 0; i < (int)files.size(); ++i)
    {
        if (std::abs(files[i].midiNote - targetNote) != bestNoteDistance)
            continue;

        float layer = (float)files[i].velocity;

        if (layer <= desiredLayer && layer > belowLayer)
        {
            belowLayer = layer;
            belowIndex = i;
        }

        if (layer >= desiredLayer && layer < aboveLayer)
        {
            aboveLayer = layer;
            aboveIndex = i;
        }
    }

    // only one side is found when the velocity is beyond the recorded range, or this root only
    // has one layer available. if both sides land on the same file, no blend is needed either.
    if (belowIndex < 0)
    {
        result.indexA = aboveIndex;
        return result;
    }

    if (aboveIndex < 0 || aboveIndex == belowIndex)
    {
        result.indexA = belowIndex;
        return result;
    }

    result.indexA = belowIndex;
    result.indexB = aboveIndex;

    float range = aboveLayer - belowLayer;
    result.blendToB = (range > 0.0f) ? (desiredLayer - belowLayer) / range : 0.0f;

    return result;
}

// this function looks for the voice currently sounding this key. voices that have already
// started releasing are skipped, since they no longer belong to the key.
JuicoliciousGrandPianoProcessor::Voice* JuicoliciousGrandPianoProcessor::findVoiceForNote(int midiNote)
{
    for (auto& voice : voices)
        if (voice.active && !voice.releasing && voice.midiNote == midiNote)
            return &voice;

    return nullptr;
}

// this function picks a voice for a new sound. it prefers a silent voice, then the quietest
// voice that is already dying away, and only takes over a fully sounding note as a last resort,
// choosing the oldest one since it has decayed the most.
JuicoliciousGrandPianoProcessor::Voice* JuicoliciousGrandPianoProcessor::findFreeVoice(const Voice* avoid)
{
    for (auto& voice : voices)
        if (!voice.active && &voice != avoid)
            return &voice;

    Voice* quietestReleasing = nullptr;

    for (auto& voice : voices)
    {
        if (&voice == avoid || !voice.releasing)
            continue;

        if (quietestReleasing == nullptr || voice.releaseEnvelope < quietestReleasing->releaseEnvelope)
            quietestReleasing = &voice;
    }

    if (quietestReleasing != nullptr)
        return quietestReleasing;

    Voice* oldest = nullptr;

    for (auto& voice : voices)
    {
        if (&voice == avoid)
            continue;

        if (oldest == nullptr || voice.startOrder < oldest->startOrder)
            oldest = &voice;
    }

    return oldest;
}

// this function makes a voice die away to silence over the given time. the multiplier is the
// amount the volume shrinks by on each sample so that it reaches the silence level exactly when
// the time runs out, which gives the natural curved decay of a damped string rather than a
// straight line. it carries on from the voice's current level, so starting a second release on
// a voice that is already releasing never makes it jump louder.
void JuicoliciousGrandPianoProcessor::startReleaseEnvelope(Voice& voice, double seconds)
{
    double samples = juce::jmax(1.0, seconds * currentSampleRate);
    voice.releaseMultiplier = (float)std::pow((double)releaseSilenceLevel, 1.0 / samples);

    if (!voice.releasing)
    {
        voice.releasing = true;
        voice.releaseEnvelope = 1.0f;
    }

    voice.midiNote = -1;
}

// this function sets up a voice to start streaming from one or two velocity layer sources,
// pitch shifted to the requested note. the actual resampling happens gradually in processBlock
// rather than all at once here.
void JuicoliciousGrandPianoProcessor::playLayersIntoVoice(const SampleFile& fileA, const SampleFile* fileB, float blendToB, int midiNote, Voice& voice, double fadeInSeconds)
{
    int semitoneShift = midiNote - fileA.midiNote;
    voice.playbackRatio = std::pow(2.0, semitoneShift / 12.0);

    voice.layerA.source = &fileA;
    voice.layerA.sourceSamplesConsumed = 0;
    voice.layerA.maxSourceSamples = -1;
    voice.layerA.interpolators[0].reset();
    voice.layerA.interpolators[1].reset();

    voice.layerB.source = fileB;
    voice.layerB.sourceSamplesConsumed = 0;
    voice.layerB.maxSourceSamples = -1;
    voice.layerB.interpolators[0].reset();
    voice.layerB.interpolators[1].reset();

    voice.blendToB = blendToB;

    voice.fadeInTotalSamples = juce::jmax(1, (int)(fadeInSeconds * currentSampleRate));
    voice.fadeInSamplesRemaining = voice.fadeInTotalSamples;

    voice.releasing = false;
    voice.releaseEnvelope = 1.0f;
    voice.releaseMultiplier = 1.0f;

    voice.startOrder = ++voiceStartCounter;
    voice.active = true;
}

void JuicoliciousGrandPianoProcessor::triggerNote(int midiNote, int midiVelocity)
{
    lastMidiNoteReceived = midiNote;
    lastVelocityReceived = midiVelocity;
    keyPhysicallyDown[(size_t)midiNote] = true;
    pedalHeldNote[(size_t)midiNote] = false;

    LayerMatch match = findLayerMatch(sampleFiles, midiNote, midiVelocity);

    if (match.indexA < 0)
        return;

    // if this key is still sounding from an earlier strike, that earlier note is left to die
    // away quickly in its own voice while the new strike starts in a separate one, so the old
    // note is never cut off or restarted.
    Voice* previous = findVoiceForNote(midiNote);

    if (previous != nullptr)
        startReleaseEnvelope(*previous, retriggerFadeSeconds);

    Voice* voice = findFreeVoice(previous);

    if (voice == nullptr)
        return;

    voice->midiNote = midiNote;
    voice->triggerVelocity = midiVelocity;
    voice->gain = velocityToGain(midiVelocity);

    // this fade in length follows the Attack knob, but never drops below the short minimum that
    // prevents a click when the sample starts.
    double fadeInSeconds = juce::jmax((double)attackParam->load(), crossfadeSeconds);

    const SampleFile* fileB = (match.indexB >= 0) ? &sampleFiles[(size_t)match.indexB] : nullptr;
    playLayersIntoVoice(sampleFiles[(size_t)match.indexA], fileB, match.blendToB, midiNote, *voice, fadeInSeconds);
}

// a key going up does not necessarily mean the note stops. if the pedal is down, the note just
// gets marked as waiting for the pedal to come up instead.
void JuicoliciousGrandPianoProcessor::handleNoteOff(int midiNote)
{
    keyPhysicallyDown[(size_t)midiNote] = false;

    if (sustainPedalDown)
        pedalHeldNote[(size_t)midiNote] = true;
    else
        triggerRelease(midiNote);
}

// when the pedal comes up, every note that was only still ringing because of the pedal gets
// released now.
void JuicoliciousGrandPianoProcessor::handleSustainPedal(bool pedalDown)
{
    sustainPedalDown = pedalDown;

    if (pedalDown)
        return;

    for (int note = 0; note < 128; ++note)
    {
        if (pedalHeldNote[(size_t)note])
        {
            pedalHeldNote[(size_t)note] = false;
            triggerRelease(note);
        }
    }
}

void JuicoliciousGrandPianoProcessor::triggerRelease(int midiNote)
{
    // if this key is not actually sounding in any voice, there is nothing to release.
    Voice* voice = findVoiceForNote(midiNote);

    if (voice == nullptr)
        return;

    // notes above the highest release recording have no damper on a real piano, so they are
    // let go from the key but left to ring out on their own with no decay applied. this only
    // applies when release recordings were actually loaded.
    if (!releaseFiles.empty() && midiNote > highestReleaseNote)
    {
        voice->midiNote = -1;
        return;
    }

    int triggerVelocity = voice->triggerVelocity;
    float sustainGain = voice->gain;

    // the note keeps playing from exactly where it is and dies away over the Release time.
    startReleaseEnvelope(*voice, (double)releaseParam->load());

    if (releaseFiles.empty())
        return;

    // this uses the velocity the key was originally struck with, so a hard struck note also gets
    // a stronger damper sound.
    LayerMatch match = findLayerMatch(releaseFiles, midiNote, triggerVelocity);

    if (match.indexA < 0)
        return;

    // a release is a short percussive sound, so blending two separate recordings of it tends to
    // sound hollow. only the single closest velocity layer is used.
    int chosenIndex = match.indexA;

    if (match.indexB >= 0 && match.blendToB > 0.5f)
        chosenIndex = match.indexB;

    // the damper sound plays in its own voice alongside the decaying note, pitch shifted to the
    // key that was released so its leftover string ring matches the note that just stopped.
    Voice* releaseVoice = findFreeVoice(voice);

    if (releaseVoice == nullptr)
        return;

    releaseVoice->midiNote = -1;
    releaseVoice->triggerVelocity = triggerVelocity;
    releaseVoice->gain = sustainGain * releaseGainScale;

    playLayersIntoVoice(releaseFiles[(size_t)chosenIndex], nullptr, 0.0f, midiNote, *releaseVoice, releaseFadeInSeconds);
}

void JuicoliciousGrandPianoProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    scratchBuffer.setSize(2, samplesPerBlock);
    scratchBufferB.setSize(2, samplesPerBlock);

    for (auto& voice : voices)
    {
        voice.active = false;
        voice.releasing = false;
        voice.releaseEnvelope = 1.0f;
        voice.releaseMultiplier = 1.0f;
        voice.layerA.sourceSamplesConsumed = 0;
        voice.layerB.sourceSamplesConsumed = 0;
    }
}

void JuicoliciousGrandPianoProcessor::releaseResources()
{
}

int JuicoliciousGrandPianoProcessor::getNumActiveVoices() const
{
    int count = 0;

    for (const auto& voice : voices)
        if (voice.active)
            ++count;

    return count;
}

// this function streams and resamples one velocity layer into destination, applying its own
// natural end taper. it always fills the full requested length, padding with silence once the
// source runs out, so the caller can treat destination as fully valid without checking how much
// was real audio.
int JuicoliciousGrandPianoProcessor::processLayerStream(Voice::LayerStream& stream, double playbackRatio, int samplesRequested, juce::AudioBuffer<float>& destination)
{
    if (stream.source == nullptr)
    {
        destination.clear(0, samplesRequested);
        return 0;
    }

    int sourceLength = stream.source->buffer.getNumSamples();

    if (stream.maxSourceSamples >= 0)
        sourceLength = juce::jmin(sourceLength, stream.maxSourceSamples);

    int availableInputSamples = sourceLength - stream.sourceSamplesConsumed;

    if (availableInputSamples <= 4)
    {
        destination.clear(0, samplesRequested);
        return 0;
    }

    // this subtracts a small safety margin from the available input, since the interpolator
    // needs a few extra samples of lookahead beyond the exact requested amount.
    int maxSafeOutput = (int)((double)(availableInputSamples - 4) / playbackRatio);
    int samplesToProduce = juce::jmin(samplesRequested, maxSafeOutput);

    if (samplesToProduce <= 0)
    {
        destination.clear(0, samplesRequested);
        return 0;
    }

    // the streaming safe overload reports exactly how many input samples it actually used, so
    // the read position stays perfectly in sync with the interpolator's own internal state
    // instead of drifting from an estimate.
    int inputSamplesUsed = 0;

    for (int channel = 0; channel < 2; ++channel)
    {
        int sourceChannel = juce::jmin(channel, stream.source->buffer.getNumChannels() - 1);
        const float* input = stream.source->buffer.getReadPointer(sourceChannel) + stream.sourceSamplesConsumed;

        inputSamplesUsed = stream.interpolators[channel].process(playbackRatio, input, destination.getWritePointer(channel), samplesToProduce, availableInputSamples, 0);
    }

    // this layer fades itself out as it nears the end of its own recording, so it tapers off
    // naturally instead of getting chopped the instant the file runs out of audio.
    int naturalEndFadeSamples = (int)(naturalEndFadeSeconds * currentSampleRate);

    if (availableInputSamples <= naturalEndFadeSamples)
    {
        float startGain = juce::jlimit(0.0f, 1.0f, (float)availableInputSamples / (float)naturalEndFadeSamples);
        float endGain = juce::jlimit(0.0f, 1.0f, (float)(availableInputSamples - inputSamplesUsed) / (float)naturalEndFadeSamples);

        for (int channel = 0; channel < 2; ++channel)
            destination.applyGainRamp(channel, 0, samplesToProduce, startGain, endGain);
    }

    stream.sourceSamplesConsumed += inputSamplesUsed;

    if (samplesToProduce < samplesRequested)
        destination.clear(samplesToProduce, samplesRequested - samplesToProduce);

    return samplesToProduce;
}

void JuicoliciousGrandPianoProcessor::addKeyboardNoteOn(int midiNote, int velocity)
{
    const juce::ScopedLock lock(keyboardMidiLock);
    keyboardMidiBuffer.addEvent(juce::MidiMessage::noteOn(1, midiNote, (juce::uint8)velocity), 0);
}

void JuicoliciousGrandPianoProcessor::addKeyboardNoteOff(int midiNote)
{
    const juce::ScopedLock lock(keyboardMidiLock);
    keyboardMidiBuffer.addEvent(juce::MidiMessage::noteOff(1, midiNote), 0);
}

// this function runs continuously, once per block of audio data, and is where all of the actual
// audio processing happens.
void JuicoliciousGrandPianoProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    // any notes queued up by the computer keyboard get merged in alongside real MIDI here.
    {
        const juce::ScopedLock lock(keyboardMidiLock);

        if (!keyboardMidiBuffer.isEmpty())
        {
            for (const auto metadata : keyboardMidiBuffer)
                midiMessages.addEvent(metadata.getMessage(), metadata.samplePosition);

            keyboardMidiBuffer.clear();
        }
    }

    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            triggerNote(message.getNoteNumber(), message.getVelocity());
        }
        else if (message.isNoteOff())
        {
            handleNoteOff(message.getNoteNumber());
        }
        else if (message.isController() && message.getControllerNumber() == 64)
        {
            handleSustainPedal(message.getControllerValue() >= 64);
        }
    }

    // every active voice resamples only enough of its source to fill this one block, then adds
    // into the shared output. spreading the resampling work evenly instead of doing it all at
    // the instant a note is pressed is what keeps this from overloading the audio thread.
    for (auto& voice : voices)
    {
        if (!voice.active)
            continue;

        if (voice.layerA.source == nullptr && voice.layerB.source == nullptr)
        {
            voice.active = false;
            continue;
        }

        int samplesToProduce = buffer.getNumSamples();

        int producedA = processLayerStream(voice.layerA, voice.playbackRatio, samplesToProduce, scratchBuffer);
        int producedB = (voice.layerB.source != nullptr)
            ? processLayerStream(voice.layerB, voice.playbackRatio, samplesToProduce, scratchBufferB)
            : 0;

        if (producedA <= 0 && producedB <= 0)
        {
            voice.active = false;
            continue;
        }

        // this blends the two velocity layers together by how close the actual velocity was to
        // each one, which turns a hard jump between two different recordings into a smooth blend.
        if (voice.layerB.source != nullptr)
        {
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                float* dataA = scratchBuffer.getWritePointer(channel);
                const float* dataB = scratchBufferB.getReadPointer(channel);

                for (int i = 0; i < samplesToProduce; ++i)
                    dataA[i] = dataA[i] * (1.0f - voice.blendToB) + dataB[i] * voice.blendToB;
            }
        }

        // this fades the start of a sound in along a sine curve, which rises smoothly from
        // silence without the sharp corner a straight line ramp has at its start.
        if (voice.fadeInSamplesRemaining > 0)
        {
            int fadeSamplesThisBlock = juce::jmin(samplesToProduce, voice.fadeInSamplesRemaining);
            int fadeSamplesSoFar = voice.fadeInTotalSamples - voice.fadeInSamplesRemaining;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                float* data = scratchBuffer.getWritePointer(channel);

                for (int i = 0; i < fadeSamplesThisBlock; ++i)
                {
                    float progress = (float)(fadeSamplesSoFar + i) / (float)voice.fadeInTotalSamples;
                    data[i] *= std::sin(progress * juce::MathConstants<float>::halfPi);
                }
            }

            voice.fadeInSamplesRemaining -= fadeSamplesThisBlock;
        }

        // a releasing voice keeps playing its sample exactly where it is, while its volume
        // shrinks a little on every sample. every channel starts from the same envelope value so
        // left and right decay identically, and the value reached at the end is stored for the
        // next block. once it drops below the silence level the voice is switched off.
        if (voice.releasing)
        {
            float envelopeAtBlockStart = voice.releaseEnvelope;
            float envelope = envelopeAtBlockStart;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                float* data = scratchBuffer.getWritePointer(channel);
                envelope = envelopeAtBlockStart;

                for (int i = 0; i < samplesToProduce; ++i)
                {
                    data[i] *= envelope;
                    envelope *= voice.releaseMultiplier;
                }
            }

            voice.releaseEnvelope = envelope;

            if (voice.releaseEnvelope <= releaseSilenceLevel)
                voice.active = false;
        }

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.addFrom(channel, 0, scratchBuffer, channel, 0, samplesToProduce, voice.gain);
    }
}

juce::AudioProcessorEditor* JuicoliciousGrandPianoProcessor::createEditor()
{
    return new JuicoliciousGrandPianoEditor(*this);
}

bool JuicoliciousGrandPianoProcessor::hasEditor() const
{
    return true;
}

const juce::String JuicoliciousGrandPianoProcessor::getName() const
{
    return JucePlugin_Name;
}

bool JuicoliciousGrandPianoProcessor::acceptsMidi() const
{
    return true;
}

bool JuicoliciousGrandPianoProcessor::producesMidi() const
{
    return false;
}

double JuicoliciousGrandPianoProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int JuicoliciousGrandPianoProcessor::getNumPrograms()
{
    return 1;
}

int JuicoliciousGrandPianoProcessor::getCurrentProgram()
{
    return 0;
}

void JuicoliciousGrandPianoProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String JuicoliciousGrandPianoProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void JuicoliciousGrandPianoProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

// this function saves the current knob positions into the host's project file.
void JuicoliciousGrandPianoProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

// this function restores the knob positions from the host's project file.
void JuicoliciousGrandPianoProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JuicoliciousGrandPianoProcessor();
}