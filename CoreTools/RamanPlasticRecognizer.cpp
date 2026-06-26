#include "RamanPlasticRecognizer.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <iomanip>
#include <new>
#include <sys/stat.h>
#include <vector>
#include <string>

#ifdef _WIN32
#include <sys/types.h>
#endif

using namespace std;

// ============================================================
// exe 测试配置区
// ============================================================

static const string TEST_FILE =
R"(F:\KeTiZuProjects\C++\Raman_pastic_project_main\Raman_plastic_recgnition\spectrum_test.csv)";

static const string TRAIN_DIR =
R"(F:\KeTiZuProjects\C++\Raman_pastic_project_main\Raman_plastic_recgnition\train_csv)";

static const vector<string> CLASS_NAMES =
{
	"未知", // 0
	"LDPE", // 1
	"HDPE", // 2
	"PP",   // 3
	"PS",   // 4
	"ABS",  // 5
	"PVC",  // 6
	"PET"   // 7
};


// ============================================================
// 本地路径工具
// ============================================================

static bool pathExists(const string& path)
{
	struct stat info;
	return stat(path.c_str(), &info) == 0;
}

static bool pathIsDirectory(const string& path)
{
	struct stat info;

	if (stat(path.c_str(), &info) != 0)
	{
		return false;
	}

	return (info.st_mode & S_IFDIR) != 0;
}

static bool pathIsRegularFile(const string& path)
{
	struct stat info;

	if (stat(path.c_str(), &info) != 0)
	{
		return false;
	}

	return (info.st_mode & S_IFREG) != 0;
}

// ============================================================
// 构造 / 析构
// ============================================================

RamanPlasticRecognizer::RamanPlasticRecognizer()
{
	m_featureX =
	{
		809, 812, 842, 857, 919, 1002, 1064, 1082, 1130, 1154,
		1296, 1324, 1331, 1362, 1414, 1430, 1442, 1447, 1453,
		1460, 1463, 1615, 1727
	};

	m_alsAsym = 0.0001;
	m_alsSmooth = 1e5;
	m_alsIter = 10;
	m_kValue = 3;
	m_excitationWavelengthNm = 532.0;

	m_trainDir.clear();
	m_lastError.clear();
}

double RamanPlasticRecognizer::wavelengthNmToRamanShiftCm1(double wavelengthNm) const
{
	if (!isfinite(wavelengthNm) || wavelengthNm <= 0.0)
	{
		throwRecognitionError(
			Error_InputSpectrumInvalidValue,
			"输入波长存在非法值，必须大于0"
		);
	}

	if (!isfinite(m_excitationWavelengthNm) || m_excitationWavelengthNm <= 0.0)
	{
		throwRecognitionError(
			Error_InputSpectrumInvalidValue,
			"激发激光波长未正确设置"
		);
	}

	return 1.0e7 / m_excitationWavelengthNm - 1.0e7 / wavelengthNm;
}


RamanPlasticRecognizer::~RamanPlasticRecognizer()
{
}

void RamanPlasticRecognizer::setTrainDirectory(const std::string& trainDir)
{
	m_trainDir = trainDir;
}

std::string RamanPlasticRecognizer::getLastError() const
{
	return m_lastError;
}

void RamanPlasticRecognizer::throwRecognitionError(
	RamanErrorCode errorCode,
	const std::string& message
) const
{
	throw RecognitionError(errorCode, message);
}

// ============================================================
// 对外核心识别接口，纯标准 C++ std::vector<float>
// ============================================================

RamanErrorCode RamanPlasticRecognizer::recognition(
	const std::vector<float>& m_waveLength,
	const std::vector<float>& temp_originalSpectrum,
	int& type
)
{
	type = 0;
	m_lastError.clear();

	try
	{
		if (m_trainDir.empty())
		{
			throwRecognitionError(
				Error_TrainDirectoryEmpty,
				"训练集路径为空，请先调用 setTrainDirectory"
			);
		}

		if (!pathExists(m_trainDir))
		{
			throwRecognitionError(
				Error_TrainDirectoryNotExist,
				"训练集目录不存在: " + m_trainDir
			);
		}

		if (!pathIsDirectory(m_trainDir))
		{
			throwRecognitionError(
				Error_TrainDirectoryNotAccessible,
				"训练集路径不是有效目录或无法访问: " + m_trainDir
			);
		}

		if (m_waveLength.empty() || temp_originalSpectrum.empty())
		{
			throwRecognitionError(
				Error_InputSpectrumEmpty,
				"输入光谱数据为空"
			);
		}

		if (m_waveLength.size() != temp_originalSpectrum.size())
		{
			throwRecognitionError(
				Error_InputSpectrumSizeMismatch,
				"波长数组和强度数组长度不一致"
			);
		}

		vector<SpectrumPoint> spectrum =
			buildSpectrumFromVector(m_waveLength, temp_originalSpectrum);

		vector<double> testFeature =
			extractFeaturesFromRawSpectrum(spectrum);

		if (testFeature.size() != m_featureX.size())
		{
			throwRecognitionError(
				Error_FeatureDimensionInvalid,
				"测试样品特征维度不是23"
			);
		}

		vector<TrainSample> trainSamples =
			loadTrainingData(m_trainDir);

		int pred = predictKNN(testFeature, trainSamples, m_kValue);

		if (pred < 1 || pred > 7)
		{
			type = 0;

			throwRecognitionError(
				Error_PredictedLabelInvalid,
				"预测类别编号非法，不在1-9范围内"
			);
		}

		type = pred;

		return Error_None_raman;
	}
	catch (const RecognitionError& e)
	{
		type = 0;
		m_lastError = e.what();
		return e.code;
	}
	catch (const bad_alloc& e)
	{
		type = 0;
		m_lastError = e.what();
		return Error_MemoryAllocationFailed;
	}
	catch (const exception& e)
	{
		type = 0;
		m_lastError = e.what();
		return Error_StdException;
	}
	catch (...)
	{
		type = 0;
		m_lastError = "未知异常";
		return Error_UnknownException;
	}
}

// ============================================================
// std::vector<float> 转内部光谱结构
// ============================================================

vector<RamanPlasticRecognizer::SpectrumPoint>
RamanPlasticRecognizer::buildSpectrumFromVector(
	const std::vector<float>& waveLength,
	const std::vector<float>& intensity
)
{
	if (waveLength.empty() || intensity.empty())
	{
		throwRecognitionError(
			Error_InputSpectrumEmpty,
			"输入光谱为空"
		);
	}

	if (waveLength.size() != intensity.size())
	{
		throwRecognitionError(
			Error_InputSpectrumSizeMismatch,
			"波长数组和强度数组长度不一致"
		);
	}

	vector<SpectrumPoint> spectrum;
	spectrum.reserve(waveLength.size());

	for (size_t i = 0; i < waveLength.size(); ++i)
	{
		double wavelengthNm = static_cast<double>(waveLength[i]);
		double y = static_cast<double>(intensity[i]);

		if (!isfinite(wavelengthNm) || !isfinite(y))
		{
			throwRecognitionError(
				Error_InputSpectrumInvalidValue,
				"输入光谱中存在 NaN 或 Inf"
			);
		}

		SpectrumPoint p;
		p.ramanShift = wavelengthNmToRamanShiftCm1(wavelengthNm);
		p.intensity = y;
		spectrum.push_back(p);
	}

	sort(
		spectrum.begin(),
		spectrum.end(),
		[](const SpectrumPoint& a, const SpectrumPoint& b)
	{
		return a.ramanShift < b.ramanShift;
	}
	);

	return spectrum;
}

// ============================================================
// 工具函数
// ============================================================

string RamanPlasticRecognizer::removeUTF8BOM(const string& s)
{
	if (s.size() >= 3 &&
		static_cast<unsigned char>(s[0]) == 0xEF &&
		static_cast<unsigned char>(s[1]) == 0xBB &&
		static_cast<unsigned char>(s[2]) == 0xBF)
	{
		return s.substr(3);
	}

	return s;
}

string RamanPlasticRecognizer::trim(const string& s)
{
	size_t start = s.find_first_not_of(" \t\r\n");

	if (start == string::npos)
	{
		return "";
	}

	size_t end = s.find_last_not_of(" \t\r\n");

	return s.substr(start, end - start + 1);
}

bool RamanPlasticRecognizer::isNumber(const string& s)
{
	string t = trim(s);

	if (t.empty())
	{
		return false;
	}

	char* endptr = nullptr;
	strtod(t.c_str(), &endptr);

	return endptr != t.c_str() && *endptr == '\0';
}

vector<string> RamanPlasticRecognizer::splitFlexible(const string& line)
{
	vector<string> result;

	string cleaned = removeUTF8BOM(line);

	for (size_t i = 0; i < cleaned.size(); ++i)
	{
		if (cleaned[i] == ',' || cleaned[i] == '\t' || cleaned[i] == ';')
		{
			cleaned[i] = ' ';
		}
	}

	stringstream ss(cleaned);
	string item;

	while (ss >> item)
	{
		item = trim(item);

		if (!item.empty())
		{
			result.push_back(item);
		}
	}

	return result;
}

// ============================================================
// 读取训练集矩阵
// ============================================================

vector<vector<double>> RamanPlasticRecognizer::readMatrixCSV(
	const string& filename
)
{
	if (!pathExists(filename))
	{
		throwRecognitionError(
			Error_TrainCsvMissing,
			"训练集 CSV 文件缺失: " + filename
		);
	}

	if (!pathIsRegularFile(filename))
	{
		throwRecognitionError(
			Error_TrainCsvOpenFailed,
			"训练集路径不是普通文件: " + filename
		);
	}

	ifstream file(filename.c_str());

	if (!file.is_open())
	{
		throwRecognitionError(
			Error_TrainCsvOpenFailed,
			"无法打开训练集文件，可能无权限、路径错误或被占用: " + filename
		);
	}

	vector<vector<double>> matrix;
	string line;
	bool firstLine = true;
	bool hasContentLine = false;
	bool hasValidNumber = false;

	while (getline(file, line))
	{
		line = trim(line);

		if (line.empty())
		{
			continue;
		}

		hasContentLine = true;

		if (firstLine)
		{
			line = removeUTF8BOM(line);
			firstLine = false;
		}

		vector<string> cols = splitFlexible(line);

		vector<double> row;

		for (size_t i = 0; i < cols.size(); ++i)
		{
			if (isNumber(cols[i]))
			{
				double value = stod(cols[i]);

				if (!isfinite(value))
				{
					throwRecognitionError(
						Error_TrainCsvContainsInvalidValue,
						"训练集 CSV 中存在 NaN 或 Inf: " + filename
					);
				}

				row.push_back(value);
				hasValidNumber = true;
			}
		}

		if (!row.empty())
		{
			matrix.push_back(row);
		}
	}

	if (file.bad())
	{
		throwRecognitionError(
			Error_TrainCsvReadFailed,
			"训练集 CSV 读取过程中失败: " + filename
		);
	}

	if (!hasContentLine)
	{
		throwRecognitionError(
			Error_TrainCsvEmpty,
			"训练集 CSV 文件为空: " + filename
		);
	}

	if (!hasValidNumber)
	{
		throwRecognitionError(
			Error_TrainCsvNoValidNumber,
			"训练集 CSV 中没有有效数字: " + filename
		);
	}

	return matrix;
}

// ============================================================
// 加载训练集
// ============================================================

vector<RamanPlasticRecognizer::TrainSample>
RamanPlasticRecognizer::loadTrainingData(
	const string& trainDir
)
{
	vector<TrainSample> samples;

	for (int label = 1; label <= 7; ++label)
	{
		ostringstream oss;
		oss << trainDir << "\\Sheet" << label << ".csv";

		string filename = oss.str();

		vector<vector<double>> mat = readMatrixCSV(filename);

		if (mat.empty())
		{
			throwRecognitionError(
				Error_TrainCsvNoValidNumber,
				"训练集文件为空或没有有效数字: " + filename
			);
		}

		int nrow = static_cast<int>(mat.size());
		int ncol = 0;

		for (size_t i = 0; i < mat.size(); ++i)
		{
			if (static_cast<int>(mat[i].size()) > ncol)
			{
				ncol = static_cast<int>(mat[i].size());
			}
		}

		cout << "读取训练集: " << filename
			<< "  行数=" << nrow
			<< "  最大列数=" << ncol << endl;

		if (nrow == static_cast<int>(m_featureX.size()))
		{
			for (int c = 0; c < ncol; ++c)
			{
				vector<double> feature;
				bool valid = true;

				for (int r = 0; r < nrow; ++r)
				{
					if (c >= static_cast<int>(mat[r].size()))
					{
						valid = false;
						break;
					}

					feature.push_back(mat[r][c]);
				}

				if (valid && feature.size() == m_featureX.size())
				{
					TrainSample s;
					s.feature = feature;
					s.label = label;
					samples.push_back(s);
				}
			}
		}
		else if (ncol == static_cast<int>(m_featureX.size()))
		{
			for (int r = 0; r < nrow; ++r)
			{
				if (mat[r].size() < m_featureX.size())
				{
					continue;
				}

				vector<double> feature;

				for (size_t c = 0; c < m_featureX.size(); ++c)
				{
					feature.push_back(mat[r][c]);
				}

				TrainSample s;
				s.feature = feature;
				s.label = label;
				samples.push_back(s);
			}
		}
		else
		{
			throwRecognitionError(
				Error_TrainCsvDimensionInvalid,
				"训练集维度不符合23维特征要求: " + filename +
				"，行数=" + to_string(nrow) +
				"，列数=" + to_string(ncol)
			);
		}
	}

	if (samples.empty())
	{
		throwRecognitionError(
			Error_TrainSampleEmpty,
			"训练集为空，没有读取到任何样本"
		);
	}

	return samples;
}

// ============================================================
// 线性方程组求解
// ============================================================

vector<double> RamanPlasticRecognizer::solveLinearSystem(
	vector<vector<double>> A,
	vector<double> b
)
{
	int n = static_cast<int>(A.size());

	for (int i = 0; i < n; ++i)
	{
		int pivot = i;
		double maxAbs = fabs(A[i][i]);

		for (int r = i + 1; r < n; ++r)
		{
			if (fabs(A[r][i]) > maxAbs)
			{
				maxAbs = fabs(A[r][i]);
				pivot = r;
			}
		}

		if (maxAbs < 1e-12)
		{
			throwRecognitionError(
				Error_LinearSystemSolveFailed,
				"线性方程组求解失败，矩阵接近奇异"
			);
		}

		if (pivot != i)
		{
			swap(A[i], A[pivot]);
			swap(b[i], b[pivot]);
		}

		double diag = A[i][i];

		for (int c = i; c < n; ++c)
		{
			A[i][c] /= diag;
		}

		b[i] /= diag;

		for (int r = i + 1; r < n; ++r)
		{
			double factor = A[r][i];

			if (fabs(factor) < 1e-15)
			{
				continue;
			}

			for (int c = i; c < n; ++c)
			{
				A[r][c] -= factor * A[i][c];
			}

			b[r] -= factor * b[i];
		}
	}

	vector<double> x(n, 0.0);

	for (int i = n - 1; i >= 0; --i)
	{
		double sum = b[i];

		for (int c = i + 1; c < n; ++c)
		{
			sum -= A[i][c] * x[c];
		}

		x[i] = sum;
	}

	return x;
}

// ============================================================
// ALS 去基线
// ============================================================

vector<double> RamanPlasticRecognizer::asymmetricLeastSquaresBaseline(
	const vector<double>& y,
	double asym,
	double smooth,
	int iterations
)
{
	int n = static_cast<int>(y.size());

	if (n < 5)
	{
		throwRecognitionError(
			Error_BaselineCorrectionFailed,
			"数据点太少，无法进行 ALS 去基线"
		);
	}

	vector<double> w(n, 1.0);
	vector<double> z(n, 0.0);

	for (int iter = 0; iter < iterations; ++iter)
	{
		vector<vector<double>> A(n, vector<double>(n, 0.0));
		vector<double> b(n, 0.0);

		for (int i = 0; i < n; ++i)
		{
			A[i][i] += w[i];
			b[i] = w[i] * y[i];
		}

		for (int i = 0; i < n; ++i)
		{
			if (i == 0 || i == n - 1)
			{
				A[i][i] += smooth * 1.0;
			}
			else if (i == 1 || i == n - 2)
			{
				A[i][i] += smooth * 5.0;
			}
			else
			{
				A[i][i] += smooth * 6.0;
			}
		}

		for (int i = 0; i < n - 1; ++i)
		{
			double val;

			if (i == 0 || i == n - 2)
			{
				val = -2.0;
			}
			else
			{
				val = -4.0;
			}

			A[i][i + 1] += smooth * val;
			A[i + 1][i] += smooth * val;
		}

		for (int i = 0; i < n - 2; ++i)
		{
			A[i][i + 2] += smooth * 1.0;
			A[i + 2][i] += smooth * 1.0;
		}

		z = solveLinearSystem(A, b);

		for (int i = 0; i < n; ++i)
		{
			double d = y[i] - z[i];

			if (d > 0)
			{
				w[i] = asym;
			}
			else if (d < 0)
			{
				w[i] = 1.0 - asym;
			}
			else
			{
				w[i] = 1.0;
			}
		}
	}

	return z;
}

// ============================================================
// 归一化
// ============================================================

vector<double> RamanPlasticRecognizer::minMaxNormalize(
	const vector<double>& y
)
{
	if (y.empty())
	{
		throwRecognitionError(
			Error_NormalizationFailed,
			"归一化失败，输入为空"
		);
	}

	double minVal = *min_element(y.begin(), y.end());
	double maxVal = *max_element(y.begin(), y.end());

	if (fabs(maxVal - minVal) < 1e-12)
	{
		throwRecognitionError(
			Error_NormalizationZeroRange,
			"归一化失败，光谱最大值和最小值几乎相等"
		);
	}

	vector<double> out(y.size(), 0.0);

	for (size_t i = 0; i < y.size(); ++i)
	{
		out[i] = (y[i] - minVal) / (maxVal - minVal);
	}

	return out;
}

// ============================================================
// 线性插值
// ============================================================

double RamanPlasticRecognizer::linearInterpolate(
	const vector<double>& x,
	const vector<double>& y,
	double target
)
{
	int n = static_cast<int>(x.size());

	if (n < 2 || y.size() != x.size())
	{
		throwRecognitionError(
			Error_InterpolationFailed,
			"插值失败，数据点太少或 x/y 数量不一致"
		);
	}

	if (target <= x.front())
	{
		double x0 = x[0];
		double x1 = x[1];
		double y0 = y[0];
		double y1 = y[1];

		if (fabs(x1 - x0) < 1e-12)
		{
			return y0;
		}

		return y0 + (target - x0) * (y1 - y0) / (x1 - x0);
	}

	if (target >= x.back())
	{
		double x0 = x[n - 2];
		double x1 = x[n - 1];
		double y0 = y[n - 2];
		double y1 = y[n - 1];

		if (fabs(x1 - x0) < 1e-12)
		{
			return y0;
		}

		return y0 + (target - x0) * (y1 - y0) / (x1 - x0);
	}

	vector<double>::const_iterator it =
		lower_bound(x.begin(), x.end(), target);

	int idx = static_cast<int>(it - x.begin());

	if (idx >= 0 && idx < n && fabs(x[idx] - target) < 1e-12)
	{
		return y[idx];
	}

	int i0 = idx - 1;
	int i1 = idx;

	double x0 = x[i0];
	double x1 = x[i1];
	double y0 = y[i0];
	double y1 = y[i1];

	if (fabs(x1 - x0) < 1e-12)
	{
		return y0;
	}

	return y0 + (target - x0) * (y1 - y0) / (x1 - x0);
}

// ============================================================
// 提取 23 维特征
// ============================================================

vector<double> RamanPlasticRecognizer::extractFeaturesFromRawSpectrum(
	const vector<SpectrumPoint>& spectrum
)
{
	vector<double> x;
	vector<double> y;

	for (size_t i = 0; i < spectrum.size(); ++i)
	{
		if (spectrum[i].ramanShift >= 800.0 &&
			spectrum[i].ramanShift <= 1800.0)
		{
			x.push_back(spectrum[i].ramanShift);
			y.push_back(spectrum[i].intensity);
		}
	}

	cout << "800-1800 cm^-1 范围内点数: " << x.size() << endl;

	if (x.size() < 20)
	{
		throwRecognitionError(
			Error_InputSpectrumRangeTooSmall,
			"800-1800 cm^-1 范围内的数据点太少，无法提取特征"
		);
	}

	vector<double> xUnique;
	vector<double> yUnique;

	for (size_t i = 0; i < x.size(); ++i)
	{
		if (!xUnique.empty() && fabs(x[i] - xUnique.back()) < 1e-12)
		{
			yUnique.back() = y[i];
		}
		else
		{
			xUnique.push_back(x[i]);
			yUnique.push_back(y[i]);
		}
	}

	x = xUnique;
	y = yUnique;

	if (x.size() < 5)
	{
		throwRecognitionError(
			Error_InputSpectrumDuplicateXTooMany,
			"有效光谱点太少，可能是拉曼位移重复点过多"
		);
	}

	vector<double> baseline = asymmetricLeastSquaresBaseline(
		y,
		m_alsAsym,
		m_alsSmooth,
		m_alsIter
	);

	vector<double> corrected(y.size(), 0.0);

	for (size_t i = 0; i < y.size(); ++i)
	{
		corrected[i] = y[i] - baseline[i];
	}

	vector<double> normalized = minMaxNormalize(corrected);

	vector<double> features;
	features.reserve(m_featureX.size());

	cout << "提取到的23维特征:" << endl;

	for (size_t i = 0; i < m_featureX.size(); ++i)
	{
		double val = linearInterpolate(x, normalized, m_featureX[i]);

		features.push_back(val);

		cout << fixed << setprecision(0) << m_featureX[i]
			<< " cm^-1 -> "
			<< setprecision(6) << val << endl;
	}

	return features;
}

// ============================================================
// 欧氏距离
// ============================================================

double RamanPlasticRecognizer::euclideanDistance(
	const vector<double>& a,
	const vector<double>& b
)
{
	if (a.size() != b.size())
	{
		throwRecognitionError(
			Error_TrainSampleDimensionMismatch,
			"欧氏距离计算失败，特征维度不一致"
		);
	}

	double sum = 0.0;

	for (size_t i = 0; i < a.size(); ++i)
	{
		double d = a[i] - b[i];
		sum += d * d;
	}

	return sqrt(sum);
}

// ============================================================
// KNN 预测
// ============================================================

int RamanPlasticRecognizer::predictKNN(
	const vector<double>& testFeature,
	const vector<TrainSample>& trainSamples,
	int k
)
{
	if (testFeature.empty())
	{
		throwRecognitionError(
			Error_FeatureExtractionFailed,
			"测试特征为空"
		);
	}

	if (trainSamples.empty())
	{
		throwRecognitionError(
			Error_KnnTrainSamplesEmpty,
			"训练样本为空"
		);
	}

	if (k <= 0)
	{
		throwRecognitionError(
			Error_KnnKInvalid,
			"KNN 参数 k 必须大于0"
		);
	}

	vector<pair<double, int>> distances;

	for (size_t i = 0; i < trainSamples.size(); ++i)
	{
		double dist = euclideanDistance(testFeature, trainSamples[i].feature);

		if (!isfinite(dist))
		{
			throwRecognitionError(
				Error_KnnDistanceFailed,
				"欧氏距离计算结果非法"
			);
		}

		distances.push_back(make_pair(dist, trainSamples[i].label));
	}

	sort(
		distances.begin(),
		distances.end(),
		[](const pair<double, int>& a, const pair<double, int>& b)
	{
		return a.first < b.first;
	}
	);

	if (distances.empty())
	{
		throwRecognitionError(
			Error_KnnPredictionFailed,
			"KNN 距离列表为空"
		);
	}

	cout << endl;
	cout << "最近邻信息:" << endl;

	for (int i = 0; i < k && i < static_cast<int>(distances.size()); ++i)
	{
		int label = distances[i].second;

		string className = "未知";

		if (label >= 0 && label < static_cast<int>(CLASS_NAMES.size()))
		{
			className = CLASS_NAMES[label];
		}

		cout << "Top " << i + 1
			<< "  label=" << label
			<< "  class=" << className
			<< "  dist=" << fixed << setprecision(6)
			<< distances[i].first << endl;
	}

	if (k <= 1)
	{
		return distances[0].second;
	}

	vector<int> vote(8, 0);

	for (int i = 0; i < k && i < static_cast<int>(distances.size()); ++i)
	{
		int label = distances[i].second;

		if (label >= 1 && label <= 7)
		{
			vote[label]++;
		}
	}

	int bestLabel = 1;
	int bestCount = vote[1];

	for (int label = 2; label <= 7; ++label)
	{
		if (vote[label] > bestCount)
		{
			bestCount = vote[label];
			bestLabel = label;
		}
	}

	return bestLabel;
}

// ============================================================
// 以下是 exe 测试 main 使用的本地工具函数
// ============================================================

static string testRemoveUTF8BOM(const string& s)
{
	if (s.size() >= 3 &&
		static_cast<unsigned char>(s[0]) == 0xEF &&
		static_cast<unsigned char>(s[1]) == 0xBB &&
		static_cast<unsigned char>(s[2]) == 0xBF)
	{
		return s.substr(3);
	}

	return s;
}

static string testTrim(const string& s)
{
	size_t start = s.find_first_not_of(" \t\r\n");

	if (start == string::npos)
	{
		return "";
	}

	size_t end = s.find_last_not_of(" \t\r\n");

	return s.substr(start, end - start + 1);
}

static bool testIsNumber(const string& s)
{
	string t = testTrim(s);

	if (t.empty())
	{
		return false;
	}

	char* endptr = nullptr;
	strtod(t.c_str(), &endptr);

	return endptr != t.c_str() && *endptr == '\0';
}

static vector<string> testSplitFlexible(const string& line)
{
	vector<string> result;

	string cleaned = testRemoveUTF8BOM(line);

	for (size_t i = 0; i < cleaned.size(); ++i)
	{
		if (cleaned[i] == ',' || cleaned[i] == '\t' || cleaned[i] == ';')
		{
			cleaned[i] = ' ';
		}
	}

	stringstream ss(cleaned);
	string item;

	while (ss >> item)
	{
		item = testTrim(item);

		if (!item.empty())
		{
			result.push_back(item);
		}
	}

	return result;
}

static bool readTestCsvToVector(
	const string& filename,
	std::vector<float>& waveLength,
	std::vector<float>& intensity
)
{
	ifstream file(filename.c_str());

	if (!file.is_open())
	{
		cerr << "无法打开测试光谱文件: " << filename << endl;
		return false;
	}

	waveLength.clear();
	intensity.clear();

	string line;
	bool firstLine = true;

	while (getline(file, line))
	{
		line = testTrim(line);

		if (line.empty())
		{
			continue;
		}

		if (firstLine)
		{
			line = testRemoveUTF8BOM(line);
			firstLine = false;
		}

		vector<string> cols = testSplitFlexible(line);

		if (cols.size() < 2)
		{
			continue;
		}

		if (cols.size() == 2)
		{
			if (!testIsNumber(cols[0]) || !testIsNumber(cols[1]))
			{
				continue;
			}

			waveLength.push_back(static_cast<float>(stod(cols[0])));
			intensity.push_back(static_cast<float>(stod(cols[1])));
		}
		else
		{
			if (!testIsNumber(cols[0]) || !testIsNumber(cols[2]))
			{
				continue;
			}

			waveLength.push_back(static_cast<float>(stod(cols[0])));
			intensity.push_back(static_cast<float>(stod(cols[2])));
		}
	}

	if (waveLength.empty() || intensity.empty())
	{
		cerr << "测试光谱文件中没有有效数据: " << filename << endl;
		return false;
	}

	if (waveLength.size() != intensity.size())
	{
		cerr << "测试光谱波长和强度数量不一致" << endl;
		return false;
	}

	return true;
}

// ============================================================
// 主函数
// ============================================================

// int main()
// {
// 	cout << "========== Raman Plastic Recognition ==========" << endl;
// 	cout << "测试光谱文件: " << TEST_FILE << endl;
// 	cout << "训练集目录: " << TRAIN_DIR << endl;
// 	cout << endl;

// 	std::vector<float> waveLength;
// 	std::vector<float> intensity;

// 	if (!readTestCsvToVector(TEST_FILE, waveLength, intensity))
// 	{
// 		cerr << "读取测试光谱失败" << endl;
// 		system("pause");
// 		return 1;
// 	}

// 	cout << "成功读取测试光谱点数: " << waveLength.size() << endl;

// 	if (!waveLength.empty())
// 	{
// 		cout << "第一个点: "
// 			<< waveLength[0] << ", "
// 			<< intensity[0] << endl;

// 		cout << "最后一个点: "
// 			<< waveLength[waveLength.size() - 1] << ", "
// 			<< intensity[intensity.size() - 1] << endl;
// 	}

// 	cout << endl;

// 	RamanPlasticRecognizer recognizer;
// 	recognizer.setTrainDirectory(TRAIN_DIR);

// 	int type = 0;

// 	RamanErrorCode ret = recognizer.recognition(
// 		waveLength,
// 		intensity,
// 		type
// 	);

// 	if (ret != Error_None_raman)
// 	{
// 		cerr << endl;
// 		cerr << "识别失败" << endl;
// 		cerr << "错误码: " << static_cast<int>(ret) << endl;
// 		cerr << "错误信息: " << recognizer.getLastError() << endl;

// 		system("pause");
// 		return 1;
// 	}

// 	cout << endl;
// 	cout << "==============================================" << endl;
// 	cout << "识别成功" << endl;
// 	cout << "识别结果类别编号: " << type << endl;

// 	if (type >= 0 && type < static_cast<int>(CLASS_NAMES.size()))
// 	{
// 		cout << "识别结果塑料类型: " << CLASS_NAMES[type] << endl;
// 	}
// 	else
// 	{
// 		cout << "识别结果塑料类型: 未知" << endl;
// 	}

// 	cout << "==============================================" << endl;

// 	system("pause");
// 	return 0;
// }
