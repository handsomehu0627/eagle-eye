#include "Analyzer.h"
#include <opencv2/opencv.hpp>
#include <json/json.h>
#include "Utils/Request.h"
#include "Utils/Log.h"
#include "Utils/Common.h"
#include "Utils/Base64.h"
#include "Scheduler.h"
#include "Config.h"
#include "Control.h"

#ifndef WIN32
#include <opencv2/opencv.hpp>
#else
#ifndef _DEBUG
#include <turbojpeg.h>
#else
#include <opencv2/opencv.hpp>
#endif
#endif

namespace AVSAnalyzer {

    static bool analy_turboJpeg_compress(int height, int width, int channels, unsigned char* bgr, unsigned char*& out_data, unsigned long* out_size) {
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
#endif

        return false;
    }

    static bool analy_compressBgrAndEncodeBase64(int height, int width, int channels, unsigned char* bgr, std::string& out_base64) {

#ifdef WIN32
#ifndef _DEBUG
        unsigned char* jpeg_data = nullptr;
        unsigned long  jpeg_size = 0;

        analy_turboJpeg_compress(height, width, channels, bgr, jpeg_data, &jpeg_size);

        if (jpeg_size > 0 && jpeg_data != nullptr) {

            Base64Encode(jpeg_data, jpeg_size, out_base64);

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

        Base64Encode(jpeg_data.data(), jpeg_data.size(), out_base64);

        return true;
#endif //WIN32
    }

    AlgorithmWithApi::AlgorithmWithApi(Config* config):mConfig(config)
    {
        LOGI("");
    }

    AlgorithmWithApi::~AlgorithmWithApi()
    {
        LOGI("");
    }
    bool AlgorithmWithApi::test() {
        std::string response;
        int randIndex = rand() % mConfig->algorithmApiHosts.size();
        std::string host = mConfig->algorithmApiHosts[randIndex];
        std::string url = host + "/image/objectDetect";

        Request request;
        bool ret = request.get(url.data(), response);

        //LOGI("ret=%d,response=%s",ret,response.data());
        return ret;

    }

    bool AlgorithmWithApi::objectDetect(int height, int width, unsigned char* bgr, std::vector<AlgorithmDetectObject>& detects) {
        cv::Mat image(height, width, CV_8UC3, bgr);

        int64_t t1 = getCurTime();
        std::string imageBase64;
        analy_compressBgrAndEncodeBase64(image.rows, image.cols, 3, image.data, imageBase64);
        int64_t t2 = getCurTime();

        int randIndex = rand() % mConfig->algorithmApiHosts.size();
        std::string host = mConfig->algorithmApiHosts[randIndex];
        std::string url = host+"/image/objectDetect";

        Json::Value param;
        param["appKey"] = "s84dsd#7hf34r3jsk@fs$d#$dd";
        param["algorithm"] = "openvino_yolov5";
        param["image_base64"] = imageBase64;
        std::string data = param.toStyledString();
        param = NULL;

        int64_t t3 = getCurTime();
        Request request;
        std::string response;
        bool result = request.post(url.data(), data.data(), response);
        int64_t t4 = getCurTime();

        if (result) {
            result = this->parseObjectDetect(response, detects);
        }

        //LOGI("serialize spend: %lld(ms),call api spend: %lld(ms)", (t2 - t1), (t4 - t3));

        return result;
    }
    bool AlgorithmWithApi::parseObjectDetect(std::string& response, std::vector<AlgorithmDetectObject>& detects) {

        Json::CharReaderBuilder builder;
        const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

        //Json::CharReaderBuilder b;
        //Json::CharReader* reader(b.newCharReader());

        Json::Value root;
        JSONCPP_STRING errs;

        bool result = false;

        if (reader->parse(response.data(), response.data() + std::strlen(response.data()),
            &root, &errs) && errs.empty()) {


            if (root["code"].isInt()) {
                int code = root["code"].asInt();
                std::string msg = root["msg"].asString();

                if (code == 1000) {
                    Json::Value root_result = root["result"];
                    int detect_num = root_result["detect_num"].asInt();
                    Json::Value detect_data = root_result["detect_data"];

                    for (auto i : detect_data) {
                        AlgorithmDetectObject object;

                        Json::Value loc = i["location"];
                        object.x1 = loc["x1"].asInt();
                        object.y1 = loc["y1"].asInt();
                        object.x2 = loc["x2"].asInt();
                        object.y2 = loc["y2"].asInt();
                        object.score = i["score"].asFloat();
                        object.class_name = i["class_name"].asString();

                        detects.push_back(object);

                    }
                    result = true;
                }
            }


        }

        root = NULL;
        //delete reader;
        //reader = NULL;

        return result;
    }


    Analyzer::Analyzer(Scheduler* scheduler, Control* control) :
        mScheduler(scheduler),
        mControl(control)
    {
        mAlgorithm = new AlgorithmWithApi(scheduler->getConfig());
    }

    Analyzer::~Analyzer()
    {
        if (mAlgorithm) {
            delete mAlgorithm;
            mAlgorithm = nullptr;
        }
        mDetects.clear();

    }

    bool Analyzer::checkVideoFrame(bool check, int64_t frameCount, unsigned char* data, float& happenScore) {
        bool happen = false;

        cv::Mat image(mControl->videoHeight, mControl->videoWidth, CV_8UC3, data);
        //cv::Mat image = cv::imread("D:\\file\\data\\images\\1.jpg");
        //cv::imshow("image", image);
        //cv::waitKey(0);
        //cv::destroyAllWindows();

        //int target_width = 300;
        //int target_height = 200;
        //int target_left = (mWidth - target_width) / 2;
        //int target_top = (mHeight - target_height) / 2;

        //cv::rectangle(image,cv::Rect(target_left,target_top,target_width,target_height),
        //              cv::Scalar(0,255,0),3
        //        ,cv::LINE_8,0);

        if (check) {
            mDetects.clear();
            mAlgorithm->objectDetect(mControl->videoHeight, mControl->videoWidth, data, mDetects);

            //当检测到视频中有两个人的时候，认为发生了危险行为
            if (mDetects.size() == 2) {
                //LOGI("当前帧发现了危险行为");
                happen = true;
                happenScore = 0.9;
            }

        }
        int x1, y1, x2, y2;
        for (int i = 0; i < mDetects.size(); i++)
        {
            x1 = mDetects[i].x1;
            y1 = mDetects[i].y1;
            x2 = mDetects[i].x2;
            y2 = mDetects[i].y2;
            cv::rectangle(image, cv::Rect(x1, y1, (x2 - x1), (y2 - y1)), cv::Scalar(0, 255, 0), 2, cv::LINE_8, 0);

            std::string class_name = mDetects[i].class_name + "-" + std::to_string(mDetects[i].score);
            cv::putText(image, class_name, cv::Point(x1, y1 + 15), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);

        }
        std::string info = "checkFps:" + std::to_string(mControl->checkFps);
        cv::putText(image, info, cv::Point(20, 40), cv::FONT_HERSHEY_COMPLEX, mControl->videoWidth / 1000, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);

        return happen;

    }
    bool Analyzer::checkAudioFrame(bool check, int64_t frameCount, unsigned char* data, int size) {

        return false;
    }


}