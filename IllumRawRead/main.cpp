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

int main(int argc, char* argv[]) {
	if (argc < 3) {
		std::cout << "Usage: input(raw), output raw image, scale(=1.0)" << std::endl;
		return 0;
	}

	float scale = 1.0f;
	if (argc >= 4) {
		scale = std::stof(argv[3]);
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
	cv::imwrite(argv[2], RawImage * scale);
	std::cout << argv[2] << " created" << std::endl;

	return 0;
}