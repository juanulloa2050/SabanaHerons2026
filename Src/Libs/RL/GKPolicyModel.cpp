#include "GKPolicyModel.h"

#include "Platform/File.h"
#include "Streaming/Global.h"

#include <CompiledNN2ONNX/Model.h>

#include <exception>
#include <filesystem>

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

RLGK::GKPolicyModel::GKPolicyModel() :
  network(&Global::getAsmjitRuntime())
{}

bool RLGK::GKPolicyModel::load(const std::string& configuredModelPath, std::string* error)
{
  std::filesystem::path path(configuredModelPath);
  if(path.empty())
  {
    if(error)
      *error = "empty GK model path";
    return false;
  }
  if(!path.is_absolute())
    path = std::filesystem::path(File::getBHDir()) / path;
  if(!std::filesystem::exists(path))
  {
    if(error)
      *error = "GK model does not exist: " + path.string();
    return false;
  }

  try
  {
    network.compile(NeuralNetworkONNX::Model(path.string()));
  }
  catch(const std::exception& e)
  {
    if(error)
      *error = "GK ONNX model failed to compile: " + path.string() + " (" + e.what() + ")";
    return false;
  }
  if(!network.valid())
  {
    if(error)
      *error = "GK ONNX model failed to compile: " + path.string();
    return false;
  }

  logitsOutputIndex = -1;
  paramsOutputIndex = -1;
  combinedOutput = false;
  if(network.numOfOutputs() == 1)
  {
    const int size = flatSize(network.output(0));
    if(size < static_cast<int>(gkSkillCount + gkParamCount))
    {
      if(error)
        *error = "GK ONNX single-output tensor is too small";
      return false;
    }
    combinedOutput = true;
  }
  else
  {
    for(unsigned int i = 0; i < network.numOfOutputs(); ++i)
    {
      const int size = flatSize(network.output(i));
      if(size == static_cast<int>(gkSkillCount) && logitsOutputIndex < 0)
        logitsOutputIndex = static_cast<int>(i);
      else if(size == static_cast<int>(gkParamCount) && paramsOutputIndex < 0)
        paramsOutputIndex = static_cast<int>(i);
    }
    if(logitsOutputIndex < 0 || paramsOutputIndex < 0)
    {
      if(error)
        *error = "GK ONNX outputs do not match expected logits/params contract";
      return false;
    }
  }

  currentModelPath = path.string();
  loaded = true;
  return true;
}

bool RLGK::GKPolicyModel::infer(const std::array<float, gkObsSize>& observation, GKPolicyOutput& output, std::string* error)
{
  if(!loaded)
  {
    if(error)
      *error = "GK model not loaded";
    return false;
  }

  auto input = network.input(0);
  if(input.rank() == 2)
  {
    if(input.dims(0) != 1 || input.dims(1) != static_cast<unsigned int>(gkObsSize))
    {
      if(error)
        *error = "GK input tensor shape mismatch";
      return false;
    }
  }
  else if(input.rank() == 1)
  {
    if(input.dims(0) != static_cast<unsigned int>(gkObsSize))
    {
      if(error)
        *error = "GK input tensor shape mismatch";
      return false;
    }
  }
  else
  {
    if(error)
      *error = "Unsupported GK input rank";
    return false;
  }

  float* inputData = input.data();
  for(std::size_t i = 0; i < observation.size(); ++i)
    inputData[i] = observation[i];

  network.apply();

  if(combinedOutput)
  {
    const float* data = network.output(0).data();
    for(std::size_t i = 0; i < gkSkillCount; ++i)
      output.skillLogits[i] = data[i];
    for(std::size_t i = 0; i < gkParamCount; ++i)
      output.paramMean[i] = data[gkSkillCount + i];
  }
  else
  {
    const float* logits = network.output(static_cast<unsigned int>(logitsOutputIndex)).data();
    const float* params = network.output(static_cast<unsigned int>(paramsOutputIndex)).data();
    for(std::size_t i = 0; i < gkSkillCount; ++i)
      output.skillLogits[i] = logits[i];
    for(std::size_t i = 0; i < gkParamCount; ++i)
      output.paramMean[i] = params[i];
  }

  output.valid = true;
  return true;
}

bool RLGK::GKPolicyModel::isLoaded() const
{
  return loaded;
}

const std::string& RLGK::GKPolicyModel::modelPath() const
{
  return currentModelPath;
}
