/**
 * @file WhistleRecognizer.h
 *
 * This file declares a module that identifies the sound of a whistle.
 *
 * Original authors:
 *  - Tim Laue
 *  - Dennis Schuethe
 *  - Thomas Röfer
 *
 * Goertzel Gate V4 (SabanaHerons 2026):
 *  - Frame-based: N=320 (20 ms), hop=80 (5 ms), 3600-4500 Hz
 *  - Gates: SNR, flatness, bandwidth, low-band ratio, flux veto,
 *           frequency stability, stationary mask
 *  - 3-state FSM: IDLE -> CANDIDATE (onset_consec) -> ACTIVE
 */

#pragma once

#include "Representations/Configuration/DamageConfiguration.h"
#include "Representations/Infrastructure/AudioData.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Modeling/Whistle.h"
#include "Framework/Module.h"
#include "Math/RingBuffer.h"

#include <vector>

MODULE(WhistleRecognizer,
{,
  REQUIRES(GameState),
  REQUIRES(AudioData),
  REQUIRES(DamageConfigurationHead),
  REQUIRES(FrameInfo),
  PROVIDES(Whistle),
  LOADS_PARAMETERS(
  {,
    (std::vector<std::string>) whistles,       /**< Legacy: unused. */
    (unsigned) bufferSize,                     /**< Frame size in samples (should be 320). */
    (unsigned) sampleRate,                     /**< Target sample rate (should be 16000). */
    (float) newSampleRatio,                    /**< Hop ratio (should be 0.25 -> 80-sample hop). */
    (float) minVolume,                         /**< Quick-reject: min peak amplitude [0..1). */
    (float) minCorrelation,                    /**< Min confidence score to report detection. */
    (int) accumulationDuration,                /**< Ms after last detection before publishing. */
    (int) minAnnotationDelay,                  /**< Min ms between whistle annotations. */
    (bool) mute,                               /**< Mute speaker during Set/Playing. */
    (float)(3600.0f) goertzelMinFreq,          /**< Whistle band lower edge (Hz). */
    (float)(4500.0f) goertzelMaxFreq,          /**< Whistle band upper edge (Hz). */
    (float)(16.0f) snrDbMin,                   /**< Min SNR for whistle frame (dB). */
    (float)(0.32f) flatMax,                    /**< Max spectral flatness (0..1). */
    (float)(120.0f) bwMax,                     /**< Max -3 dB bandwidth (Hz). */
    (float)(0.5f) lowbandMax,                  /**< Max E_low/E_whistle energy ratio. */
    (float)(7.0f) fluxDbMin,                   /**< Flux veto threshold (dB): ok if flux < this. */
    (float)(200.0f) freqStab,                  /**< Max allowed peak-frequency jump (Hz). */
    (int)(3) onsetConsec,                      /**< Consecutive OK frames to confirm onset. */
    (int)(170) offMs,                          /**< Hangover after last OK frame (ms). */
    (int)(3) gapFill,                          /**< Gap-fill budget (frames) inside ACTIVE. */
    (int)(150) minDistMs,                      /**< Min distance between whistle events (ms). */
    (float)(1.0f) stationaryHoldSec,           /**< Stationary-mask hold time (s). */
  }),
});

class WhistleRecognizer : public WhistleRecognizerBase
{
  std::vector<RingBuffer<AudioData::Sample>> buffers;
  bool soundWasPlaying = false;
  bool hasRecorded = false;
  int samplesRequired = 0;
  double sourceFrameOffset = 0.0;
  unsigned lastInputSampleRate = 0;
  float bestCorrelation = 0.0f;
  unsigned lastTimeWhistleDetected = 0;

  void update(Whistle& theWhistle) override;

  struct FrameFeatures
  {
    float peakFreq = 0.0f;
    float snrDb = 0.0f;
    float flatness = 1.0f;
    float bwHz = 0.0f;
    float lowRatio = 0.0f;
    float fluxDb = 0.0f;
    int peakIdx = 0;
    std::vector<float> powers;
  };

  struct ChannelState
  {
    std::vector<float> prevPowers;
    bool hasPrevPowers = false;
    float prevPeakFreq = -1.0f;

    std::vector<int> stationaryCounter;
    int stationaryThreshold = 0;

    enum class FsmState { IDLE, CANDIDATE, ACTIVE } fsmState = FsmState::IDLE;
    int fsmOkRun = 0;
    int fsmOffRun = 0;
    int fsmGapBudget = 0;
    int fsmLastEndSample = -1000000;
    int fsmCurrentSample = 0;
  };
  std::vector<ChannelState> channelStates;

  FrameFeatures analyzeFrame(const RingBuffer<AudioData::Sample>& buffer, ChannelState& state);
  bool updateFSM(bool ok, ChannelState& state, int offHangover, int minDistSamples);
  void initChannelState(ChannelState& state, int nBins);

public:
  WhistleRecognizer();
  ~WhistleRecognizer() override;
};
