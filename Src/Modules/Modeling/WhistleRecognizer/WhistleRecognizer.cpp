/**
 * @file WhistleRecognizer.cpp
 *
 * This file implements a module that identifies the sound of a whistle using
 * a Goertzel-based analysis (no template signatures).
 */

#include "WhistleRecognizer.h"
#include "Platform/SystemCall.h"
#include "Debugging/Annotation.h"
#include "Debugging/Plot.h"

#include <algorithm>
#include <iostream>
#include <cmath>

MAKE_MODULE(WhistleRecognizer);

WhistleRecognizer::WhistleRecognizer()
{
  // No FFTW / signature initialization needed anymore.
}

WhistleRecognizer::~WhistleRecognizer() = default;

// Goertzel algorithm implementation
WhistleRecognizer::GoertzelResult WhistleRecognizer::goertzelAnalyze(const RingBuffer<AudioData::Sample>& buffer)
{
  const float f_min = goertzelMinFreq; // Hz - whistle frequency range
  const float f_max = goertzelMaxFreq; // Hz
  const int N = static_cast<int>(buffer.size());
  const float fs = static_cast<float>(this->sampleRate);

  std::cout << "DEBUG: Goertzel params - N=" << N << ", fs=" << fs
            << ", f_range=[" << f_min << "-" << f_max << "]" << std::endl;

  // Calculate frequency bins in the whistle range
  const int k_min = static_cast<int>(std::ceil(f_min * N / fs));
  const int k_max = static_cast<int>(std::floor(f_max * N / fs));

  std::cout << "DEBUG: Goertzel bins - k_min=" << k_min << ", k_max=" << k_max
            << ", total_bins=" << (k_max - k_min + 1) << std::endl;

  // Check volume of buffer first
  float max_sample = 0.0f;
  float rms = 0.0f;
  for(size_t i = 0; i < buffer.size(); ++i)
  {
    const float sample = std::abs(static_cast<float>(buffer[i]));
    max_sample = std::max(max_sample, sample);
    rms += sample * sample;
  }
  rms = std::sqrt(rms / buffer.size());

  std::cout << "DEBUG: Audio levels - max_sample=" << max_sample
            << ", rms=" << rms
            << ", minVolume=" << minVolume
            << ", volume_check=" << (max_sample >= minVolume ? "PASS" : "FAIL") << std::endl;

  // Use a more permissive volume threshold for Goertzel (1% of max range instead of 20%)
  const float goertzel_min_volume = 0.01f;
  if(max_sample < goertzel_min_volume)
  {
    std::cout << "DEBUG: Volume too low for Goertzel analysis (" << max_sample << " < " << goertzel_min_volume << ")" << std::endl;
    GoertzelResult empty_result;
    empty_result.power = 0.0f;
    empty_result.frequency = 0.0f;
    empty_result.snr_db = 0.0f;
    empty_result.spectral_flatness = 1.0f;
    empty_result.bandwidth_hz = 0.0f;
    return empty_result;
  }

  std::vector<float> powers;
  std::vector<float> frequencies;

  // Compute Goertzel for each frequency bin
  for(int k = k_min; k <= k_max; ++k)
  {
    const float w = 2.0f * static_cast<float>(M_PI) * k / N;
    const float coeff = 2.0f * std::cos(w);

    float q1 = 0.0f, q2 = 0.0f;

    // Process all samples in buffer
    for(size_t i = 0; i < buffer.size(); ++i)
    {
      const float sample = static_cast<float>(buffer[i]);
      const float q0 = coeff * q1 - q2 + sample;
      q2 = q1;
      q1 = q0;
    }

    // Calculate power for this frequency bin
    const float power = q1 * q1 + q2 * q2 - q1 * q2 * coeff;
    powers.push_back(power);
    frequencies.push_back(k * fs / N);
  }

  GoertzelResult result;

  if(powers.empty())
  {
    result.power = 0.0f;
    result.frequency = 0.0f;
    result.snr_db = 0.0f;
    result.spectral_flatness = 1.0f;
    result.bandwidth_hz = 0.0f;
    return result;
  }

  // Find peak and show top frequencies
  const auto max_it = std::max_element(powers.begin(), powers.end());
  const int peak_idx = static_cast<int>(max_it - powers.begin());

  result.power = *max_it;
  result.frequency = frequencies[peak_idx];

  // Show top 5 frequencies for debugging
  std::vector<std::pair<float, float>> freq_power_pairs;
  for(size_t i = 0; i < powers.size(); ++i)
    freq_power_pairs.emplace_back(frequencies[i], powers[i]);

  std::sort(freq_power_pairs.begin(), freq_power_pairs.end(),
            [](const auto& a, const auto& b) {return a.second > b.second; });

  std::cout << "DEBUG: Top 5 frequencies found:" << std::endl;
  for(int i = 0; i < std::min(5, static_cast<int>(freq_power_pairs.size())); ++i)
  {
    std::cout << "  " << (i + 1) << ". " << freq_power_pairs[i].first << "Hz -> "
              << freq_power_pairs[i].second << " power" << std::endl;
  }

  // Calculate SNR (peak vs average of rest)
  float sum_rest = 0.0f;
  int count_rest = 0;

  for(int i = 0; i < static_cast<int>(powers.size()); ++i)
  {
    if(std::abs(i - peak_idx) > 1) // Exclude peak and immediate neighbors
    {
      sum_rest += powers[i];
      count_rest++;
    }
  }

  const float avg_rest = (count_rest > 0) ? sum_rest / count_rest : result.power * 0.1f;
  result.snr_db = 10.0f * std::log10((result.power + 1e-12f) / (avg_rest + 1e-12f));

  // Calculate spectral flatness (geometric mean / arithmetic mean)
  float geometric_sum = 0.0f;
  float arithmetic_sum = 0.0f;

  for(float power : powers)
  {
    const float safe_power = power + 1e-12f;
    geometric_sum += std::log(safe_power);
    arithmetic_sum += safe_power;
  }

  const float geometric_mean = std::exp(geometric_sum / powers.size());
  const float arithmetic_mean = arithmetic_sum / powers.size();
  result.spectral_flatness = geometric_mean / arithmetic_mean;

  // Calculate bandwidth at -3dB (half power)
  const float half_power = result.power * 0.5f;
  int left_idx = peak_idx, right_idx = peak_idx;

  // Find left boundary
  while(left_idx > 0 && powers[left_idx - 1] >= half_power)
    left_idx--;

  // Find right boundary
  while(right_idx < static_cast<int>(powers.size() - 1) && powers[right_idx + 1] >= half_power)
    right_idx++;

  result.bandwidth_hz = (right_idx - left_idx + 1) * (fs / N);

  std::cout << "DEBUG: Goertzel final result - Peak: " << result.frequency << "Hz, "
            << "Power: " << result.power << ", SNR: " << result.snr_db << "dB, "
            << "Flatness: " << result.spectral_flatness << ", BW: " << result.bandwidth_hz << "Hz" << std::endl;

  return result;
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
    std::cout << "WhistleRecognizer: Starting whistle detection - buffers cleared and recording enabled" << std::endl;
  }
  hasRecorded = shouldRecord;

  // Adapt number of channels to audio data.
  buffers.resize(theAudioData.channels);
  for(auto& buffer : buffers)
    buffer.reserve(bufferSize);

  // Append current samples to buffers and sample down if necessary
  ASSERT(theAudioData.sampleRate % sampleRate == 0);
  const size_t stepSize = theAudioData.sampleRate / sampleRate * theAudioData.channels;
  for(; sampleIndex < theAudioData.samples.size(); sampleIndex += stepSize)
  {
    --samplesRequired;
    for(size_t channel = 0; channel < theAudioData.channels; ++channel)
      buffers[channel].push_front(theAudioData.samples[sampleIndex + channel]);
  }
  sampleIndex -= theAudioData.samples.size();

  // Compute first channel index to access damage configuration.
  const int firstBuffer = theDamageConfigurationHead.audioChannelsDefect[0] ? 1 : 0;

  // No whistles can be detected while sound is playing.
  if(soundWasPlaying)
    theWhistle.channelsUsedForWhistleDetection = 0;

  // Count number of channels if they were set to zero and no sound is playing.
  if(!theWhistle.channelsUsedForWhistleDetection && !soundWasPlaying)
    for(size_t i = 0; i < buffers.size(); ++i)
      if(!theDamageConfigurationHead.audioChannelsDefect[i])
        ++theWhistle.channelsUsedForWhistleDetection;

  // Plot input samples for debugging
  for(size_t i = 0; i < buffers.size(); ++i)
    if(!buffers[i].empty())
      switch(i)
      {
        case 0: PLOT("module:WhistleRecognizer:samples0", buffers[i].back()); break;
        case 1: PLOT("module:WhistleRecognizer:samples1", buffers[i].back()); break;
        case 2: PLOT("module:WhistleRecognizer:samples2", buffers[i].back()); break;
        case 3: PLOT("module:WhistleRecognizer:samples3", buffers[i].back()); break;
      }

  // Analyze all channels with Goertzel
  if(shouldRecord && buffers[firstBuffer].full() && samplesRequired <= 0)
  {
    std::cout << "DEBUG: Starting correlation analysis - shouldRecord=" << shouldRecord
              << ", bufferFull=" << buffers[firstBuffer].full()
              << ", samplesRequired=" << samplesRequired
              << ", bufferSize=" << buffers[firstBuffer].size() << std::endl;

    float correlation = 0.f;
    size_t defects = 0;

    for(size_t i = 0; i < buffers.size(); ++i)
    {
      if(theDamageConfigurationHead.audioChannelsDefect[i] || !buffers[i].full())
      {
        ++defects;
      }
      else
      {
        // Use Goertzel algorithm instead of FFT correlation
        const GoertzelResult result = goertzelAnalyze(buffers[i]);

        std::cout << "DEBUG: Goertzel raw results - Freq: " << result.frequency << "Hz, "
                  << "Power: " << result.power << ", "
                  << "SNR: " << result.snr_db << "dB, "
                  << "Flat: " << result.spectral_flatness << ", "
                  << "BW: " << result.bandwidth_hz << "Hz" << std::endl;

        // Calculate confidence based on Goertzel metrics
        float channelCorrelation = 0.0f;

        // Check if frequency is in whistle range
        std::cout << "DEBUG: Frequency check - freq=" << result.frequency
                  << ", range=[" << goertzelMinFreq << "-" << goertzelMaxFreq << "]" << std::endl;

        if(result.frequency >= goertzelMinFreq && result.frequency <= goertzelMaxFreq)
        {
          // Combine SNR, spectral flatness and bandwidth for correlation score (more permissive thresholds)
          const float snr_score = std::max(0.0f, std::min(1.0f, (result.snr_db - 3.0f) / 15.0f)); // Lowered from 10dB to 3dB
          const float flat_score = std::max(0.0f, 1.0f - result.spectral_flatness / 0.8f); // Raised from 0.32 to 0.8
          const float bw_score = std::max(0.0f, 1.0f - result.bandwidth_hz / goertzelMaxBandwidth);

          channelCorrelation = snr_score * flat_score * bw_score;

          std::cout << "DEBUG: Score components:" << std::endl;
          std::cout << "  SNR: " << result.snr_db << "dB -> score=" << snr_score << " (threshold: 3dB)" << std::endl;
          std::cout << "  Flatness: " << result.spectral_flatness << " -> score=" << flat_score << " (max: 0.8)" << std::endl;
          std::cout << "  Bandwidth: " << result.bandwidth_hz << "Hz -> score=" << bw_score << " (max: " << goertzelMaxBandwidth << "Hz)" << std::endl;
          std::cout << "  Final correlation: " << channelCorrelation << std::endl;

          // Only log if correlation is significant
          if(channelCorrelation > 0.1f) // Lowered threshold for debugging
          {
            std::cout << "POTENTIAL WHISTLE - Freq: " << result.frequency << "Hz, "
                      << "SNR: " << result.snr_db << "dB, "
                      << "Flat: " << result.spectral_flatness << ", "
                      << "BW: " << result.bandwidth_hz << "Hz, "
                      << "Correlation: " << channelCorrelation << std::endl;
          }
        }
        else
        {
          std::cout << "DEBUG: Frequency out of range - " << result.frequency << "Hz not in ["
                    << goertzelMinFreq << "-" << goertzelMaxFreq << "]" << std::endl;
        }

        correlation += channelCorrelation;
      }
    }

    if(defects < buffers.size())
    {
      // Normalize correlation by number of valid channels
      const int validChannels = static_cast<int>(buffers.size() - defects);
      correlation /= static_cast<float>(validChannels);

      std::cout << "DEBUG: Final correlation check - correlation=" << correlation
                << ", minCorrelation=" << minCorrelation
                << ", bestCorrelation=" << bestCorrelation << std::endl;

      // Apply minimum correlation threshold
      if(correlation >= minCorrelation && correlation >= bestCorrelation)
      {
        theWhistle.confidenceOfLastWhistleDetection = correlation;
        theWhistle.channelsUsedForWhistleDetection = static_cast<unsigned char>(validChannels);
        bestCorrelation = correlation;

        if(theFrameInfo.getTimeSince(lastTimeWhistleDetected) > minAnnotationDelay)
        {
          ANNOTATION("WhistleRecognizer", "whistle with " << static_cast<int>(bestCorrelation * 100.f) << "%");
          std::cout << "WHISTLE DETECTED! Confidence: " << static_cast<int>(bestCorrelation * 100.f) << "%"
                    << ", Channels used: " << static_cast<int>(theWhistle.channelsUsedForWhistleDetection) << std::endl;
        }
        lastTimeWhistleDetected = theFrameInfo.time;
      }
      else
      {
        std::cout << "DEBUG: Correlation below threshold or not better than current best" << std::endl;
      }

      // Use correlation0 plot for the Goertzel-based score
      PLOT("module:WhistleRecognizer:correlation0", correlation);
    }

    samplesRequired = static_cast<unsigned>(bufferSize * newSampleRatio);
  }

  // Publish whistle detection and reset best correlation after accumulation phase.
  if(theFrameInfo.getTimeSince(lastTimeWhistleDetected) >= accumulationDuration)
  {
    if(theWhistle.lastTimeWhistleDetected != lastTimeWhistleDetected)
    {
      std::cout << "WHISTLE PUBLISHED! Detection finalized after accumulation period. "
                << "Final confidence: " << static_cast<int>(theWhistle.confidenceOfLastWhistleDetection * 100.f) << "%" << std::endl;
    }
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
