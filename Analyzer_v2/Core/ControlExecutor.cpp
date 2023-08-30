#include "ControlExecutor.h"
#include "Utils/Log.h"
#include "Utils/Common.h"
#include "Scheduler.h"
#include "Analyzer.h"
#include "Control.h"
#include "AvPullStream.h"
#include "AvPushStream.h"
#include "GenerateAlarm.h"

extern "C" {
#include "libswscale/swscale.h"
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}

namespace AVSAnalyzer {
    ControlExecutor::ControlExecutor(Scheduler* scheduler, Control* control) :
        mScheduler(scheduler),
        mControl(new Control(*control)),
        mPullStream(nullptr),
        mPushStream(nullptr),
        mGenerateAlarm(nullptr),
        mAnalyzer(nullptr),
        mState(false)
    {
        mControl->executorStartTimestamp = getCurTimestamp();

        LOGI("");
    }

    ControlExecutor::~ControlExecutor()
    {
        LOGI("");

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        mState = false;// 将执行状态设置为false

        for (auto th : mThreads) {
            th->join();

        }
        for (auto th : mThreads) {
            delete th;
            th = nullptr;
        }
        mThreads.clear();


        if (mPullStream) {
            delete mPullStream;
            mPullStream = nullptr;
        }
        if (mPushStream) {
            delete mPushStream;
            mPushStream = nullptr;
        }

        if (mAnalyzer) {
            delete mAnalyzer;
            mAnalyzer = nullptr;
        }

        if (mGenerateAlarm) {
            delete mGenerateAlarm;
            mGenerateAlarm = nullptr;
        }

        if (mControl) {
            delete mControl;
            mControl = nullptr;
        }
    }
    bool ControlExecutor::start(std::string& msg) {

        this->mPullStream = new AvPullStream(mScheduler->getConfig(), mControl);
        if (this->mPullStream->connect()) {
            if (mControl->pushStream) {
                this->mPushStream = new AvPushStream(mScheduler->getConfig(), mControl);
                if (this->mPushStream->connect()) {
                    // success
                }
                else {
                    msg = "pull stream connect success, push stream connect error";
                    return false;
                }
            }
            else {
                // success
            }
        }
        else {
            msg = "pull stream connect error";
            return false;
        }

        this->mAnalyzer = new Analyzer(mScheduler, mControl);
        this->mGenerateAlarm = new GenerateAlarm(mScheduler->getConfig(), mControl);

        mState = true;// 将执行状态设置为true


        std::thread* th = new std::thread(AvPullStream::readThread, this);
        mThreads.push_back(th);

        th = new std::thread(ControlExecutor::decodeAndAnalyzeVideoThread, this);
        mThreads.push_back(th);

        th = new std::thread(GenerateAlarm::generateAlarmThread, this);
        mThreads.push_back(th);


        if (mControl->pushStream) {
            if (mControl->videoIndex > -1) {
                th = new std::thread(AvPushStream::encodeVideoAndWriteStreamThread, this);
                mThreads.push_back(th);
            }
        }

        for (auto th : mThreads) {
            th->native_handle();
        }


        return true;
    }


    bool ControlExecutor::getState() {
        return mState;
    }
    void ControlExecutor::setState_remove() {
        this->mState = false;
        this->mScheduler->removeExecutor(mControl);
    }

    void ControlExecutor::decodeAndAnalyzeVideoThread(void* arg) {

        ControlExecutor* executor = (ControlExecutor*)arg;
        int width = executor->mPullStream->mVideoCodecCtx->width;
        int height = executor->mPullStream->mVideoCodecCtx->height;

        AVPacket pkt; // 未解码的视频帧
        int      pktQSize = 0; // 未解码视频帧队列当前长度

        AVFrame* frame_yuv420p = av_frame_alloc();// pkt->解码->frame
        AVFrame* frame_bgr = av_frame_alloc();

        int frame_bgr_buff_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, width, height, 1);
        uint8_t* frame_bgr_buff = (uint8_t*)av_malloc(frame_bgr_buff_size);
        av_image_fill_arrays(frame_bgr->data, frame_bgr->linesize, frame_bgr_buff, AV_PIX_FMT_BGR24, width, height, 1);

        SwsContext* sws_ctx_yuv420p2bgr = sws_getContext(width, height,
            executor->mPullStream->mVideoCodecCtx->pix_fmt,
            executor->mPullStream->mVideoCodecCtx->width,
            executor->mPullStream->mVideoCodecCtx->height,
            AV_PIX_FMT_BGR24,
            SWS_BICUBIC, nullptr, nullptr, nullptr);

        int fps = executor->mControl->videoFps;

        //算法检测参数start
        bool cur_is_check = false;// 当前帧是否进行算法检测
        int  continuity_check_count = 0;// 当前连续进行算法检测的帧数
        int  continuity_check_max_time = 3000;//连续进行算法检测，允许最长的时间。单位毫秒
        int64_t continuity_check_start = getCurTime();//单位毫秒
        int64_t continuity_check_end = 0;
        //算法检测参数end

        int ret = -1;
        int64_t frameCount = 0;
        while (executor->getState())
        {
            if (executor->mPullStream->getVideoPkt(pkt, pktQSize)) {

                if (executor->mControl->videoIndex > -1) {

                    ret = avcodec_send_packet(executor->mPullStream->mVideoCodecCtx, &pkt);
                    if (ret == 0) {
                        ret = avcodec_receive_frame(executor->mPullStream->mVideoCodecCtx, frame_yuv420p);

                        if (ret == 0) {
                            frameCount++;

                            // frame（yuv420p） 转 frame_bgr
                            sws_scale(sws_ctx_yuv420p2bgr,
                                frame_yuv420p->data, frame_yuv420p->linesize, 0, height,
                                frame_bgr->data, frame_bgr->linesize);

                            if (pktQSize == 0) {
                                cur_is_check = true;
                            }
                            else {
                                cur_is_check = false;
                            }

                            if (cur_is_check) {
                                continuity_check_count += 1;
                            }

                            continuity_check_end = getCurTime();
                            if (continuity_check_end - continuity_check_start > continuity_check_max_time) {
                                executor->mControl->checkFps = float(continuity_check_count) / (float(continuity_check_end - continuity_check_start) / 1000);
                                continuity_check_count = 0;
                                continuity_check_start = getCurTime();
                            }

                            float happenScore;
                            bool happen = executor->mAnalyzer->checkVideoFrame(cur_is_check, frameCount, frame_bgr->data[0], happenScore);
                            //executor->mAnalyzer->SDLShow(frame_bgr->data[0]);
                            //executor->mAnalyzer->SDLShow(frame_yuv420p->linesize, frame_yuv420p->data);


                            //LOGI("decode 1 frame frameCount=%lld,pktQSize=%d,fps=%d,check=%d,checkFps=%f",
                            //    frameCount, pktQSize, fps, check, executor->mControl->checkFps);

                            if (executor->mControl->pushStream) {
                                executor->mPushStream->pushVideoFrame(frame_bgr->data[0], frame_bgr_buff_size);
                            }
                            executor->mGenerateAlarm->pushVideoFrame(frame_bgr->data[0], frame_bgr_buff_size, happen, happenScore);
                        }
                        else {
                            LOGE("avcodec_receive_frame error : ret=%d", ret);
                        }
                    }
                    else {
                        LOGE("avcodec_send_packet error : ret=%d", ret);
                    }
                }

                // 队列获取的pkt，必须释放!!!
                //av_free_packet(&pkt);//过时
                av_packet_unref(&pkt);
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }


        av_frame_free(&frame_yuv420p);
        //av_frame_unref(frame_yuv420p);
        frame_yuv420p = NULL;

        av_frame_free(&frame_bgr);
        //av_frame_unref(frame_bgr);
        frame_bgr = NULL;


        av_free(frame_bgr_buff);
        frame_bgr_buff = NULL;


        sws_freeContext(sws_ctx_yuv420p2bgr);
        sws_ctx_yuv420p2bgr = NULL;

    }
}