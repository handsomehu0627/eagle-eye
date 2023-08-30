#ifndef ANALYZER_AVPUSHSTREAM_H
#define ANALYZER_AVPUSHSTREAM_H
#include <queue>
#include <mutex>
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}
namespace AVSAnalyzer {
	class Config;
	struct Control;
	struct VideoFrame;

	class AvPushStream
	{
	public:
		AvPushStream(Config* config, Control* control);
		~AvPushStream();
	public:
		bool connect();     // 连接流媒体服务
		bool reConnect();   // 重连流媒体服务
		void closeConnect();// 关闭流媒体服务的连接
		int mConnectCount = 0;

		AVFormatContext* mFmtCtx = nullptr;

		//视频帧
		AVCodecContext* mVideoCodecCtx = NULL;
		AVStream* mVideoStream = NULL;
		int mVideoIndex = -1;
		void pushVideoFrame(unsigned char* data, int size);


	public:
		static void encodeVideoAndWriteStreamThread(void* arg); // 编码视频帧并推流

	private:
		Config* mConfig;
		Control* mControl;

		//视频帧
		std::queue <VideoFrame*> mVideoFrameQ;
		std::mutex               mVideoFrameQ_mtx;
		bool getVideoFrame(VideoFrame*& frame, int& frameQSize);// 获取的frame，需要pushReusedVideoFrame
		void clearVideoFrameQueue();

		// bgr24转yuv420p
		unsigned char clipValue(unsigned char x, unsigned char min_val, unsigned char  max_val);
		bool bgr24ToYuv420p(unsigned char* bgrBuf, int w, int h, unsigned char* yuvBuf);
	};

}
#endif //ANALYZER_AVPUSHSTREAM_H
