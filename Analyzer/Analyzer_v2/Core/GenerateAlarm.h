#ifndef ANALYZER_GENERATEALARM_H
#define ANALYZER_GENERATEALARM_H

#include <queue>
#include <mutex>
namespace AVSAnalyzer {

	class Config;
	struct Control;
	struct VideoFrame;

	struct AVSAlarmImage
	{
	public:
		static AVSAlarmImage* Create(int width, int height, int channels);
		~AVSAlarmImage();
	private:
		AVSAlarmImage(int width, int height, int channels);
	public:
		void set(unsigned char* data, int size, int width, int height, int channels);

		bool  happen = false;// 是否发生事件
		float happenScore = 0;// 发生事件的分数

		unsigned char* getData();
		int getSize();
		int getWidth();
		int getHeight();
		int getChannels();
	private:

		unsigned char* data = nullptr;//图片经过jpg压缩后的数据
		int size = 0;                 //图片经过jpg压缩后的数据长度
		int width = 0;                //原图宽
		int height = 0;               //原图高
		int channels = 0;             //原图通道长
	};
	struct AVSAlarm
	{
	public:
		static AVSAlarm* Create(int height, int width, int fps, int64_t happen, const char* controlCode);
		~AVSAlarm();
	private:
		AVSAlarm();
	public:
		int width = 0;
		int height = 0;
		int fps = 0;
		int64_t happen = 0;
		std::string controlCode;// 布控编号

		AVSAlarmImage* headImage = nullptr;//封面图
		std::vector<AVSAlarmImage*> images;//组成报警视频的图片帧
	};

	class GenerateAlarm
	{
	public:
		GenerateAlarm(Config* config, Control* control);
		~GenerateAlarm();
	public:
		void pushVideoFrame(unsigned char* data, int size, bool happen, float happenScore);
	public:
		static void generateAlarmThread(void* arg);
	private:
		Config* mConfig;
		Control* mControl;

		//视频帧
		std::queue <VideoFrame*> mVideoFrameQ;
		std::mutex               mVideoFrameQ_mtx;
		bool getVideoFrame(VideoFrame*& frame, int& frameQSize);
		void clearVideoFrameQueue();

	};
}

#endif //ANALYZER_GENERATEALARM_H