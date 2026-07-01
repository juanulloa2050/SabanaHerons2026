/**
 * @file WhistleRecognizer.h
 *
 * Three-profile whistle recognizer for SabanaHerons 2026.
 *
 * Combines the standalone Gate V8 hand-squeeze detector with the B-Human
 * WhistleRecognizer wrapper and adds a wider raw-spectrum mouth-whistle
 * profile. The module name intentionally remains WhistleRecognizer so this
 * file can be used as a drop-in replacement after review.
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
#include <cstddef>
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
    (unsigned) bufferSize,                     /**< Main analysis window, should be 320 at 16 kHz. */
    (unsigned) sampleRate,                     /**< Target sample rate, should be 16000. */
    (float) newSampleRatio,                    /**< Hop ratio, should be 0.25. */
    (float) minVolume,                         /**< Min analysis amplitude to skip silence quickly. */
    (float) minCorrelation,                    /**< Min confidence score to report detection. */
    (int) accumulationDuration,                /**< Ms after last onset before publishing. */
    (int) minAnnotationDelay,                  /**< Min ms between whistle annotations. */
    (bool) mute,                               /**< Mute speaker during Set/Playing. */

    (bool)(true) rescueWhistleEnabled,         /**< Enable the rescue/standard whistle profile. */
    (float)(3057.556512f) goertzelMinFreq,     /**< Rescue profile lower edge (Hz). */
    (float)(4365.593451f) goertzelMaxFreq,     /**< Rescue profile upper edge (Hz). */
    (float)(3000.0f) rescueFastMinFreq,        /**< Rescue fast-window lower edge (Hz). */
    (float)(4400.0f) rescueFastMaxFreq,        /**< Rescue fast-window upper edge (Hz). */
    (float)(41.206368f) pMaxMin,               /**< Rescue min peak Goertzel power. */
    (float)(3.237137f) snrDbMin,               /**< Rescue main-window min SNR (dB). */
    (float)(0.797188f) flatMax,                /**< Rescue main-window max flatness. */
    (float)(3.667951f) snrFastMin,             /**< Rescue fast-window min SNR (dB). */
    (float)(0.266438f) flatFastMax,            /**< Rescue fast-window max flatness. */
    (float)(7.410762f) fluxMax,                /**< Rescue max one-sided spectral flux (dB). */
    (float)(2.330469f) lowbandMax,             /**< Rescue max LP/whistle energy ratio. */
    (float)(0.383048f) eRatioMin,              /**< Rescue min whistle/total energy ratio. */
    (int)(3) onsetConsec,                      /**< Rescue consecutive OK frames to confirm onset. */
    (int)(171) offMs,                          /**< Rescue hangover after last OK frame (ms). */
    (int)(7) gapFill,                          /**< Rescue gap-fill budget (frames) inside ACTIVE. */
    (int)(150) minDistMs,                      /**< Rescue min distance between whistle events (ms). */
    (float)(1.0f) stationaryHoldSec,           /**< Rescue stationary-mask hold time (s). */

    (bool)(true) acuteWhistleEnabled,          /**< Enable the hand-squeeze acute whistle profile. */
    (float)(3050.0f) acuteGoertzelMinFreq,     /**< Hand-squeeze lower aligned Goertzel bin (Hz). */
    (float)(4400.0f) acuteGoertzelMaxFreq,     /**< Hand-squeeze upper aligned Goertzel bin (Hz). */
    (float)(3000.0f) acuteFastMinFreq,         /**< Hand-squeeze fast-window lower aligned bin (Hz). */
    (float)(4400.0f) acuteFastMaxFreq,         /**< Hand-squeeze fast-window upper aligned bin (Hz). */
    (float)(41.2064f) acutePMaxMin,            /**< Hand-squeeze minimum peak Goertzel power. */
    (float)(3.2371f) acuteSnrDbMin,            /**< Hand-squeeze main-window minimum SNR (dB). */
    (float)(0.7972f) acuteFlatMax,             /**< Hand-squeeze main-window maximum flatness. */
    (float)(3.6680f) acuteSnrFastMin,          /**< Hand-squeeze fast-window minimum SNR (dB). */
    (float)(0.2664f) acuteFlatFastMax,         /**< Hand-squeeze fast-window maximum flatness. */
    (float)(7.4108f) acuteFluxMax,             /**< Hand-squeeze maximum one-sided flux (dB). */
    (float)(2.3305f) acuteLowbandMax,          /**< Hand-squeeze maximum LP/whistle energy ratio. */
    (float)(0.3830f) acuteERatioMin,           /**< Hand-squeeze minimum whistle/total energy ratio. */
    (int)(3) acuteOnsetConsec,                 /**< Hand-squeeze consecutive OK frames for onset. */
    (int)(34) acuteOffFrames,                  /**< Hand-squeeze release time in 5 ms hops. */
    (int)(7) acuteGapFill,                     /**< Hand-squeeze gap-fill budget. */
    (int)(150) acuteMinDistMs,                 /**< Hand-squeeze minimum inter-event distance. */
    (float)(1.0f) acuteStationaryHoldSec,      /**< Hand-squeeze stationary-mask hold time. */

    (bool)(true) mouthWhistleEnabled,          /**< Enable the mouth-whistle profile. */
    (float)(1700.0f) mouthGoertzelMinFreq,     /**< Mouth profile lower edge (Hz). */
    (float)(4200.0f) mouthGoertzelMaxFreq,     /**< Mouth profile upper edge (Hz). */
    (float)(1800.0f) mouthFastMinFreq,         /**< Mouth fast-window lower edge (Hz). */
    (float)(4200.0f) mouthFastMaxFreq,         /**< Mouth fast-window upper edge (Hz). */
    (float)(35.0f) mouthPMaxMin,               /**< Mouth minimum peak Goertzel power. */
    (float)(6.0f) mouthSnrDbMin,               /**< Mouth main-window minimum SNR (dB). */
    (float)(0.62f) mouthFlatMax,               /**< Mouth main-window maximum flatness. */
    (float)(4.0f) mouthSnrFastMin,             /**< Mouth fast-window minimum SNR (dB). */
    (float)(0.68f) mouthFlatFastMax,           /**< Mouth fast-window maximum flatness. */
    (float)(8.5f) mouthFluxMax,                /**< Mouth maximum one-sided flux (dB). */
    (float)(3.5f) mouthLowbandMax,             /**< Mouth maximum LP/whistle energy ratio. */
    (float)(0.25f) mouthERatioMin,             /**< Mouth minimum whistle/total energy ratio. */
    (int)(4) mouthOnsetConsec,                 /**< Mouth consecutive OK frames for onset. */
    (int)(40) mouthOffFrames,                  /**< Mouth release time in 5 ms hops. */
    (int)(8) mouthGapFill,                     /**< Mouth gap-fill budget. */
    (int)(200) mouthMinDistMs,                 /**< Mouth minimum inter-event distance. */
    (float)(1.0f) mouthStationaryHoldSec,      /**< Mouth stationary-mask hold time. */

    (bool)(true) logWhistleMonitoring,         /**< Print periodic summaries of strongest candidate. */
    (bool)(true) logWhistleDetections,         /**< Print feature summaries on confirmed detections. */
    (bool)(false) logRejectedWhistleCandidates,/**< Print near-miss summaries for tuning. */
    (int)(1000) logIntervalMs,                 /**< Minimum spacing between repeated info logs. */
  }),
});

class WhistleRecognizer : public WhistleRecognizerBase
{
  static constexpr int BP_N_SOS = 4;
  static constexpr int LP_N_SOS = 2;
  static constexpr std::size_t detectorProfileCount = 3;
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

  enum class SpectrumSource
  {
    Bandpassed,
    Raw
  };

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
    bool enabled;
    SpectrumSource source;
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

    RingBuffer<float> rawBuf;
    RingBuffer<float> bpBuf;
    RingBuffer<float> lpBuf;
    std::array<DetectorState, detectorProfileCount> detectors;
  };
  std::vector<ChannelState> channelStates;

  static float applyBP(float x, double (&z)[BP_N_SOS][2]);
  static float applyLP(float x, double (&z)[LP_N_SOS][2]);

  std::array<DetectorProfile, detectorProfileCount> detectorProfiles() const;
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
