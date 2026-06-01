/**
 * @file WhistleRecognizer.cpp
 *
 * Goertzel Gate V4 - SabanaHerons 2026
 *
 * Architecture (mirrors Gate_testing_v4.py / randomsearch_v4.py):
 *   SR=16000, N=320 (20 ms frame), hop=80 (5 ms), band=3600-4500 Hz
 *
 * Per-frame gates:
 *   snr_db >= snrDbMin
 *   spectral_flatness <= flatMax
 *   -3dB bandwidth <= bwMax
 *   E_low/E_whistle <= lowbandMax        (crowd/noise rejection)
 *   goertzel_flux < fluxDbMin            (transient rejection)
 *   |peak_freq - prev_peak_freq| <= freqStab
 *   peak bin NOT stationary (held > stationaryHoldSec)
 *
 * 3-state FSM (per channel):
 *   IDLE -> CANDIDATE after first OK frame (minimum distance satisfied)
 *        -> ACTIVE after onsetConsec consecutive OK frames  <- onset event
 *        -> back to IDLE after offMs of silence (with gapFill budget)
 */

#include "WhistleRecognizer.h"
#include "Platform/SystemCall.h"
#include "Debugging/Annotation.h"
#include "Debugging/Plot.h"

#include <algorithm>
#include <cmath>
#include <numeric>

MAKE_MODULE(WhistleRecognizer);

WhistleRecognizer::WhistleRecognizer() = default;
WhistleRecognizer::~WhistleRecognizer() = default;

void WhistleRecognizer::initChannelState(ChannelState& state, int nBins)
{
  if(static_cast<int>(state.stationaryCounter.size()) == nBins)
    return;

  state.prevPowers.assign(nBins, 0.0f);
  state.hasPrevPowers = false;
  state.stationaryCounter.assign(nBins, 0);

  const int hopSamples = std::max(1, static_cast<int>(bufferSize * newSampleRatio));
  const float hopSec = static_cast<float>(hopSamples) / static_cast<float>(sampleRate);
  state.stationaryThreshold = std::max(1, static_cast<int>(stationaryHoldSec / hopSec));
}

WhistleRecognizer::FrameFeatures WhistleRecognizer::analyzeFrame(const RingBuffer<AudioData::Sample>& buffer, ChannelState& state)
{
  FrameFeatures feat;

  const int N = static_cast<int>(buffer.size());
  const float fs = static_cast<float>(sampleRate);

  float maxAmp = 0.0f;
  for(size_t i = 0; i < buffer.size(); ++i)
    maxAmp = std::max(maxAmp, std::abs(static_cast<float>(buffer[i])));
  if(maxAmp < minVolume)
    return feat;

  const int kMin = static_cast<int>(std::ceil(goertzelMinFreq * N / fs));
  const int kMax = static_cast<int>(std::floor(goertzelMaxFreq * N / fs));
  const int nBins = kMax - kMin + 1;
  if(nBins <= 0)
    return feat;

  initChannelState(state, nBins);

  feat.powers.resize(nBins);
  std::vector<float> freqs(nBins);

  for(int k = kMin; k <= kMax; ++k)
  {
    const int idx = k - kMin;
    const float w = 2.0f * static_cast<float>(M_PI) * k / N;
    const float coeff = 2.0f * std::cos(w);
    float q1 = 0.0f, q2 = 0.0f;
    for(size_t i = 0; i < buffer.size(); ++i)
    {
      const float q0 = coeff * q1 - q2 + static_cast<float>(buffer[i]);
      q2 = q1;
      q1 = q0;
    }
    feat.powers[idx] = q1 * q1 + q2 * q2 - q1 * q2 * coeff;
    freqs[idx] = k * fs / N;
  }

  const auto maxIt = std::max_element(feat.powers.begin(), feat.powers.end());
  const int peakIdx = static_cast<int>(maxIt - feat.powers.begin());
  feat.peakIdx = peakIdx;
  feat.peakFreq = freqs[peakIdx];

  float sum_rest = 0.0f;
  int count_rest = 0;
  for(int i = 0; i < nBins; ++i)
  {
    if(std::abs(i - peakIdx) > 1)
    {
      sum_rest += feat.powers[i];
      ++count_rest;
    }
  }
  const float avgRest = count_rest > 0 ? sum_rest / count_rest : feat.powers[peakIdx] * 0.1f;
  feat.snrDb = 10.0f * std::log10((feat.powers[peakIdx] + 1e-12f) / (avgRest + 1e-12f));

  float geometric_sum = 0.0f;
  float arithmetic_sum = 0.0f;
  for(float power : feat.powers)
  {
    const float safe_power = power + 1e-12f;
    geometric_sum += std::log(safe_power);
    arithmetic_sum += safe_power;
  }
  feat.flatness = std::exp(geometric_sum / nBins) / (arithmetic_sum / nBins);

  const float halfPower = feat.powers[peakIdx] * 0.5f;
  int leftIdx = peakIdx;
  int rightIdx = peakIdx;
  while(leftIdx > 0 && feat.powers[leftIdx - 1] >= halfPower)
    --leftIdx;
  while(rightIdx < nBins - 1 && feat.powers[rightIdx + 1] >= halfPower)
    ++rightIdx;
  feat.bwHz = (rightIdx - leftIdx + 1) * (fs / N);

  const int kLowMin = static_cast<int>(std::ceil(100.0f * N / fs));
  const int kLowMax = static_cast<int>(std::floor(1500.0f * N / fs));
  float eLow = 1e-12f;
  for(int k = kLowMin; k <= kLowMax; ++k)
  {
    const float w = 2.0f * static_cast<float>(M_PI) * k / N;
    const float coeff = 2.0f * std::cos(w);
    float q1 = 0.0f, q2 = 0.0f;
    for(size_t i = 0; i < buffer.size(); ++i)
    {
      const float q0 = coeff * q1 - q2 + static_cast<float>(buffer[i]);
      q2 = q1;
      q1 = q0;
    }
    eLow += q1 * q1 + q2 * q2 - q1 * q2 * coeff;
  }
  const float eWhistle = std::accumulate(feat.powers.begin(), feat.powers.end(), 1e-12f);
  feat.lowRatio = eLow / eWhistle;

  if(state.hasPrevPowers && static_cast<int>(state.prevPowers.size()) == nBins)
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
  state.hasPrevPowers = true;

  for(int i = 0; i < nBins; ++i)
    state.stationaryCounter[i] = std::max(0, state.stationaryCounter[i] - 1);
  state.stationaryCounter[peakIdx] += 2;

  return feat;
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
  DECLARE_PLOT("module:WhistleRecognizer:correlation0");
  DECLARE_PLOT("module:WhistleRecognizer:correlation1");
  DECLARE_PLOT("module:WhistleRecognizer:correlation2");
  DECLARE_PLOT("module:WhistleRecognizer:correlation3");
  DECLARE_PLOT("module:WhistleRecognizer:correlation4");
  DECLARE_PLOT("module:WhistleRecognizer:correlation5");
  DECLARE_PLOT("module:WhistleRecognizer:samples0");
  DECLARE_PLOT("module:WhistleRecognizer:samples1");
  DECLARE_PLOT("module:WhistleRecognizer:samples2");
  DECLARE_PLOT("module:WhistleRecognizer:samples3");

  // Switch off sound to hear the whistle.
  SystemCall::mute(mute
                   && (theGameState.isSet() || theGameState.isPlaying())
                   && !theGameState.isPenalized()
                   && theGameState.playerState != GameState::calibration);

  // Remember if sound was playing during this accumulation phase.
  // Except for some corner cases, soundWasPlaying should be false.
  soundWasPlaying |= SystemCall::soundIsPlaying();

  // Empty buffers when entering a state where it should be recorded.
  const bool shouldRecord = (theGameState.isSet() || theGameState.isPlaying())
                            && !soundWasPlaying;
  if(!hasRecorded && shouldRecord)
  {
    buffers.clear();
    channelStates.clear();
    sourceFrameOffset = 0.0;
    lastInputSampleRate = 0;
  }
  hasRecorded = shouldRecord;

  buffers.resize(theAudioData.channels);
  channelStates.resize(theAudioData.channels);
  for(auto& buffer : buffers)
    buffer.reserve(bufferSize);
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

    if(lastInputSampleRate != 0)
    {
      sourceFrameOffset = 0.0;
      samplesRequired = 0;
    }
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
      buffers[channel].push_front(theAudioData.samples[inputSampleIndex + channel]);
  }
  sourceFrameOffset = sourceFrame - static_cast<double>(inputFrames);

  const int firstBuffer = theDamageConfigurationHead.audioChannelsDefect[0] ? 1 : 0;
  if(firstBuffer >= static_cast<int>(buffers.size()))
    return;

  if(soundWasPlaying)
    theWhistle.channelsUsedForWhistleDetection = 0;

  if(!theWhistle.channelsUsedForWhistleDetection && !soundWasPlaying)
    for(size_t i = 0; i < buffers.size(); ++i)
      if(!theDamageConfigurationHead.audioChannelsDefect[i])
        ++theWhistle.channelsUsedForWhistleDetection;

  for(size_t i = 0; i < buffers.size(); ++i)
    if(!buffers[i].empty())
      switch(i)
      {
        case 0: PLOT("module:WhistleRecognizer:samples0", buffers[i].back()); break;
        case 1: PLOT("module:WhistleRecognizer:samples1", buffers[i].back()); break;
        case 2: PLOT("module:WhistleRecognizer:samples2", buffers[i].back()); break;
        case 3: PLOT("module:WhistleRecognizer:samples3", buffers[i].back()); break;
      }

  const int hopSamples = std::max(1, static_cast<int>(bufferSize * newSampleRatio));
  const float hopSec = static_cast<float>(hopSamples) / static_cast<float>(sampleRate);
  const int offHangover = std::max(1, static_cast<int>(std::ceil(offMs / 1000.0f / hopSec)));
  const int minDistSamples = static_cast<int>(sampleRate * minDistMs / 1000.0f);

  if(shouldRecord && buffers[firstBuffer].full() && samplesRequired <= 0)
  {
    float correlation = 0.0f;
    size_t defects = 0;
    bool anyActive = false;

    for(size_t i = 0; i < buffers.size(); ++i)
    {
      if(theDamageConfigurationHead.audioChannelsDefect[i] || !buffers[i].full())
      {
        ++defects;
        continue;
      }

      ChannelState& channelState = channelStates[i];
      const FrameFeatures features = analyzeFrame(buffers[i], channelState);

      const int nBins = static_cast<int>(channelState.stationaryCounter.size());
      const bool isStationary = nBins > 0
                                && channelState.stationaryCounter[features.peakIdx] >= channelState.stationaryThreshold;

      bool ok = features.peakFreq >= goertzelMinFreq
                && features.peakFreq <= goertzelMaxFreq
                && features.snrDb >= snrDbMin
                && features.flatness <= flatMax
                && features.bwHz <= bwMax
                && features.lowRatio <= lowbandMax
                && features.fluxDb < fluxDbMin
                && !isStationary;

      if(ok && channelState.prevPeakFreq >= 0.0f
         && std::abs(features.peakFreq - channelState.prevPeakFreq) > freqStab)
        ok = false;
      channelState.prevPeakFreq = ok ? features.peakFreq : -1.0f;

      const bool opened = updateFSM(ok, channelState, offHangover, minDistSamples);
      const bool active = channelState.fsmState == ChannelState::FsmState::ACTIVE || opened;

      if(active)
      {
        const float snrScore = std::max(0.0f, std::min(1.0f, (features.snrDb - snrDbMin) / 15.0f));
        const float flatScore = std::max(0.0f, 1.0f - features.flatness / flatMax);
        const float bwScore = std::max(0.0f, 1.0f - features.bwHz / bwMax);
        correlation = std::max(correlation, snrScore * flatScore * bwScore);
        anyActive = true;
      }

      switch(i)
      {
        case 0: PLOT("module:WhistleRecognizer:correlation0", features.snrDb); break;
        case 1: PLOT("module:WhistleRecognizer:correlation1", features.snrDb); break;
        case 2: PLOT("module:WhistleRecognizer:correlation2", features.snrDb); break;
        case 3: PLOT("module:WhistleRecognizer:correlation3", features.snrDb); break;
      }
    }

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
