#ifndef ANALYZER_ANALYZER_H
#define ANALYZER_ANALYZER_H

#include <string>
#include <vector>

namespace AVSAnalyzer {
	struct Control;
	class Config;
	class Scheduler;

	struct AlgorithmDetectObject
	{
		int x1;
		int y1;
		int x2;
		int y2;
		float score;
		std::string class_name;
	};

	class AlgorithmWithApi
	{
	public:
		AlgorithmWithApi() = delete;
		AlgorithmWithApi(Config* config);
		~AlgorithmWithApi();
	public:
		bool test();
		bool objectDetect(int height, int width, unsigned char* bgr, std::vector<AlgorithmDetectObject>& detects);
	private:
		bool parseObjectDetect(std::string& response, std::vector<AlgorithmDetectObject>& detects);
		Config* mConfig;
	};

	class Analyzer
	{
	public:
		explicit Analyzer(Scheduler* scheduler, Control* control);
		~Analyzer();
	public:
		bool checkVideoFrame(bool check, int64_t frameCount, unsigned char* data, float& happenScore);
		bool checkAudioFrame(bool check, int64_t frameCount, unsigned char* data, int size);

	private:
		Scheduler* mScheduler;
		Control*   mControl;
		AlgorithmWithApi* mAlgorithm;
		std::vector<AlgorithmDetectObject> mDetects;
	};
}
#endif //ANALYZER_ANALYZER_H

