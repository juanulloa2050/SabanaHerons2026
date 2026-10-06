#pragma once

#include "GKCommon.h"

#include <CompiledNN2ONNX/CompiledNN.h>

#include <string>

namespace RLGK
{
  // ONNX inference for the goalkeeper policy. Mirrors RL::PPOPolicyModel but for
  // the 64-dim / 12-skill / 4-param GK contract. Outputs: skill_logits[12] + param_mean[4]
  // (two outputs, or a single combined [16] tensor).
  class GKPolicyModel
  {
  public:
    GKPolicyModel();

    bool load(const std::string& modelPath, std::string* error = nullptr);
    bool infer(const std::array<float, gkObsSize>& observation, GKPolicyOutput& output, std::string* error = nullptr);
    bool isLoaded() const;
    const std::string& modelPath() const;

  private:
    NeuralNetworkONNX::CompiledNN network;
    std::string currentModelPath;
    bool loaded = false;
    int logitsOutputIndex = -1;
    int paramsOutputIndex = -1;
    bool combinedOutput = false;
  };
}
