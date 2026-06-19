#include "PPOPolicyModel.h"

#include "Platform/File.h"
#include "Streaming/Global.h"

#include <CompiledNN2ONNX/Model.h>

#include <exception>
#include <filesystem>
#include <sstream>

namespace
{
  int flatSize(const NeuralNetworkONNX::CompiledNN::Tensor& tensor)
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

  try
  {
    network.compile(NeuralNetworkONNX::Model(path.string()));
  }
  catch(const std::exception& e)
  {
    if(error)
      *error = "PPO ONNX model failed to compile: " + path.string() + " (" + e.what() + ")";
    return false;
  }
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

bool RL::PPOPolicyModel::inferFromBuffer(const float* data, const std::size_t size, PPOPolicyOutput& output, std::string* error)
{
  if(!loaded)
  {
    if(error)
      *error = "PPO model not loaded";
    return false;
  }

  auto input = network.input(0);
  std::size_t expectedSize = 0;
  if(input.rank() == 2)
  {
    if(input.dims(0) != 1)
    {
      if(error)
        *error = "PPO input tensor batch dim mismatch";
      return false;
    }
    expectedSize = static_cast<std::size_t>(input.dims(1));
  }
  else if(input.rank() == 1)
  {
    expectedSize = static_cast<std::size_t>(input.dims(0));
  }
  else
  {
    if(error)
      *error = "Unsupported PPO input rank";
    return false;
  }

  if(size != expectedSize)
  {
    if(error)
      *error = "PPO input tensor size mismatch (got " + std::to_string(size) + ", expected " + std::to_string(expectedSize) + ")";
    return false;
  }

  float* inputData = input.data();
  for(std::size_t i = 0; i < size; ++i)
    inputData[i] = data[i];

  network.apply();

  if(combinedOutput)
  {
    const float* outData = network.output(0).data();
    for(std::size_t i = 0; i < ppoSkillCount; ++i)
      output.skillLogits[i] = outData[i];
    for(std::size_t i = 0; i < ppoParamCount; ++i)
      output.paramMean[i] = outData[ppoSkillCount + i];
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

bool RL::PPOPolicyModel::infer(const std::array<float, ppoObsSize>& observation, PPOPolicyOutput& output, std::string* error)
{
  return inferFromBuffer(observation.data(), observation.size(), output, error);
}

bool RL::PPOPolicyModel::infer(const std::array<float, ppoObsSize47>& observation, PPOPolicyOutput& output, std::string* error)
{
  return inferFromBuffer(observation.data(), observation.size(), output, error);
}

bool RL::PPOPolicyModel::isLoaded() const
{
  return loaded;
}

const std::string& RL::PPOPolicyModel::modelPath() const
{
  return currentModelPath;
}
