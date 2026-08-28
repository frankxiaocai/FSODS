#include "ModelWrapper.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>


ModelWrapper::ModelWrapper()
	: env_(ORT_LOGGING_LEVEL_WARNING, "HSI_ONNX_Classifier")
{
	sessionOptions_.SetIntraOpNumThreads(1);
	sessionOptions_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
}

ModelWrapper::~ModelWrapper() = default;

#ifdef _WIN32
std::wstring ModelWrapper::widenPath(const std::string& path)
{
	// Keep model path ASCII-only for VS2017 deployment, for example:
	//     plastic_classifier.onnx
	//     D:/HSI/plastic_classifier.onnx
	return std::wstring(path.begin(), path.end());
}
#endif

bool ModelWrapper::loadModel(
	const std::string& modelPath,
	int expectedBands,
	std::string& errorMessage)
{
	loaded_ = false;
	bands_ = expectedBands;
	session_.reset();
	inputName_.clear();
	outputName_.clear();

	if (modelPath.empty())
	{
		errorMessage = "No ONNX model path provided.";
		return false;
	}

	if (expectedBands <= 0)
	{
		errorMessage = "Invalid expected band number.";
		return false;
	}

	try
	{
#ifdef _WIN32
		const std::wstring modelPathW = widenPath(modelPath);
		session_.reset(new Ort::Session(env_, modelPathW.c_str(), sessionOptions_));
#else
		session_.reset(new Ort::Session(env_, modelPath.c_str(), sessionOptions_));
#endif

		Ort::AllocatorWithDefaultOptions allocator;

		if (session_->GetInputCount() < 1 || session_->GetOutputCount() < 1)
		{
			errorMessage = "ONNX model must have at least one input and one output.";
			session_.reset();
			return false;
		}

		char* inputName = session_->GetInputName(0, allocator);
		inputName_ = (inputName != nullptr) ? inputName : "";
		allocator.Free(inputName);

		// Prefer the probability output instead of the hard-label output.
		// sklearn-onnx classifiers usually export:
		//     output 0: label
		//     output 1: probabilities
		// Unknown judgment requires the probability/score matrix.
		outputName_.clear();
		const size_t outputCount = session_->GetOutputCount();
		for (size_t i = 0; i < outputCount; ++i)
		{
			char* outputName = session_->GetOutputName(i, allocator);
			std::string candidate = (outputName != nullptr) ? outputName : "";
			allocator.Free(outputName);

			if (outputName_.empty())
			{
				outputName_ = candidate;
			}

			if (candidate == "probabilities" ||
				candidate == "probability_tensor" ||
				candidate.find("probab") != std::string::npos ||
				candidate.find("Probability") != std::string::npos)
			{
				outputName_ = candidate;
				break;
			}
		}

		if (inputName_.empty() || outputName_.empty())
		{
			errorMessage = "Failed to get ONNX input or output name.";
			session_.reset();
			return false;
		}

		loaded_ = true;
		errorMessage.clear();
		return true;
	}
	catch (const Ort::Exception& ex)
	{
		errorMessage = std::string("ONNX Runtime error: ") + ex.what();
		session_.reset();
		return false;
	}
	catch (const std::exception& ex)
	{
		errorMessage = std::string("Failed to load ONNX model: ") + ex.what();
		session_.reset();
		return false;
	}
	catch (...)
	{
		errorMessage = "Unknown error while loading ONNX model.";
		session_.reset();
		return false;
	}
}

int ModelWrapper::sanitizeLabel(int label)
{
	if (label >= LABEL_BACKGROUND && label <= LABEL_PET)
	{
		return label;
	}
	return LABEL_UNKNOWN;
}

int ModelWrapper::argmax(const float* values, int count)
{
	if (values == nullptr || count <= 0)
	{
		return 0;
	}

	int best = 0;
	float bestValue = values[0];
	for (int i = 1; i < count; ++i)
	{
		if (values[i] > bestValue)
		{
			bestValue = values[i];
			best = i;
		}
	}
	return best;
}

float ModelWrapper::confidenceFromScores(const float* values, int count, int bestIndex)
{
	if (values == nullptr || count <= 0 || bestIndex < 0 || bestIndex >= count)
	{
		return 0.0f;
	}

	// Case 1: values already look like probabilities.
	// This is common for sklearn-onnx probability output.
	double sum = 0.0;
	bool allInProbabilityRange = true;
	for (int i = 0; i < count; ++i)
	{
		const float v = values[i];
		if (v < -1e-6f || v > 1.0f + 1e-6f)
		{
			allInProbabilityRange = false;
		}
		sum += static_cast<double>(v);
	}

	if (allInProbabilityRange && sum > 0.5 && sum < 1.5)
	{
		return values[bestIndex];
	}

	// Case 2: values are logits or unnormalized scores. Convert to softmax confidence.
	float maxValue = values[0];
	for (int i = 1; i < count; ++i)
	{
		if (values[i] > maxValue)
		{
			maxValue = values[i];
		}
	}

	double expSum = 0.0;
	for (int i = 0; i < count; ++i)
	{
		expSum += std::exp(static_cast<double>(values[i] - maxValue));
	}

	if (expSum <= 0.0)
	{
		return 0.0f;
	}

	const double bestExp = std::exp(static_cast<double>(values[bestIndex] - maxValue));
	return static_cast<float>(bestExp / expSum);
}

int ModelWrapper::labelFromClassIndex(int classIndex, int classCount) const
{
	// Your current HSI system has 8 known outputs:
	// 0 Background, 1 LDPE, 2 HDPE, 3 PP, 4 PS, 5 ABS, 6 PVC, 7 PET.
	// If the ONNX output has 8 columns, the class index is directly the label.
	if (classCount == 8)
	{
		return sanitizeLabel(classIndex);
	}

	// If a future model outputs only 7 plastic classes without background,
	// map index 0..6 to label 1..7.
	if (classCount == 7)
	{
		return sanitizeLabel(classIndex + 1);
	}

	// Unsupported class count must not be mapped implicitly, because the
	// probability-column order would be ambiguous.
	return LABEL_UNKNOWN;
}

int ModelWrapper::placeholderPredictOne(const Spectrum& snvSpectrum) const
{
	if (snvSpectrum.empty())
	{
		return LABEL_UNKNOWN;
	}

	size_t maxIndex = 0;
	float maxValue = snvSpectrum[0];
	for (size_t i = 1; i < snvSpectrum.size(); ++i)
	{
		if (snvSpectrum[i] > maxValue)
		{
			maxValue = snvSpectrum[i];
			maxIndex = i;
		}
	}

	// Placeholder is only for debugging the full pipeline when the ONNX model is not loaded.
	return static_cast<int>(maxIndex % 7) + 1;
}

int ModelWrapper::predictOne(const Spectrum& snvSpectrum) const
{
	if (!loaded_ || session_ == nullptr)
	{
		return placeholderPredictOne(snvSpectrum);
	}

	std::vector<int> labels;
	std::string errorMessage;
	if (!predictBatch(snvSpectrum, 1, labels, errorMessage) || labels.empty())
	{
		return LABEL_UNKNOWN;
	}
	return labels[0];
}

bool ModelWrapper::predictBatch(const std::vector<float>& flatInput,
	int sampleCount,
	std::vector<int>& labels,
	std::string& errorMessage) const
{
	labels.clear();

	if (sampleCount <= 0)
	{
		errorMessage = "sampleCount must be positive.";
		return false;
	}

	if (bands_ <= 0)
	{
		errorMessage = "Invalid band number.";
		return false;
	}

	const size_t expectedSize = static_cast<size_t>(sampleCount) * static_cast<size_t>(bands_);
	if (flatInput.size() != expectedSize)
	{
		std::ostringstream oss;
		oss << "Input size mismatch. Expected " << expectedSize << ", got " << flatInput.size() << ".";
		errorMessage = oss.str();
		return false;
	}

	if (!loaded_ || session_ == nullptr)
	{
		labels.resize(static_cast<size_t>(sampleCount));
		for (int i = 0; i < sampleCount; ++i)
		{
			Spectrum s(flatInput.begin() + static_cast<size_t>(i) * bands_,
				flatInput.begin() + static_cast<size_t>(i + 1) * bands_);
			labels[static_cast<size_t>(i)] = placeholderPredictOne(s);
		}
		errorMessage.clear();
		return true;
	}

	try
	{
		Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
		std::vector<int64_t> inputShape;
		inputShape.push_back(static_cast<int64_t>(sampleCount));
		inputShape.push_back(static_cast<int64_t>(bands_));

		// ONNX Runtime requires non-const data pointer for CreateTensor, but it will not modify input data.
		std::vector<float> inputCopy = flatInput;
		Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
			memoryInfo,
			inputCopy.data(),
			inputCopy.size(),
			inputShape.data(),
			inputShape.size());

		const char* inputNames[] = { inputName_.c_str() };
		const char* outputNames[] = { outputName_.c_str() };

		std::vector<Ort::Value> outputTensors = session_->Run(
			Ort::RunOptions{ nullptr },
			inputNames,
			&inputTensor,
			1,
			outputNames,
			1);

		if (outputTensors.empty() || !outputTensors[0].IsTensor())
		{
			errorMessage = "ONNX output is empty or not a tensor.";
			return false;
		}

		Ort::TensorTypeAndShapeInfo outInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
		const ONNXTensorElementDataType outType = outInfo.GetElementType();
		const std::vector<int64_t> outShape = outInfo.GetShape();
		const size_t outCount = outInfo.GetElementCount();

		labels.resize(static_cast<size_t>(sampleCount), LABEL_UNKNOWN);

		if (outType == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
		{
			const float* out = outputTensors[0].GetTensorData<float>();
			if (outCount == static_cast<size_t>(sampleCount))
			{
				// Float hard-label output. No confidence is available here;
				// therefore Unknown can only be assigned when the label is outside 0..7.
				for (int i = 0; i < sampleCount; ++i)
				{
					const int label = static_cast<int>(std::round(out[i]));
					labels[static_cast<size_t>(i)] = sanitizeLabel(label);
				}
			}
			else if (outCount % static_cast<size_t>(sampleCount) == 0)
			{
				// Probability/logit/score matrix: [sampleCount, classCount].
				// This is the preferred format for pixel-level Unknown judgment.
				const int classCount = static_cast<int>(outCount / static_cast<size_t>(sampleCount));
				if (classCount != 7 && classCount != 8)
				{
					errorMessage = "Unsupported classifier class count. Expected 7 or 8.";
					return false;
				}

				for (int i = 0; i < sampleCount; ++i)
				{
					const float* row = out + static_cast<size_t>(i) * static_cast<size_t>(classCount);
					const int bestIndex = argmax(row, classCount);
					const float confidence = confidenceFromScores(row, classCount, bestIndex);

					if (unknownEnabled_ && confidence < unknownThreshold_)
					{
						labels[static_cast<size_t>(i)] = LABEL_UNKNOWN;
					}
					else
					{
						labels[static_cast<size_t>(i)] = labelFromClassIndex(bestIndex, classCount);
					}
				}
			}
			else
			{
				errorMessage = "Unsupported float output shape.";
				return false;
			}
		}
		else if (outType == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64)
		{
			const int64_t* out = outputTensors[0].GetTensorData<int64_t>();
			if (outCount < static_cast<size_t>(sampleCount))
			{
				errorMessage = "INT64 output size is smaller than sample count.";
				return false;
			}
			for (int i = 0; i < sampleCount; ++i)
			{
				// Integer output is a hard label. No confidence is available.
				labels[static_cast<size_t>(i)] = sanitizeLabel(static_cast<int>(out[i]));
			}
		}
		else if (outType == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32)
		{
			const int32_t* out = outputTensors[0].GetTensorData<int32_t>();
			if (outCount < static_cast<size_t>(sampleCount))
			{
				errorMessage = "INT32 output size is smaller than sample count.";
				return false;
			}
			for (int i = 0; i < sampleCount; ++i)
			{
				// Integer output is a hard label. No confidence is available.
				labels[static_cast<size_t>(i)] = sanitizeLabel(static_cast<int>(out[i]));
			}
		}
		else
		{
			errorMessage = "Unsupported ONNX output type. Expected float, int64, or int32.";
			return false;
		}

		errorMessage.clear();
		return true;
	}
	catch (const Ort::Exception& ex)
	{
		errorMessage = std::string("ONNX Runtime inference error: ") + ex.what();
		return false;
	}
	catch (const std::exception& ex)
	{
		errorMessage = std::string("Inference failed: ") + ex.what();
		return false;
	}
}
