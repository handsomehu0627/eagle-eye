#ifndef AVSALARMMANAGE_GENERATEVIDEO_H
#define AVSALARMMANAGE_GENERATEVIDEO_H

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}
namespace AVSAnalyzer {
	struct AVSAlarmImage;
	struct AVSAlarm;
	class Config;
	class GenerateVideo
	{
	public:
		GenerateVideo() = delete;
		GenerateVideo(Config* config,AVSAlarm* alarm);
		~GenerateVideo();
	
	public:
		bool run();
	private:
		Config* mConfig;
		AVSAlarm* mAlarm;
		bool initCodecCtx(const char * url);
		void destoryCodecCtx();

		AVFormatContext* mFmtCtx = nullptr;
		//视频帧
		AVCodecContext* mVideoCodecCtx = nullptr;
		AVStream* mVideoStream = nullptr;
		int mVideoIndex = -1;
	};

}
#endif //AVSALARMMANAGE_GENERATEVIDEO_H
