/**
 * @file WhistleRecognizer.h
 *
 * This file declares a module that identifies the sound of a whistle.
 *
 * Original authors:
 *  - Tim Laue
 *  - Dennis Schuethe
 *  - Thomas Röfer
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
    (std::vector<std::string>) whistles, /**< Legacy parameter: base names of reference whistles (no longer used). */
    (unsigned) bufferSize, /**< The number of samples buffered per channel. */
    (unsigned) sampleRate, /**< The sample rate actually used. */
    (float) newSampleRatio, /**< The ratio of new samples buffered before recognition is tried again (0..1). */
    (float) minVolume, /**< The minimum volume that must be reached for accepting a whistle [0..1). */
    (float) minCorrelation, /**< Minimum correlation/score accepted for whistle detection ]0..1]. */
    (int) accumulationDuration, /**< The duration over which detections are accumulated before being reported. */
    (int) minAnnotationDelay, /**< The minimum time between annotations announcing a detected whistle. */
    (bool) mute, /**< Deactivate sound output in game states in which a whistle could be detected. */
    (float)(3600.0f) goertzelMinFreq, /**< Minimum frequency for Goertzel whistle detection (Hz). */
    (float)(4500.0f) goertzelMaxFreq, /**< Maximum frequency for Goertzel whistle detection (Hz). */
    (float)(3.0f) goertzelMinSNR, /**< Minimum SNR threshold for Goertzel detection (dB). (Currently used in scoring logic thresholds.) */
    (float)(0.8f) goertzelMaxFlatness, /**< Maximum spectral flatness for whistle detection. */
    (float)(120.0f) goertzelMaxBandwidth, /**< Maximum bandwidth for whistle detection (Hz). */
  }),
});

class WhistleRecognizer : public WhistleRecognizerBase
{
  std::vector<RingBuffer<AudioData::Sample>> buffers; /**< Sample buffers for all channels. */
  bool soundWasPlaying = false; /**< Was sound played back recently? */
  bool hasRecorded = false; /**< Was audio recorded in the previous cycle? */
  int samplesRequired = 0; /**< The number of new samples required. */
  size_t sampleIndex = 0; /**< Index of next sample to process for subsampling. */
  float bestCorrelation = 0.0f; /**< The best correlation/score of the last accumulation phase. */
  unsigned lastTimeWhistleDetected = 0; /**< The last time a whistle was detected. */

  /**
   * This method is called when the representation provided needs to be updated.
   * @param theWhistle The representation updated.
   */
  void update(Whistle& theWhistle) override;

  // Goertzel algorithm structures and methods
  struct GoertzelResult
  {
    float power = 0.0f;
    float frequency = 0.0f;
    float snr_db = 0.0f;
    float spectral_flatness = 1.0f;
    float bandwidth_hz = 0.0f;
  };

  /**
   * Analyze audio buffer using Goertzel algorithm for whistle detection.
   * @param buffer The audio samples to analyze.
   * @return GoertzelResult with power, frequency, SNR and other metrics.
   */
  GoertzelResult goertzelAnalyze(const RingBuffer<AudioData::Sample>& buffer);

public:
  WhistleRecognizer();
  ~WhistleRecognizer() override;
};
