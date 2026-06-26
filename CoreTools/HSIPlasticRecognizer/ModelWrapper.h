#pragma once

#include <vector>
#include <string>
#include <memory>

#include <onnxruntime_cxx_api.h>

class ModelWrapper
{
public:
	using Spectrum = std::vector<float>;

	// Label definition used by the HSI classifier.
	static const int LABEL_BACKGROUND = 0;
	static const int LABEL_LDPE = 1;
	static const int LABEL_HDPE = 2;
	static const int LABEL_PP = 3;
	static const int LABEL_PS = 4;
	static const int LABEL_ABS = 5;
	static const int LABEL_PVC = 6;
	static const int LABEL_PET = 7;
	static const int LABEL_UNKNOWN = 8;

public:
	ModelWrapper();
	~ModelWrapper();

	bool loadModel(const std::string& modelPath, int expectedBands, std::string& errorMessage);

	// Single-pixel prediction. This is convenient but slower than predictBatch().
	int predictOne(const Spectrum& snvSpectrum) const;

	// Batch prediction. flatInput is stored as [sample0_band0 ... sample0_bandB-1, sample1_band0 ...].
	bool predictBatch(const std::vector<float>& flatInput,
		int sampleCount,
		std::vector<int>& labels,
		std::string& errorMessage) const;

	bool isLoaded() const { return loaded_; }
	int bands() const { return bands_; }
	const std::string& inputName() const { return inputName_; }
	const std::string& outputName() const { return outputName_; }

	// Unknown judgment for probability/score output.
	// If enabled and the maximum confidence is lower than unknownThreshold_, the pixel label is set to 8 (Unknown).
	void setUnknownEnabled(bool enabled) { unknownEnabled_ = enabled; }
	bool unknownEnabled() const { return unknownEnabled_; }

	void setUnknownThreshold(float threshold) { unknownThreshold_ = threshold; }
	float unknownThreshold() const { return unknownThreshold_; }

private:
	int placeholderPredictOne(const Spectrum& snvSpectrum) const;
	static int argmax(const float* values, int count);
	static int sanitizeLabel(int label);
	static float confidenceFromScores(const float* values, int count, int bestIndex);
	int labelFromClassIndex(int classIndex, int classCount) const;

#ifdef _WIN32
	static std::wstring widenPath(const std::string& path);
#endif

private:
	bool loaded_ = false;
	int bands_ = 0;

	// Default: if the model outputs a probability/score vector and the maximum confidence is lower than 0.60,
	// the pixel is regarded as Unknown. You can adjust it according to validation data.
	bool unknownEnabled_ = true;
	float unknownThreshold_ = 0.60f;

	Ort::Env env_;
	Ort::SessionOptions sessionOptions_;
	std::unique_ptr<Ort::Session> session_;

	std::string inputName_;
	std::string outputName_;
};
