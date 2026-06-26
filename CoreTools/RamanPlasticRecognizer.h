#pragma once

#include <vector>
#include <string>
#include <stdexcept>

// 拉曼塑料识别错误码
enum RamanErrorCode : int
{
	Error_None_raman = 0,

	// 输入光谱相关
	Error_InputSpectrumEmpty,
	Error_InputSpectrumSizeMismatch,
	Error_InputSpectrumTooFewPoints,
	Error_InputSpectrumInvalidValue,
	Error_InputSpectrumRangeTooSmall,
	Error_InputSpectrumDuplicateXTooMany,

	// 训练集相关
	Error_TrainDirectoryEmpty,
	Error_TrainDirectoryNotExist,
	Error_TrainDirectoryNotAccessible,
	Error_TrainCsvMissing,
	Error_TrainCsvOpenFailed,
	Error_TrainCsvOccupied,
	Error_TrainCsvReadFailed,
	Error_TrainCsvEmpty,
	Error_TrainCsvNoValidNumber,
	Error_TrainCsvFormatInvalid,
	Error_TrainCsvDimensionInvalid,
	Error_TrainCsvContainsInvalidValue,
	Error_TrainSampleEmpty,
	Error_TrainSampleDimensionMismatch,

	// 预处理 / 特征提取相关
	Error_BaselineCorrectionFailed,
	Error_LinearSystemSolveFailed,
	Error_NormalizationFailed,
	Error_NormalizationZeroRange,
	Error_InterpolationFailed,
	Error_FeatureExtractionFailed,
	Error_FeatureDimensionInvalid,

	// 分类相关
	Error_KnnKInvalid,
	Error_KnnTrainSamplesEmpty,
	Error_KnnDistanceFailed,
	Error_KnnPredictionFailed,
	Error_PredictedLabelInvalid,

	// 兜底错误
	Error_MemoryAllocationFailed,
	Error_StdException,
	Error_UnknownException
};

class  RamanPlasticRecognizer
{
public:
	RamanPlasticRecognizer();
	~RamanPlasticRecognizer();

	void setTrainDirectory(const std::string& trainDir);

	// 标准 C++ 接口
	RamanErrorCode recognition(
		const std::vector<float>& m_waveLength,
		const std::vector<float>& temp_originalSpectrum,
		int& type
	);

	std::string getLastError() const;

private:
	struct SpectrumPoint
	{
		double ramanShift;
		double intensity;
	};

	struct TrainSample
	{
		std::vector<double> feature;
		int label;
	};

	class RecognitionError : public std::runtime_error
	{
	public:
		RecognitionError(RamanErrorCode errorCode, const std::string& message)
			: std::runtime_error(message), code(errorCode)
		{
		}

		RamanErrorCode code;
	};

private:
	std::string m_trainDir;
	std::string m_lastError;

	std::vector<double> m_featureX;

	double m_alsAsym;
	double m_alsSmooth;
	int m_alsIter;
	int m_kValue;

private:
	
	double m_excitationWavelengthNm;

	double wavelengthNmToRamanShiftCm1(double wavelengthNm) const;


	void throwRecognitionError(
		RamanErrorCode errorCode,
		const std::string& message
	) const;

	std::vector<SpectrumPoint> buildSpectrumFromVector(
		const std::vector<float>& waveLength,
		const std::vector<float>& intensity
	);

	std::vector<double> extractFeaturesFromRawSpectrum(
		const std::vector<SpectrumPoint>& spectrum
	);

	std::vector<TrainSample> loadTrainingData(
		const std::string& trainDir
	);

	std::vector<std::vector<double>> readMatrixCSV(
		const std::string& filename
	);

	std::vector<double> asymmetricLeastSquaresBaseline(
		const std::vector<double>& y,
		double asym,
		double smooth,
		int iterations
	);

	std::vector<double> solveLinearSystem(
		std::vector<std::vector<double>> A,
		std::vector<double> b
	);

	std::vector<double> minMaxNormalize(
		const std::vector<double>& y
	);

	double linearInterpolate(
		const std::vector<double>& x,
		const std::vector<double>& y,
		double target
	);

	double euclideanDistance(
		const std::vector<double>& a,
		const std::vector<double>& b
	);

	int predictKNN(
		const std::vector<double>& testFeature,
		const std::vector<TrainSample>& trainSamples,
		int k
	);

	std::string removeUTF8BOM(const std::string& s);
	std::string trim(const std::string& s);
	bool isNumber(const std::string& s);
	std::vector<std::string> splitFlexible(const std::string& line);
};
