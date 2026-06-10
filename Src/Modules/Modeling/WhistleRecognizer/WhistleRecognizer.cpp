/**
 * @file WhistleRecognizer.cpp
 *
 * Goertzel Gate V5 - SabanaHerons 2026
 *
 * Architecture:
 *   SR=16000, N_MAIN=320 (20 ms), N_FAST=160 (10 ms), hop=80 (5 ms)
 *   Band=3600-4500 Hz
 *
 * Pipeline per hop:
 *   1. Decimate/resample to target SR using the actual input sample rate
 *   2. IIR bandpass (2500-6000 Hz) + lowpass (1500 Hz)
 *   3. Main Goertzel on N_MAIN BP-filtered samples
 *   4. Stage 1: reject if P_max < pMaxMin
 *   5. Stage 2: SNR/flatness (main + fast), energy ratios, flux
 *   6. Stationary-bin mask (persistent bins, +-1 spread)
 *   7. 3-state FSM per channel: IDLE -> CANDIDATE -> ACTIVE
 */

#include "WhistleRecognizer.h"
#include "Platform/SystemCall.h"
#include "Debugging/Annotation.h"
#include "Debugging/Plot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <sstream>

namespace
{
  float clamp01(const float value)
  {
    return std::max(0.0f, std::min(1.0f, value));
  }
}

MAKE_MODULE(WhistleRecognizer);

const double WhistleRecognizer::BP_B[WhistleRecognizer::BP_N_SOS][3] = {
  {6.178390703854285e-02, -1.235678140770857e-01, 6.178390703854285e-02},
  {1.000000000000000e+00, 2.000000000000000e+00, 1.000000000000000e+00},
  {1.000000000000000e+00, -2.000000000000000e+00, 1.000000000000000e+00},
  {1.000000000000000e+00, 2.000000000000000e+00, 1.000000000000000e+00},
};

const double WhistleRecognizer::BP_A[WhistleRecognizer::BP_N_SOS][2] = {
  {-3.513343112738304e-01, 1.988547544487332e-01},
  {6.308883599288199e-01, 2.476570630902564e-01},
  {-8.837494470589440e-01, 6.528451408471714e-01},
  {1.173320827100737e+00, 6.957180586149299e-01},
};

const double WhistleRecognizer::LP_B[WhistleRecognizer::LP_N_SOS][3] = {
  {3.869518318520656e-03, 7.739036637041312e-03, 3.869518318520656e-03},
  {1.000000000000000e+00, 2.000000000000000e+00, 1.000000000000000e+00},
};

const double WhistleRecognizer::LP_A[WhistleRecognizer::LP_N_SOS][2] = {
  {-1.098897269965641e+00, 3.216325091214370e-01},
  {-1.371374655125205e+00, 6.493382738637064e-01},
};

WhistleRecognizer::WhistleRecognizer() = default;
WhistleRecognizer::~WhistleRecognizer() = default;

float WhistleRecognizer::applyBP(float x, double (&z)[BP_N_SOS][2])
{
  double v = static_cast<double>(x);
  for(int s = 0; s < BP_N_SOS; ++s)
  {
    const double y = BP_B[s][0] * v + z[s][0];
    z[s][0] = BP_B[s][1] * v - BP_A[s][0] * y + z[s][1];
    z[s][1] = BP_B[s][2] * v - BP_A[s][1] * y;
    v = y;
  }
  return static_cast<float>(v);
}

float WhistleRecognizer::applyLP(float x, double (&z)[LP_N_SOS][2])
{
  double v = static_cast<double>(x);
  for(int s = 0; s < LP_N_SOS; ++s)
  {
    const double y = LP_B[s][0] * v + z[s][0];
    z[s][0] = LP_B[s][1] * v - LP_A[s][0] * y + z[s][1];
    z[s][1] = LP_B[s][2] * v - LP_A[s][1] * y;
    v = y;
  }
  return static_cast<float>(v);
}

void WhistleRecognizer::initChannelState(ChannelState& state, int nBins)
{
  if(static_cast<int>(state.stationaryCounter.size()) == nBins)
    return;

  state.prevPowers.assign(nBins, 0.0f);
  state.prevPowersValid = false;
  state.stationaryCounter.assign(nBins, 0);

  const int hopSamples = std::max(1, static_cast<int>(bufferSize * newSampleRatio));
  const float hopSec = static_cast<float>(hopSamples) / static_cast<float>(sampleRate);
  state.stationaryThreshold = std::max(1, static_cast<int>(stationaryHoldSec / hopSec));
}

WhistleRecognizer::FrameFeatures WhistleRecognizer::analyzeFrame(ChannelState& state)
{
  FrameFeatures feat;

  const int n = static_cast<int>(state.bpBuf.size());
  const int fastWindow = n / 2;
  const float fs = static_cast<float>(sampleRate);
  if(n < 2)
    return feat;

  std::vector<float> bpChrono(n);
  std::vector<float> lpChrono(n);
  for(int i = 0; i < n; ++i)
  {
    bpChrono[i] = state.bpBuf[n - 1 - i];
    lpChrono[i] = state.lpBuf[n - 1 - i];
  }

  float maxAmp = 0.0f;
  for(int i = 0; i < n; ++i)
    maxAmp = std::max(maxAmp, std::abs(bpChrono[i]));
  if(maxAmp < minVolume)
    return feat;

  const int kMin = static_cast<int>(std::ceil(goertzelMinFreq * n / fs));
  const int kMax = static_cast<int>(std::floor(goertzelMaxFreq * n / fs));
  const int nBins = kMax - kMin + 1;
  if(nBins <= 0)
    return feat;

  initChannelState(state, nBins);

  feat.powers.resize(nBins);
  for(int k = kMin; k <= kMax; ++k)
  {
    const int idx = k - kMin;
    const float w = 2.0f * static_cast<float>(M_PI) * static_cast<float>(k) / static_cast<float>(n);
    const float coeff = 2.0f * std::cos(w);
    float q1 = 0.0f;
    float q2 = 0.0f;
    for(int i = 0; i < n; ++i)
    {
      const float q0 = coeff * q1 - q2 + bpChrono[i];
      q2 = q1;
      q1 = q0;
    }
    feat.powers[idx] = q1 * q1 + q2 * q2 - q1 * q2 * coeff;
  }

  const auto maxIt = std::max_element(feat.powers.begin(), feat.powers.end());
  feat.peakIdx = static_cast<int>(maxIt - feat.powers.begin());
  feat.pMax = feat.powers[feat.peakIdx];
  feat.peakFreq = goertzelMinFreq + static_cast<float>(feat.peakIdx) * (fs / static_cast<float>(n));

  if(feat.pMax < pMaxMin)
  {
    state.prevPowers = feat.powers;
    state.prevPowersValid = true;
    for(int i = 0; i < nBins; ++i)
      state.stationaryCounter[i] = std::max(0, state.stationaryCounter[i] - 1);
    state.stationaryCounter[feat.peakIdx] += 2;
    return feat;
  }

  float sumRest = 0.0f;
  int cntRest = 0;
  for(int i = 0; i < nBins; ++i)
  {
    if(std::abs(i - feat.peakIdx) > 1)
    {
      sumRest += feat.powers[i];
      ++cntRest;
    }
  }
  const float avgRest = cntRest > 0 ? sumRest / static_cast<float>(cntRest) : feat.pMax * 0.1f;
  feat.snrDb = 10.0f * std::log10((feat.pMax + 1e-12f) / (avgRest + 1e-12f));

  float geoSum = 0.0f;
  float arithSum = 0.0f;
  for(float p : feat.powers)
  {
    geoSum += std::log(p + 1e-12f);
    arithSum += p + 1e-12f;
  }
  feat.flatness = std::exp(geoSum / static_cast<float>(nBins)) / (arithSum / static_cast<float>(nBins));

  if(fastWindow >= 2)
  {
    const int kMinFast = static_cast<int>(std::ceil(goertzelMinFreq * fastWindow / fs));
    const int kMaxFast = static_cast<int>(std::floor(goertzelMaxFreq * fastWindow / fs));
    const int nBinsFast = kMaxFast - kMinFast + 1;
    if(nBinsFast > 0)
    {
      std::vector<float> fastPowers(nBinsFast);
      for(int k = kMinFast; k <= kMaxFast; ++k)
      {
        const int idx = k - kMinFast;
        const float w = 2.0f * static_cast<float>(M_PI) * static_cast<float>(k) / static_cast<float>(fastWindow);
        const float coeff = 2.0f * std::cos(w);
        float q1 = 0.0f;
        float q2 = 0.0f;
        for(int i = 0; i < fastWindow; ++i)
        {
          const float q0 = coeff * q1 - q2 + bpChrono[n - fastWindow + i];
          q2 = q1;
          q1 = q0;
        }
        fastPowers[idx] = q1 * q1 + q2 * q2 - q1 * q2 * coeff;
      }

      const auto maxItFast = std::max_element(fastPowers.begin(), fastPowers.end());
      const int peakFast = static_cast<int>(maxItFast - fastPowers.begin());
      float sumRestFast = 0.0f;
      int cntRestFast = 0;
      for(int i = 0; i < nBinsFast; ++i)
      {
        if(std::abs(i - peakFast) > 1)
        {
          sumRestFast += fastPowers[i];
          ++cntRestFast;
        }
      }
      const float avgRestFast = cntRestFast > 0 ? sumRestFast / static_cast<float>(cntRestFast) : fastPowers[peakFast] * 0.1f;
      feat.snrFast = 10.0f * std::log10((fastPowers[peakFast] + 1e-12f) / (avgRestFast + 1e-12f));

      float geoSumFast = 0.0f;
      float arithSumFast = 0.0f;
      for(float p : fastPowers)
      {
        geoSumFast += std::log(p + 1e-12f);
        arithSumFast += p + 1e-12f;
      }
      feat.flatFast = std::exp(geoSumFast / static_cast<float>(nBinsFast)) / (arithSumFast / static_cast<float>(nBinsFast));
    }
  }

  double eWhistle = 1e-12;
  double eLow = 1e-12;
  double eTotal = 1e-12;
  for(int i = 0; i < n; ++i)
  {
    const double bp = bpChrono[i];
    const double lp = lpChrono[i];
    eWhistle += bp * bp;
    eLow += lp * lp;
    eTotal += (bp + lp) * (bp + lp);
  }
  feat.eRatio = static_cast<float>(eWhistle / (eTotal + 1e-12));
  feat.lowband = static_cast<float>(eLow / (eWhistle + 1e-12));

  if(state.prevPowersValid && static_cast<int>(state.prevPowers.size()) == nBins)
  {
    float sumPosDiff = 0.0f;
    float sumCur = 1e-12f;
    for(int i = 0; i < nBins; ++i)
    {
      sumPosDiff += std::max(0.0f, feat.powers[i] - state.prevPowers[i]);
      sumCur += feat.powers[i];
    }
    feat.fluxDb = 20.0f * std::log10(sumPosDiff / sumCur + 1.0f);
  }
  state.prevPowers = feat.powers;
  state.prevPowersValid = true;

  for(int i = 0; i < nBins; ++i)
    state.stationaryCounter[i] = std::max(0, state.stationaryCounter[i] - 1);
  state.stationaryCounter[feat.peakIdx] += 2;

  return feat;
}

WhistleRecognizer::GateEvaluation WhistleRecognizer::evaluateGates(const FrameFeatures& feat, ChannelState& state) const
{
  GateEvaluation gates;

  const int nBins = static_cast<int>(state.stationaryCounter.size());
  if(nBins > 0 && feat.peakIdx >= 0 && feat.peakIdx < nBins)
  {
    const int low = std::max(0, feat.peakIdx - 1);
    const int high = std::min(nBins - 1, feat.peakIdx + 1);
    for(int k = low; k <= high; ++k)
    {
      if(state.stationaryCounter[k] >= state.stationaryThreshold)
      {
        gates.stationary = true;
        break;
      }
    }
  }

  gates.pMax = feat.pMax >= pMaxMin;
  gates.snr = feat.snrDb >= snrDbMin;
  gates.flat = feat.flatness <= flatMax;
  gates.snrFast = feat.snrFast >= snrFastMin;
  gates.flatFast = feat.flatFast <= flatFastMax;
  gates.eRatio = feat.eRatio >= eRatioMin;
  gates.lowband = feat.lowband <= lowbandMax;
  gates.flux = feat.fluxDb <= fluxMax;
  return gates;
}

int WhistleRecognizer::passedGateCount(const GateEvaluation& gates)
{
  return static_cast<int>(gates.pMax) +
         static_cast<int>(gates.snr) +
         static_cast<int>(gates.flat) +
         static_cast<int>(gates.snrFast) +
         static_cast<int>(gates.flatFast) +
         static_cast<int>(gates.eRatio) +
         static_cast<int>(gates.lowband) +
         static_cast<int>(gates.flux) +
         static_cast<int>(!gates.stationary);
}

std::string WhistleRecognizer::formatGateSummary(const FrameFeatures& feat, const GateEvaluation& gates, size_t channel) const
{
  std::ostringstream stream;
  stream << "ch=" << channel
         << " peakHz=" << feat.peakFreq
         << " pMax=" << feat.pMax << "/" << pMaxMin
         << " snr=" << feat.snrDb << "/" << snrDbMin
         << " flat=" << feat.flatness << "/" << flatMax
         << " snrFast=" << feat.snrFast << "/" << snrFastMin
         << " flatFast=" << feat.flatFast << "/" << flatFastMax
         << " eRatio=" << feat.eRatio << "/" << eRatioMin
         << " lowband=" << feat.lowband << "/" << lowbandMax
         << " flux=" << feat.fluxDb << "/" << fluxMax;

  std::array<const char*, 9> failedLabels = {
    gates.stationary ? "stationary" : nullptr,
    !gates.pMax ? "pMax" : nullptr,
    !gates.snr ? "snr" : nullptr,
    !gates.flat ? "flat" : nullptr,
    !gates.snrFast ? "snrFast" : nullptr,
    !gates.flatFast ? "flatFast" : nullptr,
    !gates.eRatio ? "eRatio" : nullptr,
    !gates.lowband ? "lowband" : nullptr,
    !gates.flux ? "flux" : nullptr,
  };

  bool first = true;
  stream << " fail=[";
  for(const char* label : failedLabels)
  {
    if(!label)
      continue;
    if(!first)
      stream << ",";
    stream << label;
    first = false;
  }
  stream << "]";
  return stream.str();
}

bool WhistleRecognizer::updateFSM(bool ok, ChannelState& state, int offHangover, int minDistSamples)
{
  const int hopSamples = std::max(1, static_cast<int>(bufferSize * newSampleRatio));
  state.fsmCurrentSample += hopSamples;

  bool opened = false;
  switch(state.fsmState)
  {
    case ChannelState::FsmState::IDLE:
      if(ok && (state.fsmCurrentSample - state.fsmLastEndSample) >= minDistSamples)
      {
        state.fsmOkRun = 1;
        state.fsmState = ChannelState::FsmState::CANDIDATE;
      }
      else
        state.fsmOkRun = 0;
      break;

    case ChannelState::FsmState::CANDIDATE:
      if(ok)
      {
        if(++state.fsmOkRun >= onsetConsec)
        {
          state.fsmState = ChannelState::FsmState::ACTIVE;
          state.fsmOffRun = 0;
          state.fsmGapBudget = gapFill;
          opened = true;
        }
      }
      else
      {
        state.fsmOkRun = 0;
        state.fsmState = ChannelState::FsmState::IDLE;
      }
      break;

    case ChannelState::FsmState::ACTIVE:
      if(ok)
      {
        state.fsmOffRun = 0;
        state.fsmGapBudget = gapFill;
      }
      else if(state.fsmGapBudget > 0)
        --state.fsmGapBudget;
      else if(++state.fsmOffRun >= offHangover)
      {
        state.fsmState = ChannelState::FsmState::IDLE;
        state.fsmLastEndSample = state.fsmCurrentSample;
        state.fsmOkRun = 0;
        state.fsmOffRun = 0;
        state.fsmGapBudget = gapFill;
      }
      break;
  }

  return opened;
}

void WhistleRecognizer::update(Whistle& theWhistle)
{
  DECLARE_PLOT("module:WhistleRecognizer:snrDb0");
  DECLARE_PLOT("module:WhistleRecognizer:snrDb1");
  DECLARE_PLOT("module:WhistleRecognizer:snrDb2");
  DECLARE_PLOT("module:WhistleRecognizer:snrDb3");
  DECLARE_PLOT("module:WhistleRecognizer:samples0");
  DECLARE_PLOT("module:WhistleRecognizer:samples1");
  DECLARE_PLOT("module:WhistleRecognizer:samples2");
  DECLARE_PLOT("module:WhistleRecognizer:samples3");
  DECLARE_PLOT("module:WhistleRecognizer:bestPMax");
  DECLARE_PLOT("module:WhistleRecognizer:bestFlatness");
  DECLARE_PLOT("module:WhistleRecognizer:bestSnrDb");
  DECLARE_PLOT("module:WhistleRecognizer:bestSnrFast");
  DECLARE_PLOT("module:WhistleRecognizer:bestFlatFast");
  DECLARE_PLOT("module:WhistleRecognizer:bestERatio");
  DECLARE_PLOT("module:WhistleRecognizer:bestLowband");
  DECLARE_PLOT("module:WhistleRecognizer:bestFluxDb");
  DECLARE_PLOT("module:WhistleRecognizer:active");

  const bool inCalibration = theGameState.playerState == GameState::calibration;
  const bool soundIsPlayingNow = SystemCall::soundIsPlaying();

  SystemCall::mute(mute
                   && (theGameState.isSet() || theGameState.isPlaying())
                   && !theGameState.isPenalized()
                   && !inCalibration);

  soundWasPlaying |= soundIsPlayingNow;

  const bool shouldDetectWhistles = (theGameState.isSet() || theGameState.isPlaying()) && !soundWasPlaying;
  // Keep monitoring active in any game state so thresholds can be tuned without arming whistle-triggered behaviors.
  const bool shouldRecord = !soundWasPlaying;
  if(!hasRecorded && shouldRecord)
  {
    buffers.clear();
    channelStates.clear();
    sourceFrameOffset = 0.0;
    lastInputSampleRate = 0;
    samplesRequired = 0;
  }
  hasRecorded = shouldRecord;

  if(lastInputSampleRate != 0 && lastInputSampleRate != theAudioData.sampleRate)
  {
    buffers.clear();
    channelStates.clear();
    sourceFrameOffset = 0.0;
    samplesRequired = 0;
  }

  buffers.resize(theAudioData.channels);
  channelStates.resize(theAudioData.channels);
  for(auto& buffer : buffers)
    buffer.reserve(bufferSize);
  for(auto& state : channelStates)
  {
    state.bpBuf.reserve(bufferSize);
    state.lpBuf.reserve(bufferSize);
  }
  if(buffers.empty())
    return;

  if(lastInputSampleRate != theAudioData.sampleRate)
  {
    if(theAudioData.sampleRate == sampleRate)
      OUTPUT_WARNING("WhistleRecognizer: input sample rate is now " << theAudioData.sampleRate << " Hz.");
    else if(theAudioData.sampleRate > sampleRate)
      OUTPUT_WARNING("WhistleRecognizer: input sample rate is " << theAudioData.sampleRate << " Hz, resampling to " << sampleRate << " Hz.");
    else
      OUTPUT_WARNING("WhistleRecognizer: input sample rate is only " << theAudioData.sampleRate << " Hz, below required " << sampleRate << " Hz.");
  }
  lastInputSampleRate = theAudioData.sampleRate;

  if(theAudioData.sampleRate < sampleRate)
  {
    sourceFrameOffset = 0.0;
    return;
  }

  const double sourceFramesPerTargetFrame = static_cast<double>(theAudioData.sampleRate) / static_cast<double>(sampleRate);
  const size_t inputFrames = theAudioData.samples.size() / theAudioData.channels;
  double sourceFrame = sourceFrameOffset;
  for(; sourceFrame < static_cast<double>(inputFrames); sourceFrame += sourceFramesPerTargetFrame)
  {
    const size_t inputFrameIndex = static_cast<size_t>(sourceFrame);
    const size_t inputSampleIndex = inputFrameIndex * theAudioData.channels;
    --samplesRequired;

    for(size_t channel = 0; channel < theAudioData.channels; ++channel)
    {
      const AudioData::Sample rawSample = theAudioData.samples[inputSampleIndex + channel];
      const float sample = static_cast<float>(rawSample);
      buffers[channel].push_front(rawSample);
      channelStates[channel].bpBuf.push_front(applyBP(sample, channelStates[channel].bp_z));
      channelStates[channel].lpBuf.push_front(applyLP(sample, channelStates[channel].lp_z));
    }
  }
  sourceFrameOffset = sourceFrame - static_cast<double>(inputFrames);

  const int firstBuffer = theDamageConfigurationHead.audioChannelsDefect[0] ? 1 : 0;
  if(firstBuffer >= static_cast<int>(buffers.size()))
    return;

  if(soundWasPlaying)
    theWhistle.channelsUsedForWhistleDetection = 0;

  if(!theWhistle.channelsUsedForWhistleDetection && !soundWasPlaying)
  {
    for(size_t i = 0; i < buffers.size(); ++i)
      if(!theDamageConfigurationHead.audioChannelsDefect[i])
        ++theWhistle.channelsUsedForWhistleDetection;
  }

  for(size_t i = 0; i < buffers.size(); ++i)
  {
    if(!buffers[i].empty())
      switch(i)
      {
        case 0: PLOT("module:WhistleRecognizer:samples0", buffers[i].back()); break;
        case 1: PLOT("module:WhistleRecognizer:samples1", buffers[i].back()); break;
        case 2: PLOT("module:WhistleRecognizer:samples2", buffers[i].back()); break;
        case 3: PLOT("module:WhistleRecognizer:samples3", buffers[i].back()); break;
      }
  }

  const int hopSamples = std::max(1, static_cast<int>(bufferSize * newSampleRatio));
  const float hopSec = static_cast<float>(hopSamples) / static_cast<float>(sampleRate);
  const int offHangover = std::max(1, static_cast<int>(std::ceil(offMs / 1000.0f / hopSec)));
  const int minDistSamples = static_cast<int>(sampleRate * minDistMs / 1000.0f);

  if(shouldRecord && buffers[firstBuffer].full() && samplesRequired <= 0)
  {
    struct CandidateSummary
    {
      bool valid = false;
      size_t channel = 0;
      FrameFeatures feat;
      GateEvaluation gates;
      int passCount = -1;
      bool active = false;
    } bestRejectedCandidate;

    CandidateSummary bestObservedCandidate;

    float correlation = 0.0f;
    size_t defects = 0;
    bool anyActive = false;
    bool loggedDetection = false;
    bool haveBestFeatures = false;
    FrameFeatures bestFeatures;

    for(size_t i = 0; i < buffers.size(); ++i)
    {
      if(theDamageConfigurationHead.audioChannelsDefect[i] || !buffers[i].full())
      {
        ++defects;
        continue;
      }

      ChannelState& state = channelStates[i];
      const FrameFeatures feat = analyzeFrame(state);
      const GateEvaluation gates = evaluateGates(feat, state);
      const bool ok = !gates.stationary
                      && gates.pMax
                      && gates.snr
                      && gates.flat
                      && gates.snrFast
                      && gates.flatFast
                      && gates.eRatio
                      && gates.lowband
                      && gates.flux;

      const bool opened = shouldDetectWhistles && updateFSM(ok, state, offHangover, minDistSamples);
      const bool active = state.fsmState == ChannelState::FsmState::ACTIVE || opened;
      const int passCount = passedGateCount(gates);

      if(!bestObservedCandidate.valid ||
         feat.pMax > bestObservedCandidate.feat.pMax ||
         (feat.pMax == bestObservedCandidate.feat.pMax && passCount > bestObservedCandidate.passCount))
      {
        bestObservedCandidate.valid = true;
        bestObservedCandidate.channel = i;
        bestObservedCandidate.feat = feat;
        bestObservedCandidate.gates = gates;
        bestObservedCandidate.passCount = passCount;
        bestObservedCandidate.active = active;
      }

      if(active)
      {
        const float snrScore = clamp01((feat.snrDb - snrDbMin) / 15.0f);
        const float flatScore = std::max(0.0f, 1.0f - feat.flatness / std::max(flatMax, 1e-6f));
        const float eRatioScore = clamp01((feat.eRatio - eRatioMin) / (1.0f - eRatioMin + 1e-6f));
        correlation = std::max(correlation, snrScore * flatScore * eRatioScore);
        anyActive = true;
      }
      else if(shouldRecord)
      {
        if((!bestRejectedCandidate.valid || passCount > bestRejectedCandidate.passCount ||
            (passCount == bestRejectedCandidate.passCount && feat.pMax > bestRejectedCandidate.feat.pMax)) &&
           (passCount >= 5 || feat.pMax >= 0.5f * pMaxMin))
        {
          bestRejectedCandidate.valid = true;
          bestRejectedCandidate.channel = i;
          bestRejectedCandidate.feat = feat;
          bestRejectedCandidate.gates = gates;
          bestRejectedCandidate.passCount = passCount;
        }
      }

      if(!haveBestFeatures || feat.pMax > bestFeatures.pMax)
      {
        bestFeatures = feat;
        haveBestFeatures = true;
      }

      if(opened && logWhistleDetections &&
         (lastLogTime == 0 || theFrameInfo.getTimeSince(lastLogTime) >= logIntervalMs))
      {
        OUTPUT_WARNING("Whistle detected: " << formatGateSummary(feat, gates, i)
                       << " corr=" << correlation);
        lastLogTime = theFrameInfo.time;
        loggedDetection = true;
      }

      switch(i)
      {
        case 0: PLOT("module:WhistleRecognizer:snrDb0", feat.snrDb); break;
        case 1: PLOT("module:WhistleRecognizer:snrDb1", feat.snrDb); break;
        case 2: PLOT("module:WhistleRecognizer:snrDb2", feat.snrDb); break;
        case 3: PLOT("module:WhistleRecognizer:snrDb3", feat.snrDb); break;
      }
    }

    if(haveBestFeatures)
    {
      PLOT("module:WhistleRecognizer:bestPMax", bestFeatures.pMax);
      PLOT("module:WhistleRecognizer:bestFlatness", bestFeatures.flatness);
      PLOT("module:WhistleRecognizer:bestSnrDb", bestFeatures.snrDb);
      PLOT("module:WhistleRecognizer:bestSnrFast", bestFeatures.snrFast);
      PLOT("module:WhistleRecognizer:bestFlatFast", bestFeatures.flatFast);
      PLOT("module:WhistleRecognizer:bestERatio", bestFeatures.eRatio);
      PLOT("module:WhistleRecognizer:bestLowband", bestFeatures.lowband);
      PLOT("module:WhistleRecognizer:bestFluxDb", bestFeatures.fluxDb);
    }

    PLOT("module:WhistleRecognizer:active", anyActive ? 1.0f : 0.0f);

    if(defects < buffers.size() && anyActive)
    {
      const int validChannels = static_cast<int>(buffers.size() - defects);
      if(correlation >= minCorrelation && correlation >= bestCorrelation)
      {
        theWhistle.confidenceOfLastWhistleDetection = correlation;
        theWhistle.channelsUsedForWhistleDetection = static_cast<unsigned char>(validChannels);
        bestCorrelation = correlation;

        if(theFrameInfo.getTimeSince(lastTimeWhistleDetected) > minAnnotationDelay)
          ANNOTATION("WhistleRecognizer", "whistle with " << static_cast<int>(bestCorrelation * 100.f) << "%");
        lastTimeWhistleDetected = theFrameInfo.time;
      }
    }
    else if(logWhistleMonitoring && bestObservedCandidate.valid &&
            !loggedDetection &&
            (lastLogTime == 0 || theFrameInfo.getTimeSince(lastLogTime) >= logIntervalMs))
    {
      OUTPUT_WARNING("Whistle monitor: "
                     << formatGateSummary(bestObservedCandidate.feat,
                                          bestObservedCandidate.gates,
                                          bestObservedCandidate.channel)
                     << " passCount=" << bestObservedCandidate.passCount
                     << " active=" << (bestObservedCandidate.active ? "yes" : "no"));
      lastLogTime = theFrameInfo.time;
    }
    else if(logRejectedWhistleCandidates && bestRejectedCandidate.valid &&
            !loggedDetection &&
            (lastLogTime == 0 || theFrameInfo.getTimeSince(lastLogTime) >= logIntervalMs))
    {
      OUTPUT_WARNING("Whistle near miss: "
                     << formatGateSummary(bestRejectedCandidate.feat,
                                          bestRejectedCandidate.gates,
                                          bestRejectedCandidate.channel));
      lastLogTime = theFrameInfo.time;
    }

    samplesRequired = hopSamples;
  }

  if(theFrameInfo.getTimeSince(lastTimeWhistleDetected) >= accumulationDuration)
  {
    theWhistle.lastTimeWhistleDetected = lastTimeWhistleDetected;
    bestCorrelation = 0.0f;
    soundWasPlaying = SystemCall::soundIsPlaying();
  }

  DEBUG_RESPONSE_ONCE("module:WhistleRecognizer:detectNow")
  {
    lastTimeWhistleDetected = theFrameInfo.time;
    theWhistle.confidenceOfLastWhistleDetection = 2.f;
  }
}
