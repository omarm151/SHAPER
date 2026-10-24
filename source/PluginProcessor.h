#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Curve.h"
#include "dsp/ShaperEngine.h"

#include <array>
#include <atomic>
#include <deque>
#include <random>

namespace illuminize
{

// Single-producer / single-consumer triple buffer: the message thread publishes curve
// snapshots, the audio thread picks up the newest one without locks or allocation.
template <typename T>
class TripleBuffer
{
public:
    void write (const T& value) noexcept
    {
        buffers[(size_t) back] = value;
        back = state.exchange (back | dirtyBit) & indexMask;
    }
    bool read (T& out) noexcept
    {
        if ((state.load() & dirtyBit) == 0)
            return false;
        front = state.exchange (front) & indexMask;
        out = buffers[(size_t) front];
        return true;
    }

private:
    static constexpr int dirtyBit = 4, indexMask = 3;
    std::array<T, 3> buffers {};
    std::atomic<int> state { 1 };
    int front = 0, back = 2;
};

namespace ParamIDs
{
    inline constexpr const char* pre = "pre";
    inline constexpr const char* bias = "bias";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* post = "post";
    inline constexpr const char* dc = "dc";
    inline constexpr const char* oversampling = "oversampling";
    inline constexpr const char* autoGain = "autogain";
    inline constexpr const char* limiter = "limiter";
    inline constexpr const char* ceiling = "ceiling";
    inline constexpr const char* release = "release";
} // namespace ParamIDs

class ShaperProcessor : public juce::AudioProcessor,
                        public juce::ChangeBroadcaster
{
public:
    ShaperProcessor();
    ~ShaperProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Illuminize Shaper"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Curve editing (message thread). Changes are published to the audio thread.
    CurveModel getCurve() const;
    void setCurve (const CurveModel& model, bool addUndoStep);
    void beginCurveEdit();                 // records an undo step (call once per gesture)
    void applyPreset (int preset);
    void randomCurve();
    void setSymmetric (bool sym);
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void undo();
    void redo();
    void resetAll();

    // Display settings stored with the session (not automatable).
    int getTheme() const;
    void setTheme (int theme);
    bool getMotion() const;
    void setMotion (bool on);

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return parameters; }
    ShaperEngine& getEngine() noexcept { return engine; }

    static constexpr int maxUndo = 40;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void publishCurve();

    juce::AudioProcessorValueTreeState parameters;
    ShaperEngine engine;

    mutable juce::CriticalSection curveLock;
    CurveModel curve;
    std::deque<std::string> undoStack, redoStack;
    std::mt19937 rng { std::random_device {}() };
    TripleBuffer<CurveSnapshot> curveMailbox;

    // Audio-thread state
    CurveSnapshot pendingCurve, currentCurve;
    bool hasPendingCurve = false, autoGainDirty = true;
    double lastPreDb = 1e9, lastBiasPercent = 1e9, autoGainValue = 1.0;

    std::atomic<float>* pPre = nullptr;
    std::atomic<float>* pBias = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pPost = nullptr;
    std::atomic<float>* pDc = nullptr;
    std::atomic<float>* pOversampling = nullptr;
    std::atomic<float>* pAutoGain = nullptr;
    std::atomic<float>* pLimiter = nullptr;
    std::atomic<float>* pCeiling = nullptr;
    std::atomic<float>* pRelease = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShaperProcessor)
};

} // namespace illuminize
