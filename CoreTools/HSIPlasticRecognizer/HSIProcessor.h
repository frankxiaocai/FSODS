#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

/*
 * Error code type.
 * The type name and every enum item use the _HSI suffix.
 */
typedef enum HSI_ErrorCode_HSI
{
    Error_None_HSI = 0,

    Error_ConnectFailed_HSI = 1,
    Error_InvalidArgument_HSI = 2,
    Error_NullPointer_HSI = 3,
    Error_Exception_HSI = 4,

    Error_FileOpenFailed_HSI = 10,
    Error_FileEmpty_HSI = 11,
    Error_FileReadFailed_HSI = 12,
    Error_FileWriteFailed_HSI = 13,

    Error_InvalidBatch_HSI = 20,
    Error_InvalidWidth_HSI = 21,
    Error_InvalidBands_HSI = 22,
    Error_InvalidLines_HSI = 23,
    Error_UnsupportedBytesPerPixel_HSI = 24,
    Error_DataSizeMismatch_HSI = 25,

    Error_ModelPathEmpty_HSI = 30,
    Error_ModelLoadFailed_HSI = 31,
    Error_ModelInputOutputInvalid_HSI = 32,
    Error_ModelInputTypeInvalid_HSI = 33,
    Error_ModelBandMismatch_HSI = 34,
    Error_ModelNotLoaded_HSI = 35,

    Error_InferenceInputInvalid_HSI = 40,
    Error_InferenceInputSizeMismatch_HSI = 41,
    Error_InferenceRunFailed_HSI = 42,
    Error_InferenceOutputEmpty_HSI = 43,
    Error_InferenceOutputTypeUnsupported_HSI = 44,
    Error_InferenceOutputShapeUnsupported_HSI = 45,
    Error_InferenceOutputSizeMismatch_HSI = 46,

    Error_ResultUnknown_HSI = 53
} error_code_HSI;

/*
 * Public input structure.
 * data must be BIL raw bytes in this order:
 *     line -> band -> pixel
 * Data type:
 *     uint16 little-endian, bytesPerPixel = 2
 */
struct HyperLineBatch
{
    std::vector<unsigned char> data;
    int width = 0;
    int bands = 0;
    int bytesPerPixel = 2;
    int requestedLines = 0;
    int receivedLines = 0;
};

class ModelWrapper;

class HSIProcessor
{
public:
    /*
     * Public input:
     *     const HyperLineBatch& batch
     *
     * Public output:
     *     int& finalLabel
     *
     * Label definition:
     *     0 Background
     *     1 LDPE
     *     2 HDPE
     *     3 PP
     *     4 PS
     *     5 ABS
     *     6 PVC
     *     7 PET
     *     8 Unknown
     *
     * Return:
     *     Error_None_HSI if the interface runs successfully.
     *     Other error_code_HSI values indicate data/model/inference errors.
     */
    HSIProcessor();
    ~HSIProcessor();

    error_code_HSI classifyFinalLabel(
        const HyperLineBatch& batch,
        int& finalLabel);

private:
    HSIProcessor(const HSIProcessor&);
    HSIProcessor& operator=(const HSIProcessor&);

private:
    typedef std::vector<float> Spectrum;
    typedef std::vector<std::vector<int> > LabelMatrix;

    struct VoteResult
    {
        int finalLabel = 8;
        int totalPixels = 0;
        int backgroundPixels = 0;
        int plasticPixels = 0;
        int unknownPixels = 0;
        int dominantPixels = 0;
        double plasticRatio = 0.0;
        double dominantRatio = 0.0;
    };

private:
    error_code_HSI ensureModelLoaded(int bands);
    error_code_HSI loadDefaultModel(int bands);
    std::string defaultModelPath() const;

    error_code_HSI validateBatch(const HyperLineBatch& batch) const;

    Spectrum getSpectrumBIL(
        const HyperLineBatch& batch,
        int row,
        int col) const;

    Spectrum snv(const Spectrum& spectrum) const;

    error_code_HSI predictPixelLabels(
        const HyperLineBatch& batch,
        LabelMatrix& labelMatrix);

    VoteResult voteFinalLabel(const LabelMatrix& labelMatrix) const;

    static std::map<int, int> countLabels(const LabelMatrix& labels);

    error_code_HSI inferenceErrorFromMessage(const std::string& message) const;

private:
    std::unique_ptr<ModelWrapper> model_;

    int blockRows_ = 256;
    int blockCols_ = 256;

    double minPlasticRatio_ = 0.05;
    double minDominantRatio_ = 0.60;

    bool unknownEnabled_ = true;
    float unknownThreshold_ = 0.60f;

    std::string defaultModelFileName_ = "plastic_classifier.onnx";
};
