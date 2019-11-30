/////////////////////////////////////////////////////////////////////////////////////////////
//Lytro DesktopがIllumの撮影データから書き出すカメラRaw画像ファイル（.lfr）を入力し，
//画像センサ（5368×7728画素，10ビット/画素）のデータを読み出し，
//画像センサの生画像を5368×7728画素，8ビット/画素のtifファイル（IllumRaw.tif）として保存する．
//生画像はカラーフィルタアレイ画像である．
//これに，単純なデモザイク処理を施し，RGB画像のtifファイル（IllumRGB.tif）としても保存する．
//
//京都産業大学　蚊野浩
/////////////////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
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

float bayer_G[5][5] = {
	{0, 0, -1, 0, 0},
	{0, 0, 2, 0, 0},
	{-1, 2, 4, 2, -1},
	{ 0, 0, 2, 0, 0 },
	{ 0, 0, -1, 0, 0 },
};

float bayer_RBatGinRrowBcolumn[5][5] = {
	{ 0, 0, 1/2, 0, 0 },
	{ 0, -1, 0, -1, 0 },
	{ -1, 4, 5, 4, -1 },
	{ 0, -1, 0, -1, 0 },
	{ 0, 0, 1/2, 0, 0 },
};

float bayer_RBatGinBrowRcolumn[5][5] = {
	{ 0, 0, -1, 0, 0 },
	{ 0, -1, 4, -1, 0 },
	{ 1/2, 0, 5, 0, 1/2 },
	{ 0, -1, 4, -1, 0 },
	{ 0, 0, -1, 0, 0 },
};

float bayer_RBatB[5][5] = {
	{ 0, 0, -3/2, 0, 0 },
	{ 0, 2, 0, 2, 0 },
	{ -3/2, 0, 6, 0, -3/2 },
	{ 0, 2, 0, 2, 0 },
	{ 0, 0, -3/2, 0, 0 },
};

template <typename T>
T conv(const cv::Mat& mat, int x, int y, const float filter[5][5]) {
	T ret(0);
	if (!filter) {
		return mat.at<T>(y, x);
	}
	for (int dy = -2; dy <= 2; dy++) {
		for (int dx = -2; dx <= 2; dx++) {
			int x2 = std::abs(x + dx);
			int y2 = std::abs(y + dy);
			if (x2 >= mat.cols) {
				x2 = (mat.cols - 1) * 2 - x2;
			}
			if (y2 >= mat.rows) {
				y2 = (mat.cols - 1) * 2 - y2;
			}
			ret += mat.at<T>(y2, x2) * filter[dy + 2][dx + 2];
		}
	}
	return ret / 8.0;
}

int main(int argc, char* argv[]) {
	if (argc < 6) {
		std::cout << "Usage: input(lfr), output raw image, output rgb image, output raw file, output rgb file" << std::endl;
		return 0;
	}

	//IllumのLytroカメラRaw画像ファイルの名前（Lytro　Desktopが書き出すファイルの一つ）．
	const char *file_name = argv[1];

	//ファイルのオープン．
	FILE *fp = fopen(file_name, "rb");
	if (fp == NULL) {
		printf("file (%s) open error\n", file_name);
		return 1;
	}
	printf("file (%s) open success\n", file_name);

	//ファイルサイズを確認する．
	fseek(fp, 0, SEEK_END);
	fpos_t fsize = 0;
	fgetpos(fp, &fsize);

	//全データを*lfr_rawに読み込む．
	fseek(fp, 0, SEEK_SET);
	std::unique_ptr<unsigned char[]> lfr_raw(new unsigned char[fsize]);
	if (fread(lfr_raw.get(), 1, (size_t) fsize, fp) != fsize) {
		printf("file read error\n");
		fclose(fp);
		return 1;
	}
	fclose(fp);

	//バイト列の読み取り位置を示すポインタ．
	int ptr = 0;

	///////////////////////////////////////////////////////////////////////////////////////////////////
	//.lfrファイルは複数のセクションに分かれており，JSONで記述されたファイルの属性などが格納されている．
	//画像センサの生データは第11セクションにあり，今回は，ここだけを利用する．
	///////////////////////////////////////////////////////////////////////////////////////////////////

	//第0セクションを読み飛ばす．
	//各セクションの先頭は12バイトのマジックコードとセクションの長さを示す4バイトが格納されている．
	ptr += 16;

	//第1セクション～第10セクションを読み飛ばす
	for (int i = 1; i <= 10; i++) {
		//セクション間に0が挿入されている場合があるので、そこをスキップする．
		while (lfr_raw[ptr] == 0) ptr++;

		//マジックコードをスキップする．
		ptr += 12;

		//セクションのバイト数を読み取る
		int len = lfr_raw[ptr] * 16777216 + lfr_raw[ptr + 1] * 65536 + lfr_raw[ptr + 2] * 256 + lfr_raw[ptr + 3];
		printf("length of section %d is %d\n", i, len);

		//4バイトのセクションのバイト数，45バイトのsha1 hash，35バイトの0，セクションデータの順に格納されている．
		ptr += (4 + 45 + 35 + len);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//第11セクションに生データが格納されており，それを生画像に変換してtif画像ファイルととして出力する．
	//////////////////////////////////////////////////////////////////////////////////////////////////

	//セクション間に0が挿入されている場合があるので、そこをスキップする．
	while (lfr_raw[ptr] == 0) ptr++;

	//マジックコードをスキップする．
	ptr += 12;

	//セクションのバイト数を読み取る
	int len = lfr_raw[ptr] * 16777216 + lfr_raw[ptr + 1] * 65536 + lfr_raw[ptr + 2] * 256 + lfr_raw[ptr + 3];
	printf("length of section 11 is %d\n", len);
	ptr += (4 + 45 + 35);

	//生画像を格納するcvMatを宣言する
	int v = 5368; //生画像の垂直画素数
	int h = 7728; //生画像の水平画素数
	cv::Mat RawImage = cv::Mat::zeros(v*h, 1, CV_32F);

	//生画像の画素値を4画素単位で計算する．
	for (int i = 0; i < v*h; i += 4) {
		//生データの5バイトに4画素分のデータが格納されている．各画素10ビット．
		unsigned char byte1 = lfr_raw[ptr++];
		unsigned char byte2 = lfr_raw[ptr++];
		unsigned char byte3 = lfr_raw[ptr++];
		unsigned char byte4 = lfr_raw[ptr++];
		unsigned char byte5 = lfr_raw[ptr++];

		//10ビットデータの下位2ビットは，5バイト目（byte5）にパックして格納されている模様．
		unsigned short data1 = byte1 * 4 + (((byte5)&(0x0c0)) >> 6);
		unsigned short data2 = byte2 * 4 + (((byte5)&(0x030)) >> 4);
		unsigned short data3 = byte3 * 4 + (((byte5)&(0x00c)) >> 2);
		unsigned short data4 = byte4 * 4 + ((byte5)&(0x003));

		//今回は単純に上位8ビットを生画像の画素値として保存する．
		RawImage.at<float>(i, 0) = (float) data1;
		RawImage.at<float>(i + 1, 0) = (float) data2;
		RawImage.at<float>(i + 2, 0) = (float) data3;
		RawImage.at<float>(i + 3, 0) = (float) data4;
	}

	//v行×h列にリシェイプし，tifファイルとして保存する．
	RawImage = RawImage.reshape(1, v);
//	cv::imwrite(argv[2], RawImage * 256 / 1024);
//	std::ofstream ofs_raw(argv[4], std::ios::binary);
//	ofs_raw.write(reinterpret_cast<const char*>(RawImage.data), sizeof(float) * v * h);
//	std::cout << argv[2] << " created" << std::endl;

	//////////////////////////////////////////////////////
	//Illumの生画像を簡単な方法でRGB画像にデモザイクする．
	//////////////////////////////////////////////////////

	//RGB画像を記憶するcvMat
	cv::Mat RGBImage = cv::Mat::zeros(v, h, CV_32FC3);

	//各色信号のゲインは第8セクションのJSONに記述されている数値を利用する．
	float r_gain = 1.0f;
	float g_gain = 1.0f;
	float b_gain = 1.0f;

//	cv::cvtColor(RawImage, RGBImage, CV_BayerBG2BGR);

	//2×2の4画素単位でデモザイクを行う
	for (int vv = 0; vv < v; vv += 2) {
		for (int hh = 0; hh < h; hh += 2) {
			float r = r_gain * RawImage.at<float>(vv, hh + 1);
			float g = g_gain * (RawImage.at<float>(vv, hh) + RawImage.at<float>(vv + 1, hh + 1)) / 2.0f;
			float b = b_gain * RawImage.at<float>(vv + 1, hh);

			RGBImage.at<cv::Vec3f>(vv, hh) = cv::Vec3f(b, g, r);
			RGBImage.at<cv::Vec3f>(vv + 1, hh) = cv::Vec3f(b, g, r);
			RGBImage.at<cv::Vec3f>(vv, hh + 1) = cv::Vec3f(b, g, r);
			RGBImage.at<cv::Vec3f>(vv + 1, hh + 1) = cv::Vec3f(b, g, r);
		}
	}

/*	for (int vv = 0; vv < v; vv++) {
		for (int hh = 0; hh < h; hh++) {
			float r = r_gain * conv<float>(RawImage, hh, vv, vv % 2 == 0 ? (hh % 2 == 0 ? bayer_RBatGinRrowBcolumn : nullptr) : (hh % 2 == 0 ? bayer_RBatB : bayer_RBatGinBrowRcolumn));
			float g = g_gain * conv<float>(RawImage, hh, vv, vv % 2 == hh % 2 ? nullptr : bayer_G);
			float b = b_gain * conv<float>(RawImage, hh, vv, vv % 2 == 0 ? (hh % 2 == 0 ? bayer_RBatGinRrowBcolumn : bayer_RBatB) : (hh % 2 == 0 ? nullptr : bayer_RBatGinBrowRcolumn));

			RGBImage.at<cv::Vec3f>(vv, hh) = cv::Vec3f(b, g, r);
		}
	}*/

	//デモザイク画像をtifファイルとして保存する．
//	cv::imwrite(argv[3], RGBImage * 256 / 1024);
	std::ofstream ofs_rgb(argv[5], std::ios::binary);
	ofs_rgb.write(reinterpret_cast<const char*>(RGBImage.data), sizeof(float) * v * h * 3);
	std::cout << argv[3] << " created" << std::endl;

	return 0;
}