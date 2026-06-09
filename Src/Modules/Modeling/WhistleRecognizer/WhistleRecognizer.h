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
 * Goertzel Gate V5 (SabanaHerons 2026):
 *  - IIR pre-filter: BP 2500-6000 Hz + LP 1500 Hz
 *  - Dual-window: N=320 (20 ms), N_FAST=160 (10 ms), hop=80 (5 ms)
 *  - Gates: Pmax, SNR/flatness (main+fast), energy ratios, flux, stationary mask
 *  - 3-state FSM: IDLE -> CANDIDATE (onsetConsec) -> ACTIVE
 */

#pragma once

#include "Representations/Configuration/DamageConfiguration.h"
#include "Representations/Infrastructure/AudioData.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Modeling/Whistle.h"
#include "Framework/Module.h"
#include "Math/RingBuffer.h"

#include <array>
#include <string>
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
    (unsigned) bufferSize,                     /**< Main analysis window, should be 320 at 16 kHz. */
    (unsigned) sampleRate,                     /**< Target sample rate, should be 16000. */
    (float) newSampleRatio,                    /**< Hop ratio, should be 0.25. */
    (float) minVolume,                         /**< Min BP amplitude to skip silence quickly. */
    (float) minCorrelation,                    /**< Min confidence score to report detection. */
    (int) accumulationDuration,                /**< Ms after last onset before publishing. */
    (int) minAnnotationDelay,                  /**< Min ms between whistle annotations. */
    (bool) mute,                               /**< Mute speaker during Set/Playing. */
    (float)(3600.0f) goertzelMinFreq,          /**< Whistle band lower edge (Hz). */
    (float)(4500.0f) goertzelMaxFreq,          /**< Whistle band upper edge (Hz). */
    (float)(632.8f) pMaxMin,                   /**< Stage 1: min peak Goertzel power. */
    (float)(7.26f) snrDbMin,                   /**< Stage 2 main-window min SNR (dB). */
    (float)(0.546f) flatMax,                   /**< Stage 2 main-window max flatness. */
    (float)(3.34f) snrFastMin,                 /**< Stage 2 fast-window min SNR (dB). */
    (float)(0.692f) flatFastMax,               /**< Stage 2 fast-window max flatness. */
    (float)(4.47f) fluxMax,                    /**< Stage 2 max one-sided spectral flux (dB). */
    (float)(6.01f) lowbandMax,                 /**< Stage 2 max LP/BP energy ratio. */
    (float)(0.279f) eRatioMin,                 /**< Stage 2 min BP/total energy ratio. */
    (int)(2) onsetConsec,                      /**< Consecutive OK frames to confirm onset. */
    (int)(115) offMs,                          /**< Hangover after last OK frame (ms). */
    (int)(5) gapFill,                          /**< Gap-fill budget (frames) inside ACTIVE. */
    (int)(150) minDistMs,                      /**< Min distance between whistle events (ms). */
    (float)(1.0f) stationaryHoldSec,           /**< Stationary-mask hold time (s). */
    (bool)(true) logWhistleMonitoring,         /**< Print periodic summaries of the strongest whistle candidate for tuning. */
    (bool)(true) logWhistleDetections,         /**< Print feature summaries on confirmed whistle detections. */
    (bool)(false) logRejectedWhistleCandidates,/**< Print near-miss feature summaries for parameter tuning. */
    (int)(1000) logIntervalMs,                 /**< Minimum spacing between repeated info logs. */
  }),
});

class WhistleRecognizer : public WhistleRecognizerBase
{
  static constexpr int BP_N_SOS = 4;
  static constexpr int LP_N_SOS = 2;
  static const double BP_B[BP_N_SOS][3];
  static const double BP_A[BP_N_SOS][2];
  static const double LP_B[LP_N_SOS][3];
  static const double LP_A[LP_N_SOS][2];

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
    float pMax = 0.0f;
    int peakIdx = 0;
    float snrDb = 0.0f;
    float flatness = 1.0f;
    float snrFast = 0.0f;
    float flatFast = 1.0f;
    float eRatio = 0.0f;
    float lowband = 0.0f;
    float fluxDb = 0.0f;
    float peakFreq = 0.0f;
    std::vector<float> powers;
  };

  struct GateEvaluation
  {
    bool stationary = false;
    bool pMax = false;
    bool snr = false;
    bool flat = false;
    bool snrFast = false;
    bool flatFast = false;
    bool eRatio = false;
    bool lowband = false;
    bool flux = false;
  };

  struct ChannelState
  {
    double bp_z[BP_N_SOS][2] = {};
    double lp_z[LP_N_SOS][2] = {};

    RingBuffer<float> bpBuf;
    RingBuffer<float> lpBuf;

    std::vector<float> prevPowers;
    bool prevPowersValid = false;

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

  static float applyBP(float x, double (&z)[BP_N_SOS][2]);
  static float applyLP(float x, double (&z)[LP_N_SOS][2]);

  FrameFeatures analyzeFrame(ChannelState& state);
  GateEvaluation evaluateGates(const FrameFeatures& feat, ChannelState& state) const;
  bool updateFSM(bool ok, ChannelState& state, int offHangover, int minDistSamples);
  void initChannelState(ChannelState& state, int nBins);
  static int passedGateCount(const GateEvaluation& gates);
  std::string formatGateSummary(const FrameFeatures& feat, const GateEvaluation& gates, size_t channel) const;
  unsigned lastLogTime = 0;

public:
  WhistleRecognizer();
  ~WhistleRecognizer() override;
};
