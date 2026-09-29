#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>

class JuicoliciousGrandPianoProcessor : public juce::AudioProcessor
{
public:
    JuicoliciousGrandPianoProcessor();
    ~JuicoliciousGrandPianoProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    int getNumSampleFilesLoaded() const { return (int)sampleFiles.size(); }
    int getNumReleaseFilesLoaded() const { return (int)releaseFiles.size(); }
    int getLastMidiNoteReceived() const { return lastMidiNoteReceived; }
    int getLastVelocityReceived() const { return lastVelocityReceived; }
    int getNumActiveVoices() const;

    // this function reports whether the given note is currently held down.
    bool isKeyDown(int midiNote) const
    {
        return midiNote >= 0 && midiNote < 128 && keyPhysicallyDown[(size_t)midiNote];
    }

    // this function is called from the editor on the message thread when a computer keyboard key
    // goes down or up. the note is queued here instead of triggering the voice engine directly,
    // because the editor and the audio thread are different threads and cannot safely touch the
    // same voice state at once.
    void addKeyboardNoteOn(int midiNote, int velocity);
    void addKeyboardNoteOff(int midiNote);

    // this member is public so the editor's sliders can attach directly to it.
    juce::AudioProcessorValueTreeState apvts;

private:
    // one specific recorded file, holding the audio for one root note at one velocity layer.
    struct SampleFile
    {
        int midiNote;
        int velocity;
        juce::AudioBuffer<float> buffer;
    };

    // the result of a velocity lookup. indexA is the nearest available layer at or below the
    // target velocity and indexB is the nearest available layer at or above it. blendToB stores
    // how far between the two layers the real velocity sits. indexB stays -1 when only one layer
    // is available, so no blending happens.
    struct LayerMatch
    {
        int indexA = -1;
        int indexB = -1;
        float blendToB = 0.0f;
    };

    // one of these exists per simultaneously sounding note. this is what makes polyphony work.
    struct Voice
    {
        // everything needed to stream and resample one recorded file. a voice owns two of these
        // at once, one per velocity layer being blended together.
        struct LayerStream
        {
            const SampleFile* source = nullptr;
            int sourceSamplesConsumed = 0;
            int maxSourceSamples = -1;
            std::array<juce::LagrangeInterpolator, 2> interpolators;
        };

        int midiNote = -1;
        int triggerVelocity = 0;
        LayerStream layerA;
        LayerStream layerB;
        float blendToB = 0.0f;
        double playbackRatio = 1.0;
        float gain = 1.0f;
        int fadeInSamplesRemaining = 0;
        int fadeInTotalSamples = 0;

        // when releasing is true, the voice keeps playing from exactly where it is while
        // releaseEnvelope shrinks by releaseMultiplier on every sample. the envelope is stored
        // as a running value rather than recomputed from a start point, so a new release can
        // take over from the current level without any jump in volume.
        bool releasing = false;
        float releaseEnvelope = 1.0f;
        float releaseMultiplier = 1.0f;

        // this counter value records when the voice was started, so the oldest voice can be
        // found when every voice is busy and one has to be taken over.
        juce::uint64 startOrder = 0;

        bool active = false;
    };

    static constexpr int maxVoices = 32;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void loadSampleFiles();
    void loadReleaseFiles();

    // this function finds the nearest root note first, then the two nearest available velocity
    // layers at that root that bracket the actual velocity, so playback can blend between them.
    LayerMatch findLayerMatch(const std::vector<SampleFile>& files, int targetNote, int midiVelocity) const;
    float velocityToGain(int midiVelocity) const;

    // this function streams and resamples one layer into destination, applying its own natural
    // end taper. it returns how many samples were actually real audio, which is 0 once this
    // layer has run out.
    int processLayerStream(Voice::LayerStream& stream, double playbackRatio, int samplesRequested, juce::AudioBuffer<float>& destination);

    Voice* findVoiceForNote(int midiNote);

    // this function returns a voice to start a new sound in. avoid is a voice that must not be
    // returned, used when a voice has just started releasing and a second one is needed for
    // the release sample.
    Voice* findFreeVoice(const Voice* avoid = nullptr);

    // this function starts or retargets a voice's decay to silence over the given number of
    // seconds, continuing smoothly from whatever level the voice is currently at.
    void startReleaseEnvelope(Voice& voice, double seconds);

    void playLayersIntoVoice(const SampleFile& fileA, const SampleFile* fileB, float blendToB, int midiNote, Voice& voice, double fadeInSeconds);

    void triggerNote(int midiNote, int midiVelocity);
    void triggerRelease(int midiNote);
    void handleNoteOff(int midiNote);
    void handleSustainPedal(bool pedalDown);

    juce::AudioFormatManager formatManager;
    std::vector<SampleFile> sampleFiles;
    std::vector<SampleFile> releaseFiles;
    int highestReleaseNote = -1;
    double currentSampleRate = 44100.0;
    std::array<Voice, maxVoices> voices;
    juce::AudioBuffer<float> scratchBuffer;
    juce::AudioBuffer<float> scratchBufferB;
    bool sustainPedalDown = false;
    std::array<bool, 128> keyPhysicallyDown{};
    std::array<bool, 128> pedalHeldNote{};
    int lastMidiNoteReceived = -1;
    int lastVelocityReceived = -1;

    // this counter increases every time a voice starts, giving each voice a start order value.
    juce::uint64 voiceStartCounter = 0;

    // these are raw pointers into apvts, read directly on the audio thread. this is the standard,
    // lock free way to read a live parameter value from inside processBlock.
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;

    // this lock is only ever held for a quick copy and clear. contention is rare, since it only
    // happens when a human is typing, not on every sample of audio work.
    juce::CriticalSection keyboardMidiLock;
    juce::MidiBuffer keyboardMidiBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JuicoliciousGrandPianoProcessor)
};