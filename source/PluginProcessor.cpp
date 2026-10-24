#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace illuminize
{

namespace
{
    juce::String dbText (float v, bool showPlus)
    {
        if (std::abs (v) < 0.05f)
            v = 0.0f;
        return (showPlus && v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
    }

    juce::NormalisableRange<float> centredRange (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi, 0.01f);
        r.setSkewForCentre (centre);
        return r;
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout ShaperProcessor::createLayout()
{
    using namespace juce;
    using FloatAttr = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dbAttr = [] (bool plus)
    {
        return FloatAttr().withLabel ("dB").withStringFromValueFunction ([plus] (float v, int) { return dbText (v, plus); });
    };
    auto pctAttr = FloatAttr().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::pre, 1 }, "Pre",
                                                        NormalisableRange<float> (-24.0f, 36.0f, 0.01f), 0.0f, dbAttr (true)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::bias, 1 }, "Bias",
                                                        NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",
                                                        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::post, 1 }, "Post",
                                                        NormalisableRange<float> (-36.0f, 6.0f, 0.01f), 0.0f, dbAttr (true)));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::dc, 1 }, "DC Filter", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::oversampling, 1 }, "Oversampling",
                                                         StringArray { "1x", "2x", "4x" }, 1));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::autoGain, 1 }, "Auto Gain", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::limiter, 1 }, "Limiter", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::ceiling, 1 }, "Limiter Ceiling",
                                                        NormalisableRange<float> (-12.0f, 0.0f, 0.01f), -0.3f, dbAttr (false)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::release, 1 }, "Limiter Release",
                                                        centredRange (10.0f, 500.0f, 80.0f), 80.0f,
                                                        FloatAttr().withLabel ("ms").withStringFromValueFunction ([] (float v, int)
                                                                                                                  { return String (roundToInt (v)) + " ms"; })));
    return { p.begin(), p.end() };
}

ShaperProcessor::ShaperProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "IlluminizeShaper", createLayout())
{
    pPre = parameters.getRawParameterValue (ParamIDs::pre);
    pBias = parameters.getRawParameterValue (ParamIDs::bias);
    pMix = parameters.getRawParameterValue (ParamIDs::mix);
    pPost = parameters.getRawParameterValue (ParamIDs::post);
    pDc = parameters.getRawParameterValue (ParamIDs::dc);
    pOversampling = parameters.getRawParameterValue (ParamIDs::oversampling);
    pAutoGain = parameters.getRawParameterValue (ParamIDs::autoGain);
    pLimiter = parameters.getRawParameterValue (ParamIDs::limiter);
    pCeiling = parameters.getRawParameterValue (ParamIDs::ceiling);
    pRelease = parameters.getRawParameterValue (ParamIDs::release);

    parameters.state.setProperty ("theme", 0, nullptr);
    parameters.state.setProperty ("motion", true, nullptr);

    setLatencySamples (ShaperEngine::latencySamples);
    publishCurve();
}

bool ShaperProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

static EngineParams readParams (std::atomic<float>* pre, std::atomic<float>* bias, std::atomic<float>* mix,
                                std::atomic<float>* post, std::atomic<float>* dc, std::atomic<float>* os,
                                std::atomic<float>* lim, std::atomic<float>* ceil, std::atomic<float>* rel)
{
    EngineParams p;
    p.preDb = pre->load();
    p.biasPercent = bias->load();
    p.mixPercent = mix->load();
    p.postDb = post->load();
    p.dcFilter = dc->load() > 0.5f;
    p.oversampling = juce::jlimit (0, 2, (int) std::lround (os->load()));
    p.limiter = lim->load() > 0.5f;
    p.ceilingDb = ceil->load();
    p.releaseMs = rel->load();
    return p;
}

void ShaperProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    if (curveMailbox.read (pendingCurve))
        currentCurve = pendingCurve;
    hasPendingCurve = false;
    engine.setCurve (currentCurve);
    auto p = readParams (pPre, pBias, pMix, pPost, pDc, pOversampling, pLimiter, pCeiling, pRelease);
    autoGainDirty = true;
    lastPreDb = lastBiasPercent = 1e9;
    p.autoGain = pAutoGain->load() > 0.5f ? computeAutoGain (currentCurve, p.preDb, p.biasPercent) : 1.0;
    engine.setParams (p);
    engine.snapToTargets();
    setLatencySamples (ShaperEngine::latencySamples);
}

void ShaperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    // New curve from the editor? Start the crossfade once the previous one has finished.
    if (! hasPendingCurve && curveMailbox.read (pendingCurve))
        hasPendingCurve = true;
    if (hasPendingCurve && engine.curveFadeDone())
    {
        engine.setCurve (pendingCurve);
        currentCurve = pendingCurve;
        hasPendingCurve = false;
        autoGainDirty = true;
    }

    auto p = readParams (pPre, pBias, pMix, pPost, pDc, pOversampling, pLimiter, pCeiling, pRelease);
    if (pAutoGain->load() > 0.5f)
    {
        if (autoGainDirty || p.preDb != lastPreDb || p.biasPercent != lastBiasPercent)
        {
            autoGainValue = computeAutoGain (currentCurve, p.preDb, p.biasPercent);
            lastPreDb = p.preDb;
            lastBiasPercent = p.biasPercent;
            autoGainDirty = false;
        }
        p.autoGain = autoGainValue;
    }
    else
    {
        p.autoGain = 1.0;
        autoGainDirty = true;
    }
    engine.setParams (p);

    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;
    engine.process (left, right, buffer.getNumSamples());
}

// ---------------------------------------------------------------- curve editing

CurveModel ShaperProcessor::getCurve() const
{
    const juce::ScopedLock sl (curveLock);
    return curve;
}

void ShaperProcessor::publishCurve()
{
    {
        // Writers (editor, host state loading) are serialised by the lock; the audio
        // thread reads the mailbox without locking.
        const juce::ScopedLock sl (curveLock);
        curveMailbox.write (curve.makeSnapshot());
    }
    sendChangeMessage();
}

void ShaperProcessor::beginCurveEdit()
{
    {
        const juce::ScopedLock sl (curveLock);
        undoStack.push_back (curve.toString());
        if ((int) undoStack.size() > maxUndo)
            undoStack.pop_front();
        redoStack.clear();
    }
    sendChangeMessage();
}

void ShaperProcessor::setCurve (const CurveModel& model, bool addUndoStep)
{
    if (addUndoStep)
        beginCurveEdit();
    {
        const juce::ScopedLock sl (curveLock);
        curve = model;
    }
    publishCurve();
}

void ShaperProcessor::applyPreset (int preset)
{
    beginCurveEdit();
    {
        const juce::ScopedLock sl (curveLock);
        curve.applyPreset (preset);
    }
    publishCurve();
}

void ShaperProcessor::randomCurve()
{
    beginCurveEdit();
    {
        const juce::ScopedLock sl (curveLock);
        curve.randomize (rng);
    }
    publishCurve();
}

void ShaperProcessor::setSymmetric (bool sym)
{
    beginCurveEdit();
    {
        const juce::ScopedLock sl (curveLock);
        curve.setSymmetric (sym);
    }
    publishCurve();
}

void ShaperProcessor::undo()
{
    {
        const juce::ScopedLock sl (curveLock);
        if (undoStack.empty())
            return;
        redoStack.push_back (curve.toString());
        curve.fromString (undoStack.back());
        undoStack.pop_back();
    }
    publishCurve();
}

void ShaperProcessor::redo()
{
    {
        const juce::ScopedLock sl (curveLock);
        if (redoStack.empty())
            return;
        undoStack.push_back (curve.toString());
        curve.fromString (redoStack.back());
        redoStack.pop_back();
    }
    publishCurve();
}

void ShaperProcessor::resetAll()
{
    for (auto* param : AudioProcessor::getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
            ranged->endChangeGesture();
        }
    {
        const juce::ScopedLock sl (curveLock);
        curve.reset();
        undoStack.clear();
        redoStack.clear();
    }
    publishCurve();
}

int ShaperProcessor::getTheme() const { return (int) parameters.state.getProperty ("theme", 0); }
void ShaperProcessor::setTheme (int theme) { parameters.state.setProperty ("theme", theme, nullptr); sendChangeMessage(); }
bool ShaperProcessor::getMotion() const { return (bool) parameters.state.getProperty ("motion", true); }
void ShaperProcessor::setMotion (bool on) { parameters.state.setProperty ("motion", on, nullptr); }

// ---------------------------------------------------------------- state

void ShaperProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    {
        const juce::ScopedLock sl (curveLock);
        state.setProperty ("curve", juce::String (curve.toString()), nullptr);
    }
    state.setProperty ("version", juce::String (JucePlugin_VersionString), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ShaperProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    const auto curveString = tree.getProperty ("curve").toString().toStdString();
    parameters.replaceState (tree);
    {
        const juce::ScopedLock sl (curveLock);
        CurveModel loaded;
        if (loaded.fromString (curveString))
            curve = loaded;
        undoStack.clear();
        redoStack.clear();
    }
    publishCurve();
}

juce::AudioProcessorEditor* ShaperProcessor::createEditor()
{
    return new ShaperEditor (*this);
}

} // namespace illuminize

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new illuminize::ShaperProcessor();
}
