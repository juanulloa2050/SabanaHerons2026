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
 * Dual Goertzel Gate V8 (SabanaHerons 2026):
 *  - IIR pre-filter: BP 2500-6000 Hz + LP 1500 Hz
 *  - Dual-window: N=320 (20 ms), N_FAST=160 (10 ms), hop=80 (5 ms)
 *  - Independent standard and hand-squeeze-acute detector profiles
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
    (float)(3057.556512f) goertzelMinFreq,     /**< Whistle band lower edge (Hz). */
    (float)(4365.593451f) goertzelMaxFreq,     /**< Whistle band upper edge (Hz). */
    (float)(41.206368f) pMaxMin,               /**< Stage 1: min peak Goertzel power. */
    (float)(3.237137f) snrDbMin,               /**< Stage 2 main-window min SNR (dB). */
    (float)(0.797188f) flatMax,                /**< Stage 2 main-window max flatness. */
    (float)(3.667951f) snrFastMin,             /**< Stage 2 fast-window min SNR (dB). */
    (float)(0.266438f) flatFastMax,            /**< Stage 2 fast-window max flatness. */
    (float)(7.410762f) fluxMax,                /**< Stage 2 max one-sided spectral flux (dB). */
    (float)(2.330469f) lowbandMax,             /**< Stage 2 max LP/BP energy ratio. */
    (float)(0.383048f) eRatioMin,              /**< Stage 2 min BP/total energy ratio. */
    (int)(3) onsetConsec,                      /**< Consecutive OK frames to confirm onset. */
    (int)(171) offMs,                          /**< Hangover after last OK frame (ms). */
    (int)(7) gapFill,                          /**< Gap-fill budget (frames) inside ACTIVE. */
    (int)(150) minDistMs,                      /**< Min distance between whistle events (ms). */
    (float)(1.0f) stationaryHoldSec,           /**< Stationary-mask hold time (s). */
    (bool)(true) acuteWhistleEnabled,           /**< Enable the hand-squeeze acute whistle profile. */
    (float)(3050.0f) acuteGoertzelMinFreq,      /**< Acute profile lower aligned Goertzel bin (Hz). */
    (float)(4400.0f) acuteGoertzelMaxFreq,      /**< Acute profile upper aligned Goertzel bin (Hz). */
    (float)(3000.0f) acuteFastMinFreq,          /**< Acute fast-window lower aligned bin (Hz). */
    (float)(4400.0f) acuteFastMaxFreq,          /**< Acute fast-window upper aligned bin (Hz). */
    (float)(41.2064f) acutePMaxMin,             /**< Acute profile minimum peak Goertzel power. */
    (float)(3.2371f) acuteSnrDbMin,             /**< Acute profile main-window minimum SNR (dB). */
    (float)(0.7972f) acuteFlatMax,              /**< Acute profile main-window maximum flatness. */
    (float)(3.6680f) acuteSnrFastMin,           /**< Acute profile fast-window minimum SNR (dB). */
    (float)(0.2664f) acuteFlatFastMax,          /**< Acute profile fast-window maximum flatness. */
    (float)(7.4108f) acuteFluxMax,              /**< Acute profile maximum one-sided flux (dB). */
    (float)(2.3305f) acuteLowbandMax,           /**< Acute profile maximum LP/BP energy ratio. */
    (float)(0.3830f) acuteERatioMin,            /**< Acute profile minimum BP/total energy ratio. */
    (int)(3) acuteOnsetConsec,                  /**< Acute profile consecutive frames for onset. */
    (int)(34) acuteOffFrames,                   /**< Acute profile release time in 5 ms hops. */
    (int)(7) acuteGapFill,                      /**< Acute profile gap-fill budget. */
    (int)(150) acuteMinDistMs,                  /**< Acute profile minimum inter-event distance. */
    (float)(1.0f) acuteStationaryHoldSec,       /**< Acute profile stationary-mask hold time. */
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

  struct DetectorProfile
  {
    const char* name;
    float minFreq;
    float maxFreq;
    float fastMinFreq;
    float fastMaxFreq;
    float pMaxMin;
    float snrDbMin;
    float flatMax;
    float snrFastMin;
    float flatFastMax;
    float fluxMax;
    float lowbandMax;
    float eRatioMin;
    int onsetConsec;
    int offFrames;
    int gapFill;
    int minDistMs;
    float stationaryHoldSec;
  };

  struct DetectorState
  {
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

  struct ChannelState
  {
    double bp_z[BP_N_SOS][2] = {};
    double lp_z[LP_N_SOS][2] = {};

    RingBuffer<float> bpBuf;
    RingBuffer<float> lpBuf;
    std::array<DetectorState, 2> detectors;
  };
  std::vector<ChannelState> channelStates;

  static float applyBP(float x, double (&z)[BP_N_SOS][2]);
  static float applyLP(float x, double (&z)[LP_N_SOS][2]);

  std::array<DetectorProfile, 2> detectorProfiles() const;
  FrameFeatures analyzeFrame(ChannelState& channel, DetectorState& state, const DetectorProfile& profile);
  GateEvaluation evaluateGates(const FrameFeatures& feat, const DetectorState& state, const DetectorProfile& profile) const;
  bool updateFSM(bool ok, DetectorState& state, const DetectorProfile& profile);
  void initDetectorState(DetectorState& state, int nBins, const DetectorProfile& profile);
  static int passedGateCount(const GateEvaluation& gates);
  std::string formatGateSummary(const FrameFeatures& feat,
                                const GateEvaluation& gates,
                                size_t channel,
                                const DetectorProfile& profile) const;
  unsigned lastLogTime = 0;

public:
  WhistleRecognizer();
  ~WhistleRecognizer() override;
};
