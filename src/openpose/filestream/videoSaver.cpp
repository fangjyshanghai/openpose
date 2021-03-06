#include <opencv2/highgui/highgui.hpp> // cv::VideoWriter
#include <openpose/filestream/imageSaver.hpp>
#include <openpose/utilities/fileSystem.hpp>
#include <openpose/utilities/string.hpp>
#include <openpose/filestream/videoSaver.hpp>

namespace op
{
    const auto RANDOM_TEXT = "_r8904530ijyiopf9034jiop4g90j0yh795640h38j";

    struct VideoSaver::ImplVideoSaver
    {
        const std::string mVideoSaverPath;
        const int mCvFourcc;
        const double mFps;
        const std::string mAddAudioFromThisVideo;
        const bool mUseFfmpeg;
        Point<int> mCvSize;
        bool mVideoStarted;
        unsigned long long mImageSaverCounter;
        cv::VideoWriter mVideoWriter;
        std::unique_ptr<ImageSaver> upImageSaver;
        std::string mTempImageFolder;

        ImplVideoSaver(const std::string& videoSaverPath, const int cvFourcc, const double fps,
                       const std::string& addAudioFromThisVideo) :
            mVideoSaverPath{videoSaverPath},
            mCvFourcc{cvFourcc},
            mFps{fps},
            mAddAudioFromThisVideo{addAudioFromThisVideo},
            mUseFfmpeg{toLower(getFileExtension(videoSaverPath)) == "mp4"},
            mVideoStarted{false},
            mImageSaverCounter{0ull}
        {
            try
            {
                if (mUseFfmpeg)
                    mTempImageFolder = getFullFilePathNoExtension(mVideoSaverPath) + RANDOM_TEXT;
            }
            catch (const std::exception& e)
            {
                error(e.what(), __LINE__, __FUNCTION__, __FILE__);
            }
        }
    };

    cv::VideoWriter openVideo(const std::string& videoSaverPath, const int cvFourcc, const double fps,
                              const Point<int>& cvSize)
    {
        try
        {
            // Open video
            const cv::VideoWriter videoWriter{
                videoSaverPath, cvFourcc, fps, cv::Size{cvSize.x, cvSize.y}};
            // Check it was successfully opened
            if (!videoWriter.isOpened())
            {
                const std::string errorMessage{
                    "Video to write frames could not be opened as `" + videoSaverPath + "`. Please, check that:"
                    "\n\t1. The path ends in `.avi`.\n\t2. The parent folder exists.\n\t3. OpenCV is properly"
                    " compiled with the FFmpeg codecs in order to save video."
                    "\n\t4. You are not saving in a protected folder. If you desire to save a video in a"
                    " protected folder, use sudo (Ubuntu) or execute the binary file as administrator (Windows)."};
                error(errorMessage, __LINE__, __FUNCTION__, __FILE__);
            }
            // Return video
            return videoWriter;
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
            return cv::VideoWriter{};
        }
    }

    VideoSaver::VideoSaver(const std::string& videoSaverPath, const int cvFourcc, const double fps,
                           const std::string& addAudioFromThisVideo) :
        upImpl{new ImplVideoSaver{videoSaverPath, cvFourcc, fps, addAudioFromThisVideo}}
    {
        try
        {
            // Sanity checks
            if (fps <= 0.)
                error("Desired fps (frame rate) to save the video is <= 0.", __LINE__, __FUNCTION__, __FILE__);
            #ifdef _WIN32
                if (upImpl->mUseFfmpeg)
                    error("MP4 recording requires an Ubuntu or Mac machine.", __LINE__, __FUNCTION__, __FILE__);
            #endif
            if (upImpl->mUseFfmpeg && system("ffmpeg --help") != 0)
                error("In order to save the video in MP4 format, FFmpeg must be installed on your system."
                      " Please, use an `avi` output format (e.g., `--write_video output.avi`) or install FFmpeg"
                      " by running `sudo apt-get install ffmpeg` (Ubuntu) or an analogous command.",
                      __LINE__, __FUNCTION__, __FILE__);
            if (!upImpl->mAddAudioFromThisVideo.empty() && !upImpl->mUseFfmpeg)
                error("In order to save the video with audio, it must be in MP4 format. So either 1) do not set"
                      " `--write_video_audio` or 2) make sure `--write_video` finishes in `.mp4`.",
                      __LINE__, __FUNCTION__, __FILE__);
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
        }
    }

    VideoSaver::~VideoSaver()
    {
        try
        {
            // Images --> Video
            if (upImpl->mUseFfmpeg)
            {
                log("JPG images temporarily generated in " + upImpl->mTempImageFolder + ".", op::Priority::High);
                // FFmpeg command: Save video from images (override if video with same name exists)
                const std::string imageToVideoCommand = "ffmpeg -y -i " + upImpl->mTempImageFolder + "/%12d_rendered.jpg"
                    + " -c:v libx264 -framerate " + std::to_string(upImpl->mFps) + " -pix_fmt yuv420p "
                    + upImpl->mVideoSaverPath;
                log("Creating MP4 video out of JPG images by running:\n" + imageToVideoCommand + "\n",
                    op::Priority::High);
                auto codeAnswer = system(imageToVideoCommand.c_str());
                // Remove temporary images
                if (codeAnswer == 0)
                {
                    codeAnswer = system(("rm -rf " + upImpl->mTempImageFolder).c_str());
                    log("Video saved and temporary image folder removed.", op::Priority::High);
                }
                // Sanity check
                if (codeAnswer != 0)
                    log("\nVideo " + upImpl->mVideoSaverPath + " could not be saved (exit code: "
                        + std::to_string(codeAnswer) + "). Make sure you can manually run the following command"
                        " (with no errors) from the terminal:\n" + imageToVideoCommand, op::Priority::High);
                // Video (no sound) --> Video (with sound)
                if (!upImpl->mAddAudioFromThisVideo.empty())
                {
                    const auto tempOutput = upImpl->mVideoSaverPath + RANDOM_TEXT + ".mp4";
                    const auto audioCommand = "ffmpeg -y -i " + upImpl->mVideoSaverPath
                        + " -i " + upImpl->mAddAudioFromThisVideo + " -codec copy -shortest " + tempOutput;
                    log("Adding audio to video by running:\n" + audioCommand, op::Priority::High);
                    auto codeAnswer = system(audioCommand.c_str());
                    // Move temp output to real output
                    if (codeAnswer == 0)
                        codeAnswer = system(("mv " + tempOutput + " " + upImpl->mVideoSaverPath).c_str());
                    // Sanity check
                    if (codeAnswer != 0)
                        log("\nVideo " + upImpl->mVideoSaverPath + " could not be saved with audio (exit code: "
                            + std::to_string(codeAnswer) + "). Make sure you can manually run the following command"
                            " (with no errors) from the terminal:\n" + audioCommand, op::Priority::High);
                }
            }
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
        }
    }

    bool VideoSaver::isOpened()
    {
        try
        {
            // FFmpeg video
            if (upImpl->mUseFfmpeg)
                return (upImpl->upImageSaver != nullptr);
            // OpenCV video
            else
                return upImpl->mVideoWriter.isOpened();
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
            return false;
        }
    }

    void VideoSaver::write(const cv::Mat& cvMat)
    {
        try
        {
            write(std::vector<cv::Mat>{cvMat});
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
        }
    }

    void VideoSaver::write(const std::vector<cv::Mat>& cvMats)
    {
        try
        {
            // Sanity check
            if (cvMats.empty())
                error("The image(s) to be saved cannot be empty.", __LINE__, __FUNCTION__, __FILE__);
            for (const auto& cvMat : cvMats)
                if (cvMat.empty())
                    error("The image(s) to be saved cannot be empty.", __LINE__, __FUNCTION__, __FILE__);
            // Open video (1st frame)
            // Done here and not in the constructor to handle cases where the resolution is not known (e.g.,
            // reading images or multiple cameras)
            if (!upImpl->mVideoStarted)
            {
                upImpl->mVideoStarted = true;
                const auto cvSize = cvMats.at(0).size();
                upImpl->mCvSize = Point<int>{(int)cvMats.size()*cvSize.width, cvSize.height};
                // FFmpeg video
                if (upImpl->mUseFfmpeg)
                {
                    log("Temporarily saving video frames as JPG images in: " + upImpl->mTempImageFolder,
                        op::Priority::High);
                    upImpl->upImageSaver.reset(new ImageSaver{upImpl->mTempImageFolder, "jpg"});
                }
                // OpenCV video
                else
                    upImpl->mVideoWriter = openVideo(
                        upImpl->mVideoSaverPath, upImpl->mCvFourcc, upImpl->mFps, upImpl->mCvSize);
            }
            // Sanity check
            if (!isOpened())
                error("Video to write frames is not opened.", __LINE__, __FUNCTION__, __FILE__);
            // Concat images
            cv::Mat cvOutputData;
            if (cvMats.size() > 1)
                cv::hconcat(cvMats.data(), cvMats.size(), cvOutputData);
            else
                cvOutputData = cvMats.at(0);
            // Sanity check
            if (upImpl->mCvSize.x != cvOutputData.cols || upImpl->mCvSize.y != cvOutputData.rows)
                error("You selected to write video (`--write_video`), but the frames to be saved have different"
                      " resolution. You can only save frames with the same resolution.",
                      __LINE__, __FUNCTION__, __FILE__);
            // Save concatenated image
            // FFmpeg video
            if (upImpl->mUseFfmpeg)
            {
                upImpl->upImageSaver->saveImages(cvOutputData, toFixedLengthString(upImpl->mImageSaverCounter, 12u));
                upImpl->mImageSaverCounter++;
            }
            // OpenCV video
            else
                upImpl->mVideoWriter.write(cvOutputData);
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
        }
    }
}
