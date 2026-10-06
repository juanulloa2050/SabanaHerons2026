#pragma once

#include "PPOCommon.h"

#include <CompiledNN2ONNX/CompiledNN.h>

#include <string>

namespace RL
{
  class PPOPolicyModel
  {
  public:
    PPOPolicyModel();

    bool load(const std::string& modelPath, std::string* error = nullptr);
    bool infer(const std::array<float, ppoObsSize>& observation, PPOPolicyOutput& output, std::string* error = nullptr);
    bool infer(const std::array<float, ppoObsSize47>& observation, PPOPolicyOutput& output, std::string* error = nullptr);
    bool isLoaded() const;
    const std::string& modelPath() const;

  private:
    bool inferFromBuffer(const float* data, std::size_t size, PPOPolicyOutput& output, std::string* error);

  private:
    NeuralNetworkONNX::CompiledNN network;
    std::string currentModelPath;
    bool loaded = false;
    int logitsOutputIndex = -1;
    int paramsOutputIndex = -1;
    bool combinedOutput = false;
  };
}
