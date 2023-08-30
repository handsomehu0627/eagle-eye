#include "GenerateAlarm.h"
#include "Config.h"
#include "Utils/Log.h"
#include "Utils/Common.h"
#include "Control.h"
#include "ControlExecutor.h"
#include "Scheduler.h"
#include <vector>

#ifndef WIN32
#include <opencv2/opencv.hpp>
#else
#ifndef _DEBUG
#include <turbojpeg.h>
#else
#include <opencv2/opencv.hpp>
#endif
#endif //WIN32

namespace AVSAnalyzer {

    bool gen_turboJpeg_compress(int height, int width, int channels, unsigned char* bgr, unsigned char*& out_data, unsigned long* out_size) {
#ifdef WIN32
#ifndef _DEBUG

        tjhandle handle = tjInitCompress();
        if (nullptr == handle) {
            return false;
        }

        //pixel_format : TJPF::TJPF_BGR or other
        const int JPEG_QUALITY = 75;
        int pixel_format = TJPF::TJPF_BGR;
        int pitch = tjPixelSize[pixel_format] * width;
        int ret = tjCompress2(handle, bgr, width, pitch, height, pixel_format,
            &out_data, out_size, TJSAMP_444, JPEG_QUALITY, TJFLAG_FASTDCT);

        tjDestroy(handle);

        if (ret != 0) {
            return false;
        }
        return true;

#endif // !_DEBUG
#endif //WIN32

        return false;
    }

    bool genCompressImage(int height, int width, int channels, unsigned char* bgr, AVSAlarmImage* image) {
#ifdef WIN32
#ifndef _DEBUG
        unsigned char* jpeg_data = nullptr;
        unsigned long  jpeg_size = 0;

        gen_turboJpeg_compress(height, width, channels, bgr, jpeg_data, &jpeg_size);

        if (jpeg_size > 0 && jpeg_data != nullptr) {
            image->set(jpeg_data, jpeg_size, width, height, channels);
            free(jpeg_data);
            jpeg_data = nullptr;
            return true;
        }
        else {
            return false;
        }
#endif // !_DEBUG

#else
        cv::Mat bgr_image(height, width, CV_8UC3, bgr);
        std::vector<int> quality = { 100 };
        std::vector<uchar> jpeg_data;
        cv::imencode(".jpg", bgr_image, jpeg_data, quality);
        image->set(jpeg_data.data(), jpeg_data.size(), width, height, channels);
        return true;
#endif //WIN32
    }


    AVSAlarmImage* AVSAlarmImage::Create(int width, int height, int channels) {
        return new AVSAlarmImage(width,height,channels);
    }
    AVSAlarmImage::AVSAlarmImage(int width, int height, int channels) {
        this->size = width * height * channels;
        this->data = (unsigned char*)malloc(this->size);
    }
    AVSAlarmImage::~AVSAlarmImage() {
        if (this->data) {
            free(this->data);
            this->data = nullptr;
        }
        this->size = 0;
        this->width = 0;
        this->height = 0;
        this->channels = 0;

    }

    void AVSAlarmImage::set(unsigned char* data, int size, int width, int height, int channels) {
        memcpy(this->data, data, size);
        this->size = size;
        this->width = width;
        this->height = height;
        this->channels = channels;
    }

    unsigned char* AVSAlarmImage::getData() {
        return this->data;
    }
    int AVSAlarmImage::getSize() {
        return this->size;
    }
    int AVSAlarmImage::getWidth() {
        return this->width;
    }
    int AVSAlarmImage::getHeight() {
        return this->height;
    }
    int AVSAlarmImage::getChannels() {
        return this->channels;
    }
    AVSAlarm* AVSAlarm::Create(int height, int width, int fps, int64_t happen, const char* controlCode) {

        AVSAlarm* alarm = new AVSAlarm;

        alarm->height = height;
        alarm->width = width;
        alarm->fps = fps;
        alarm->happen = happen;
        alarm->controlCode = controlCode;

        return alarm;
    }
    AVSAlarm::AVSAlarm() {
        LOGI("");
    }
    AVSAlarm::~AVSAlarm() {
        LOGI("");
    }


    GenerateAlarm::GenerateAlarm(Config* config, Control* control) :
        mConfig(config),
        mControl(control)
    {

    }

    GenerateAlarm::~GenerateAlarm()
    {
        clearVideoFrameQueue();
    }


    void GenerateAlarm::pushVideoFrame(unsigned char* data, int size, bool happen, float happenScore) {

        VideoFrame* frame = new VideoFrame(VideoFrame::BGR, size, mControl->videoWidth, mControl->videoHeight);
        frame->size = size;
        memcpy(frame->data, data, size);
        frame->happen = happen;
        frame->happenScore = happenScore;

        mVideoFrameQ_mtx.lock();
        mVideoFrameQ.push(frame);
        mVideoFrameQ_mtx.unlock();

    }
    bool GenerateAlarm::getVideoFrame(VideoFrame*& frame, int& frameQSize) {

        mVideoFrameQ_mtx.lock();

        if (!mVideoFrameQ.empty()) {
            frame = mVideoFrameQ.front();
            mVideoFrameQ.pop();
            frameQSize = mVideoFrameQ.size();
            mVideoFrameQ_mtx.unlock();
            return true;

        }
        else {
            mVideoFrameQ_mtx.unlock();
            return false;
        }

    }
    void GenerateAlarm::clearVideoFrameQueue() {

        mVideoFrameQ_mtx.lock();
        while (!mVideoFrameQ.empty())
        {
            VideoFrame* frame = mVideoFrameQ.front();
            mVideoFrameQ.pop();
            delete frame;
            frame = nullptr;
        }
        mVideoFrameQ_mtx.unlock();

    }

    void GenerateAlarm::generateAlarmThread(void* arg) {
        ControlExecutor* executor = (ControlExecutor*)arg;
        int width = executor->mControl->videoWidth;
        int height = executor->mControl->videoHeight;
        int channels = 3;

        VideoFrame* videoFrame = nullptr; // 未编码的视频帧（bgr格式）
        int         videoFrameQSize = 0; // 未编码视频帧队列当前长度

        std::vector<AVSAlarmImage* > cacheV;
        int cacheV_max_size = 75; //75 = 25 * 3，最多缓存事件发生前3秒的数据，1张压缩图片100kb
        int cacheV_min_size = 50;  // 50 = 25 * 2, 最少缓存事件发生前2秒的数据

        bool happening = false;// 当前是否正在发生报警行为
        std::vector<AVSAlarmImage* > happenV;
        int     happenV_alarm_max_size = 150;//报警总帧数最大长度
        int64_t last_alarm_timestamp = 0;// 上一次报警的时间戳

        int64_t t1, t2 = 0;

        while (executor->getState())
        {
            if (executor->mGenerateAlarm->getVideoFrame(videoFrame, videoFrameQSize)) {

                t1 = getCurTime();

                AVSAlarmImage* image = executor->mScheduler->gainAlarmImage();

                bool comp = genCompressImage(height, width, channels, videoFrame->data, image);

                if (comp) {
                    image->happen = videoFrame->happen;
                    image->happenScore = videoFrame->happenScore;
                }

                t2 = getCurTime();

                if (happening) {// 报警事件已经发生，正在进行中

                    if (comp) {
                        happenV.push_back(image);
                        //LOGI("h=%d,w=%d,compressSize=%lu,compress spend: %lld(ms),happenV.size=%lld",
                        //    height, width, compressImage->size, (t2 - t1), happenV.size());
                    }
                    else {
                        executor->mScheduler->giveBackAlarmImage(image);
                    }

                    if (happenV.size() >= happenV_alarm_max_size) {
                        last_alarm_timestamp = getCurTimestamp();

                        AVSAlarm* alarm = AVSAlarm::Create(
                            height,
                            width,
                            executor->mControl->videoFps,
                            last_alarm_timestamp,
                            executor->mControl->code.data()
                        );

                        for (size_t i = 0; i < happenV.size(); i++)
                        {
                            alarm->images.push_back(happenV[i]);
                        }
                        happenV.clear();

                        executor->mScheduler->addAlarm(alarm);

                        happening = false;
                    }
                    delete videoFrame;
                    videoFrame = nullptr;

                }
                else {// 暂未发生报警事件

                    if (comp) {

                        cacheV.push_back(image);

                        //LOGI("cache h=%d,w=%d,compressSize=%d,compress spend: %lld(ms),cacheQ.size=%lld",height, width, compressImage.getSize(), (t2 - t1), cacheV.size());


                        if (!cacheV.empty() && cacheV.size() > cacheV_max_size) {
                            //满足缓存过期帧
                            auto b = cacheV.begin();
                            cacheV.erase(b);
                            AVSAlarmImage* headImage = *b;
                            executor->mScheduler->giveBackAlarmImage(headImage);
                        }


                        if (videoFrame->happen && cacheV.size() > cacheV_min_size &&
                            (getCurTimestamp() - last_alarm_timestamp) > executor->mControl->alarmMinInterval) {
                            //满足报警触发帧
                            happening = true;
                            happenV = cacheV;
                            cacheV.clear();
                        }

                    }
                    else {
                        executor->mScheduler->giveBackAlarmImage(image);
                    }
                    delete videoFrame;
                    videoFrame = nullptr;
                }

            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }

        }


    }
}
