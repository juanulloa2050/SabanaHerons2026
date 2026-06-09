#include "PPOPolicyModel.h"

#include "Platform/File.h"
#include "Streaming/Global.h"

#include <CompiledNN2ONNX/Model.h>

#include <filesystem>
#include <sstream>

namespace
{
  int flatSize(const TensorXf& tensor)
  {
    int result = 1;
    for(unsigned int i = 0; i < tensor.rank(); ++i)
      result *= static_cast<int>(tensor.dims(i));
    return result;
  }
}

RL::PPOPolicyModel::PPOPolicyModel() :
  network(&Global::getAsmjitRuntime())
{}

bool RL::PPOPolicyModel::load(const std::string& configuredModelPath, std::string* error)
{
  std::filesystem::path path(configuredModelPath);
  if(path.empty())
  {
    if(error)
      *error = "empty PPO model path";
    return false;
  }
  if(!path.is_absolute())
    path = std::filesystem::path(File::getBHDir()) / path;
  if(!std::filesystem::exists(path))
  {
    if(error)
      *error = "PPO model does not exist: " + path.string();
    return false;
  }

  network.compile(NeuralNetworkONNX::Model(path.string()));
  if(!network.valid())
  {
    if(error)
      *error = "PPO ONNX model failed to compile: " + path.string();
    return false;
  }

  logitsOutputIndex = -1;
  paramsOutputIndex = -1;
  combinedOutput = false;
  if(network.numOfOutputs() == 1)
  {
    const int size = flatSize(network.output(0));
    if(size < static_cast<int>(ppoSkillCount + ppoParamCount))
    {
      if(error)
        *error = "PPO ONNX single-output tensor is too small";
      return false;
    }
    combinedOutput = true;
  }
  else
  {
    for(unsigned int i = 0; i < network.numOfOutputs(); ++i)
    {
      const int size = flatSize(network.output(i));
      if(size == static_cast<int>(ppoSkillCount) && logitsOutputIndex < 0)
        logitsOutputIndex = static_cast<int>(i);
      else if(size == static_cast<int>(ppoParamCount) && paramsOutputIndex < 0)
        paramsOutputIndex = static_cast<int>(i);
    }
    if(logitsOutputIndex < 0 || paramsOutputIndex < 0)
    {
      if(error)
        *error = "PPO ONNX outputs do not match expected logits/params contract";
      return false;
    }
  }

  currentModelPath = path.string();
  loaded = true;
  return true;
}

bool RL::PPOPolicyModel::infer(const std::array<float, ppoObsSize>& observation, PPOPolicyOutput& output, std::string* error)
{
  if(!loaded)
  {
    if(error)
      *error = "PPO model not loaded";
    return false;
  }

  TensorXf& input = network.input(0);
  if(input.rank() == 2)
  {
    if(input.dims(0) != 1 || input.dims(1) != static_cast<unsigned int>(ppoObsSize))
    {
      if(error)
        *error = "PPO input tensor shape mismatch";
      return false;
    }
  }
  else if(input.rank() == 1)
  {
    if(input.dims(0) != static_cast<unsigned int>(ppoObsSize))
    {
      if(error)
        *error = "PPO input tensor shape mismatch";
      return false;
    }
  }
  else
  {
    if(error)
      *error = "Unsupported PPO input rank";
    return false;
  }

  float* inputData = input.data();
  for(std::size_t i = 0; i < observation.size(); ++i)
    inputData[i] = observation[i];

  network.apply();

  if(combinedOutput)
  {
    const float* data = network.output(0).data();
    for(std::size_t i = 0; i < ppoSkillCount; ++i)
      output.skillLogits[i] = data[i];
    for(std::size_t i = 0; i < ppoParamCount; ++i)
      output.paramMean[i] = data[ppoSkillCount + i];
  }
  else
  {
    const float* logits = network.output(static_cast<unsigned int>(logitsOutputIndex)).data();
    const float* params = network.output(static_cast<unsigned int>(paramsOutputIndex)).data();
    for(std::size_t i = 0; i < ppoSkillCount; ++i)
      output.skillLogits[i] = logits[i];
    for(std::size_t i = 0; i < ppoParamCount; ++i)
      output.paramMean[i] = params[i];
  }

  output.valid = true;
  return true;
}

bool RL::PPOPolicyModel::isLoaded() const
{
  return loaded;
}

const std::string& RL::PPOPolicyModel::modelPath() const
{
  return currentModelPath;
}
