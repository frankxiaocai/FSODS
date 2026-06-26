#include "HSIProcessor.h"
#include "ModelWrapper.h"

#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

HSIProcessor::HSIProcessor()
    : model_(new ModelWrapper())
{
    model_->setUnknownEnabled(unknownEnabled_);
    model_->setUnknownThreshold(unknownThreshold_);
}

HSIProcessor::~HSIProcessor() = default;

error_code_HSI HSIProcessor::classifyFinalLabel(
	const HyperLineBatch& batch,
	int& finalLabel)
{
	finalLabel = 8;

	error_code_HSI ec = validateBatch(batch);
	if (ec != Error_None_HSI)
	{
		return ec;
	}

	ec = ensureModelLoaded(batch.bands);
	if (ec != Error_None_HSI)
	{
		return ec;
	}

	LabelMatrix labelMatrix;
	ec = predictPixelLabels(batch, labelMatrix);
	if (ec != Error_None_HSI)
	{
		return ec;
	}

	const VoteResult vote = voteFinalLabel(labelMatrix);
	finalLabel = vote.finalLabel;

	return Error_None_HSI;
}

error_code_HSI HSIProcessor::ensureModelLoaded(int bands)
{
	if (bands <= 0)
	{
		return Error_InvalidBands_HSI;
	}

	if (model_ == nullptr)
	{
		return Error_ModelNotLoaded_HSI;
	}

	try
	{
		if (model_->isLoaded())
		{
			if (model_->bands() > 0 && model_->bands() != bands)
			{
				return Error_ModelBandMismatch_HSI;
			}

			return Error_None_HSI;
		}

		return loadDefaultModel(bands);
	}
	catch (const std::exception&)
	{
		return Error_Exception_HSI;
	}
	catch (...)
	{
		return Error_Exception_HSI;
	}
}

error_code_HSI HSIProcessor::loadDefaultModel(int bands)
{
	if (bands <= 0)
	{
		return Error_InvalidBands_HSI;
	}

	if (model_ == nullptr)
	{
		return Error_ModelNotLoaded_HSI;
	}

	const std::string modelPath = defaultModelPath();
	if (modelPath.empty())
	{
		return Error_ModelPathEmpty_HSI;
	}

	try
	{
		std::string errorMessage;

		if (!model_->loadModel(modelPath, bands, errorMessage))
		{
			if (errorMessage.find("band mismatch") != std::string::npos)
			{
				return Error_ModelBandMismatch_HSI;
			}

			if (errorMessage.find("input type") != std::string::npos ||
				errorMessage.find("float32") != std::string::npos)
			{
				return Error_ModelInputTypeInvalid_HSI;
			}

			if (errorMessage.find("at least one input") != std::string::npos ||
				errorMessage.find("one output") != std::string::npos)
			{
				return Error_ModelInputOutputInvalid_HSI;
			}

			return Error_ModelLoadFailed_HSI;
		}

		return Error_None_HSI;
	}
	catch (const std::exception&)
	{
		return Error_Exception_HSI;
	}
	catch (...)
	{
		return Error_Exception_HSI;
	}
}

std::string HSIProcessor::defaultModelPath() const
{
#ifdef _WIN32
    char* programPath = nullptr;
    if (_get_pgmptr(&programPath) == 0 && programPath != nullptr)
    {
        std::string path(programPath);
        const size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos)
        {
            return path.substr(0, pos + 1) + defaultModelFileName_;
        }
    }
#endif

    return defaultModelFileName_;
}

error_code_HSI HSIProcessor::validateBatch(const HyperLineBatch& batch) const
{
    if (batch.width <= 0)
    {
        return Error_InvalidWidth_HSI;
    }

    if (batch.bands <= 0)
    {
        return Error_InvalidBands_HSI;
    }

    if (batch.receivedLines <= 0)
    {
        return Error_InvalidLines_HSI;
    }

    if (batch.bytesPerPixel != 2)
    {
        return Error_UnsupportedBytesPerPixel_HSI;
    }

    const size_t expectedBytes =
        static_cast<size_t>(batch.receivedLines)
        * static_cast<size_t>(batch.bands)
        * static_cast<size_t>(batch.width)
        * static_cast<size_t>(batch.bytesPerPixel);

    if (batch.data.size() != expectedBytes)
    {
        return Error_DataSizeMismatch_HSI;
    }

    return Error_None_HSI;
}

HSIProcessor::Spectrum HSIProcessor::getSpectrumBIL(
    const HyperLineBatch& batch,
    int row,
    int col) const
{
    if (row < 0 || row >= batch.receivedLines ||
        col < 0 || col >= batch.width)
    {
        throw std::out_of_range("Pixel index out of range.");
    }

    Spectrum spectrum(static_cast<size_t>(batch.bands), 0.0f);
    const unsigned char* raw = batch.data.data();

    for (int b = 0; b < batch.bands; ++b)
    {
        const size_t byteOffset =
            (static_cast<size_t>(row) * static_cast<size_t>(batch.bands) * static_cast<size_t>(batch.width)
            + static_cast<size_t>(b) * static_cast<size_t>(batch.width)
            + static_cast<size_t>(col))
            * static_cast<size_t>(batch.bytesPerPixel);

        const uint16_t value =
            static_cast<uint16_t>(raw[byteOffset])
            | (static_cast<uint16_t>(raw[byteOffset + 1]) << 8);

        spectrum[static_cast<size_t>(b)] = static_cast<float>(value);
    }

    return spectrum;
}

HSIProcessor::Spectrum HSIProcessor::snv(const Spectrum& spectrum) const
{
    const size_t n = spectrum.size();
    Spectrum result(n, 0.0f);

    if (n == 0)
    {
        return result;
    }

    double sum = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        sum += static_cast<double>(spectrum[i]);
    }

    const double mean = sum / static_cast<double>(n);

    double varSum = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double diff = static_cast<double>(spectrum[i]) - mean;
        varSum += diff * diff;
    }

    double stdValue = 0.0;
    if (n > 1)
    {
        stdValue = std::sqrt(varSum / static_cast<double>(n - 1));
    }

    if (stdValue <= 1e-12)
    {
        stdValue = 1e-6;
    }

    for (size_t i = 0; i < n; ++i)
    {
        result[i] =
            static_cast<float>((static_cast<double>(spectrum[i]) - mean) / stdValue);
    }

    return result;
}

error_code_HSI HSIProcessor::predictPixelLabels(
    const HyperLineBatch& batch,
    LabelMatrix& labelMatrix)
{
    labelMatrix.assign(
        static_cast<size_t>(batch.receivedLines),
        std::vector<int>(static_cast<size_t>(batch.width), 0));

    for (int r0 = 0; r0 < batch.receivedLines; r0 += blockRows_)
    {
        const int rEnd = std::min(r0 + blockRows_, batch.receivedLines);

        for (int c0 = 0; c0 < batch.width; c0 += blockCols_)
        {
            const int cEnd = std::min(c0 + blockCols_, batch.width);
            const int h = rEnd - r0;
            const int w = cEnd - c0;
            const int sampleCount = h * w;

            std::vector<float> blockInput;
            blockInput.reserve(
                static_cast<size_t>(sampleCount)
                * static_cast<size_t>(batch.bands));

            for (int r = r0; r < rEnd; ++r)
            {
                for (int c = c0; c < cEnd; ++c)
                {
                    const Spectrum spectrum = getSpectrumBIL(batch, r, c);
                    const Spectrum spectrumSnv = snv(spectrum);

                    blockInput.insert(
                        blockInput.end(),
                        spectrumSnv.begin(),
                        spectrumSnv.end());
                }
            }

            std::vector<int> blockLabels;
            std::string predError;
            if (!model_->predictBatch(blockInput, sampleCount, blockLabels, predError))
            {
                return inferenceErrorFromMessage(predError);
            }

            int idx = 0;
            for (int r = r0; r < rEnd; ++r)
            {
                for (int c = c0; c < cEnd; ++c)
                {
                    int label = blockLabels[static_cast<size_t>(idx++)];

                    if (label < 0 || label > 8)
                    {
                        label = 8;
                    }

                    labelMatrix[static_cast<size_t>(r)][static_cast<size_t>(c)] = label;
                }
            }
        }
    }

    return Error_None_HSI;
}

HSIProcessor::VoteResult HSIProcessor::voteFinalLabel(
    const LabelMatrix& labelMatrix) const
{
    VoteResult result;
    result.finalLabel = 8;

    const std::map<int, int> counts = countLabels(labelMatrix);

    for (size_t r = 0; r < labelMatrix.size(); ++r)
    {
        for (size_t c = 0; c < labelMatrix[r].size(); ++c)
        {
            const int label = labelMatrix[r][c];
            result.totalPixels++;

            if (label == 0)
            {
                result.backgroundPixels++;
            }
            else if (label >= 1 && label <= 7)
            {
                result.plasticPixels++;
            }
            else
            {
                result.unknownPixels++;
            }
        }
    }

    if (result.totalPixels <= 0 || result.plasticPixels <= 0)
    {
        result.finalLabel = 8;
        return result;
    }

    result.plasticRatio =
        static_cast<double>(result.plasticPixels)
        / static_cast<double>(result.totalPixels);

    int bestLabel = 1;
    int bestCount = 0;

    for (int label = 1; label <= 7; ++label)
    {
        std::map<int, int>::const_iterator it = counts.find(label);
        const int count = (it == counts.end()) ? 0 : it->second;

        if (count > bestCount)
        {
            bestCount = count;
            bestLabel = label;
        }
    }

    result.dominantPixels = bestCount;
    result.dominantRatio =
        static_cast<double>(bestCount)
        / static_cast<double>(result.plasticPixels);

    if (result.plasticRatio < minPlasticRatio_)
    {
        result.finalLabel = 8;
        return result;
    }

    if (result.dominantRatio < minDominantRatio_)
    {
        result.finalLabel = 8;
        return result;
    }

    result.finalLabel = bestLabel;
    return result;
}

std::map<int, int> HSIProcessor::countLabels(const LabelMatrix& labels)
{
    std::map<int, int> counts;

    for (size_t r = 0; r < labels.size(); ++r)
    {
        for (size_t c = 0; c < labels[r].size(); ++c)
        {
            counts[labels[r][c]]++;
        }
    }

    return counts;
}

error_code_HSI HSIProcessor::inferenceErrorFromMessage(
    const std::string& message) const
{
    if (message.find("Input size mismatch") != std::string::npos)
    {
        return Error_InferenceInputSizeMismatch_HSI;
    }

    if (message.find("empty") != std::string::npos ||
        message.find("not a tensor") != std::string::npos)
    {
        return Error_InferenceOutputEmpty_HSI;
    }

    if (message.find("Unsupported") != std::string::npos &&
        message.find("type") != std::string::npos)
    {
        return Error_InferenceOutputTypeUnsupported_HSI;
    }

    if (message.find("shape") != std::string::npos)
    {
        return Error_InferenceOutputShapeUnsupported_HSI;
    }

    if (message.find("smaller than sample count") != std::string::npos)
    {
        return Error_InferenceOutputSizeMismatch_HSI;
    }

    return Error_InferenceRunFailed_HSI;
}
