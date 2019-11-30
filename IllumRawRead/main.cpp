/////////////////////////////////////////////////////////////////////////////////////////////
//Lytro Desktop��Illum�̎B�e�f�[�^���珑���o���J����Raw�摜�t�@�C���i.lfr�j����͂��C
//�摜�Z���T�i5368�~7728��f�C10�r�b�g/��f�j�̃f�[�^��ǂݏo���C
//�摜�Z���T�̐��摜��5368�~7728��f�C8�r�b�g/��f��tif�t�@�C���iIllumRaw.tif�j�Ƃ��ĕۑ�����D
//���摜�̓J���[�t�B���^�A���C�摜�ł���D
//����ɁC�P���ȃf���U�C�N�������{���CRGB�摜��tif�t�@�C���iIllumRGB.tif�j�Ƃ��Ă��ۑ�����D
//
//���s�Y�Ƒ�w�@���_
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

	//Illum��Lytro�J����Raw�摜�t�@�C���̖��O�iLytro�@Desktop�������o���t�@�C���̈�j�D
	const char *file_name = argv[1];

	//�t�@�C���̃I�[�v���D
	FILE *fp = fopen(file_name, "rb");
	if (fp == NULL) {
		printf("file (%s) open error\n", file_name);
		return 1;
	}
	printf("file (%s) open success\n", file_name);

	//�t�@�C���T�C�Y���m�F����D
	fseek(fp, 0, SEEK_END);
	fpos_t fsize = 0;
	fgetpos(fp, &fsize);

	//�S�f�[�^��*lfr_raw�ɓǂݍ��ށD
	fseek(fp, 0, SEEK_SET);
	std::unique_ptr<unsigned char[]> lfr_raw(new unsigned char[fsize]);
	if (fread(lfr_raw.get(), 1, (size_t) fsize, fp) != fsize) {
		printf("file read error\n");
		fclose(fp);
		return 1;
	}
	fclose(fp);

	//�o�C�g��̓ǂݎ��ʒu�������|�C���^�D
	int ptr = 0;

	//���摜���i�[����cvMat��錾����
	int v = 5368; //���摜�̐�����f��
	int h = 7728; //���摜�̐�����f��
	cv::Mat RawImage = cv::Mat::zeros(v*h, 1, CV_32F);

	//���摜�̉�f�l��4��f�P�ʂŌv�Z����D
	for (int i = 0; i < v*h; i += 4) {
		//���f�[�^��5�o�C�g��4��f���̃f�[�^���i�[����Ă���D�e��f10�r�b�g�D
		unsigned char byte1 = lfr_raw[ptr++];
		unsigned char byte2 = lfr_raw[ptr++];
		unsigned char byte3 = lfr_raw[ptr++];
		unsigned char byte4 = lfr_raw[ptr++];
		unsigned char byte5 = lfr_raw[ptr++];

		//10�r�b�g�f�[�^�̉���2�r�b�g�́C5�o�C�g�ځibyte5�j�Ƀp�b�N���Ċi�[����Ă���͗l�D
		unsigned short data1 = byte1 * 4 + (((byte5)&(0x0c0)) >> 6);
		unsigned short data2 = byte2 * 4 + (((byte5)&(0x030)) >> 4);
		unsigned short data3 = byte3 * 4 + (((byte5)&(0x00c)) >> 2);
		unsigned short data4 = byte4 * 4 + ((byte5)&(0x003));

		//����͒P���ɏ��8�r�b�g�𐶉摜�̉�f�l�Ƃ��ĕۑ�����D
		RawImage.at<float>(i, 0) = (float) data1;
		RawImage.at<float>(i + 1, 0) = (float) data2;
		RawImage.at<float>(i + 2, 0) = (float) data3;
		RawImage.at<float>(i + 3, 0) = (float) data4;
	}

	//v�s�~h��Ƀ��V�F�C�v���Ctif�t�@�C���Ƃ��ĕۑ�����D
	RawImage = RawImage.reshape(1, v);
	cv::imwrite(argv[2], RawImage * scale);
	std::cout << argv[2] << " created" << std::endl;

	return 0;
}