## "Eagle Eye" A Real Time Stream Media Analysis Framework



#### Briefing

This is a stream media framework that connects all kinds of edge devices/user touch points with stream protocols like RTSP, RTP and RTMP. In the future it will support even more stream protocols.

The given codes is a demo for real time video analysis, it connects a edge camera, Machine Learning platform and a user admin portal. The data flow is this: 

1. Edge cameras send real time video to the Eagle Eye via RTMP protocol
2. Eagle Eye send the video to algorithm server for detecting frame by frame
3. If there are any abnormalities, Eagle Eye will compose the related frames as a short video and send it to the admin portal.



#### System Diagram

![image-20231023131738755](https://hansomehu-picgo.oss-cn-hangzhou.aliyuncs.com/typora/image-20231023131738755.png)



#### Compilation

Mac & Linux (all required x64)



#### Required Libraries(Analyzer Module C++)

1.  ffmpeg-4.4
2.  curl-7.83.0
3.  event
4.  jpeg-turbo
5.  jsoncpp
6.  opencv-3.4.10

`* `  Lib without the version label means you are free to use the latest one, there'll be no potential conflict.
