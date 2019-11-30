#include <iostream>
#include <memory>
#include <fstream>

#include <opencv/cv.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#ifdef _DEBUG
#pragma comment(lib,"opencv_core2411d.lib")
#pragma comment(lib,"opencv_highgui2411d.lib")
#pragma comment(lib,"opencv_imgproc2411d.lib")
#else
#pragma comment(lib,"opencv_core2411.lib")
#pragma comment(lib,"opencv_highgui2411.lib")
#pragma comment(lib,"opencv_imgproc2411.lib")
#endif

constexpr int resX = 7728, resY = 5368;

cv::Vec2f leftTop, rightTop, leftBottom, rightBottom, center;
cv::Vec2f lensSize;
cv::Vec2i lensNum;
//float focalLength, mlaFocalLength;
cv::Vec3f whiteBalance(1.0f, 1.0f, 1.0f);
float intensity = 1.0f;
int channels = 3;

void readParams(const std::string& paramfile) {
	cv::FileStorage fs(paramfile, cv::FileStorage::READ);
	if (!fs.isOpened()) {
		std::cerr << "Param file: " << paramfile << " not found or corrupted." << std::endl;
		return;
	}

	channels = (int) fs["channels"];
	leftTop[0] = (float) fs["LeftTopX"];
	leftTop[1] = (float) fs["LeftTopY"];
	rightBottom[0] = (float) fs["RightBottomX"];
	rightBottom[1] = (float) fs["RightBottomY"];
	leftBottom[0] = (float) fs["LeftBottomX"];
	leftBottom[1] = (float) fs["LeftBottomY"];
	rightTop[0] = (float) fs["RightTopX"];
	rightTop[1] = (float) fs["RightTopY"];
	lensNum[0] = (int) fs["LensNumX"];
	lensNum[1] = (int) fs["LensNumY"];
//	focalLength = (float) fs["FocalLength"];
//	mlaFocalLength = (float) fs["MLAFocalLength"];
	lensSize[0] = (rightTop[0] - leftTop[0] + rightBottom[0] - leftBottom[0]) / 2.0f / (float) lensNum[0];
	lensSize[1] = (leftBottom[1] - leftTop[1] + rightBottom[1] - rightTop[1]) / 2.0f / (float) lensNum[1];

	if (lensNum[1] % 2 == 0) {
		leftBottom[0] -= lensSize[0] / 2.0;
		rightBottom[0] += lensSize[0] / 2.0;
	}
	center = (leftTop + rightTop + leftBottom + rightBottom) / 4.0f;

	whiteBalance[0] = (float) fs["WhiteBalanceB"];
	whiteBalance[1] = (float) fs["WhiteBalanceG"];
	whiteBalance[2] = (float) fs["WhiteBalanceR"];
	intensity = (float) fs["IntensityScale"];
}

int main(int argc, char* argv[]) {
	if (argc < 7) {
		std::cout << "Usage: input(bin), param.yml, apertX, apertY, output image, output image (super resolution)" << std::endl;
		return 0;
	}

	readParams(argv[2]);
	std::cout << "LensSize: " << lensSize << std::endl;

	cv::Vec2f apertPos(std::stof(argv[3]), std::stof(argv[4]));

	cv::Mat image = cv::Mat::zeros(resY, resX, channels == 3 ? CV_32FC3 : CV_32F);
	std::ifstream ifs(argv[1], std::ios::binary);
	ifs.read(reinterpret_cast<char*>(image.data), sizeof(float) * resX * resY * channels);

	cv::Mat outImage = cv::Mat::zeros(lensNum[1], lensNum[0], channels == 3 ? CV_32FC3 : CV_32F);
	for (int y = 0; y < lensNum[1]; y++) {
		for (int x = 0; x < lensNum[0]; x++) {
			float u = (float) x / (float) (lensNum[0] - 1), v = (float) y / (float) (lensNum[1] - 1);
			cv::Vec2f pos = (1.0f - u) * (1.0f - v) * leftTop + u * (1.0f - v) * rightTop + (1.0f - u) * v * leftBottom + u * v * rightBottom;
			if (y % 2 == 1) {
				pos[0] += lensSize[0] / 2.0f;
			}
//			auto diff = ((pos - center) - apertPos) * mlaFocalLength / focalLength;
//			pos += diff;
			if (pos[0] < 0 || resX <= pos[0] || pos[1] < 0 || resY <= pos[1]) {
				continue;
			}
			if (channels == 3) {
				const auto& a = image.at<cv::Vec3f>(std::floor(pos[1]), std::floor(pos[0]));
				const auto& b = image.at<cv::Vec3f>(std::floor(pos[1]), std::ceil(pos[0]));
				const auto& c = image.at<cv::Vec3f>(std::ceil(pos[1]), std::floor(pos[0]));
				const auto& d = image.at<cv::Vec3f>(std::ceil(pos[1]), std::ceil(pos[0]));
				float u = pos[0] - std::floor(pos[0]), v = pos[1] - std::floor(pos[1]);
				outImage.at<cv::Vec3f>(y, x) += ((1.0f - u) * (1.0f - v) * a + u * (1.0f - v) * b + (1.0f - u) * v * c + u * v * d).mul(whiteBalance) * intensity;
			} else {
				const auto& a = image.at<float>(std::floor(pos[1]), std::floor(pos[0]));
				const auto& b = image.at<float>(std::floor(pos[1]), std::ceil(pos[0]));
				const auto& c = image.at<float>(std::ceil(pos[1]), std::floor(pos[0]));
				const auto& d = image.at<float>(std::ceil(pos[1]), std::ceil(pos[0]));
				float u = pos[0] - std::floor(pos[0]), v = pos[1] - std::floor(pos[1]);
				outImage.at<float>(y, x) += ((1.0f - u) * (1.0f - v) * a + u * (1.0f - v) * b + (1.0f - u) * v * c + u * v * d) * intensity;
			}
		}
	}

	cv::imwrite(argv[5], outImage);

	cv::Mat outImage2 = cv::Mat::zeros(lensNum[1], lensNum[0] * 2, channels == 3 ? CV_32FC3 : CV_32F);
	for (int y = 0; y < lensNum[1] / 2; y++) {
		for (int x = 0; x < lensNum[0] - 1; x++) {
			if (channels == 3) {
				const auto& a = outImage.at<cv::Vec3f>(y * 2, x);
				const auto& b = outImage.at<cv::Vec3f>(y * 2, x + 1);
				const auto& c = outImage.at<cv::Vec3f>(y * 2 + 1, x);
				const auto& d = outImage.at<cv::Vec3f>(y * 2 + 1, x + 1);

				outImage2.at<cv::Vec3f>(y * 2, x * 2) = a;
				outImage2.at<cv::Vec3f>(y * 2, x * 2 + 1) = (a + b) / 2;
				outImage2.at<cv::Vec3f>(y * 2 + 1, x * 2 + 1) = c;
				outImage2.at<cv::Vec3f>(y * 2 + 1, x * 2 + 2) = (c + d) / 2;
			} else {
				const auto& a = outImage.at<float>(y * 2, x);
				const auto& b = outImage.at<float>(y * 2, x + 1);
				const auto& c = outImage.at<float>(y * 2 + 1, x);
				const auto& d = outImage.at<float>(y * 2 + 1, x + 1);

				outImage2.at<float>(y * 2, x * 2) = a;
				outImage2.at<float>(y * 2, x * 2 + 1) = (a + b) / 2;
				outImage2.at<float>(y * 2 + 1, x * 2 + 1) = c;
				outImage2.at<float>(y * 2 + 1, x * 2 + 2) = (c + d) / 2;
			}
		}
	}

	cv::Mat ParallaxImage = cv::Mat((int) (lensNum[1] * sqrt(3.0) + 0.5), 2 * lensNum[0], channels == 3 ? CV_32FC3 : CV_32F);
	cv::resize(outImage2, ParallaxImage, ParallaxImage.size(), 0, 0, cv::INTER_CUBIC);

	cv::imwrite(argv[6], ParallaxImage);

	return 0;
}
