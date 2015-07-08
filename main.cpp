#include <windows.h>
#include "spi00in.h"
#include "webp/decode.h"
#include <cstring>
#include <cstdio>

/*-------------------------------------------------------------------------*/
/* この Plugin の情報 */
/*-------------------------------------------------------------------------*/
static const char *pluginfo[] = {
	"00IN",				/* Plug-in API バージョン */
	"WebP to DIB Ver 1.5 with libwebp-0.2.0 (c)mimizuno",		/* Plug-in名,バージョン及び copyright */
	"*.webp",			/* 代表的な拡張子 ("*.JPG" "*.JPG;*.JPEG" など) */
	"WebP file(*.WEBP)",	/* ファイル形式名 */
};

#pragma pack(push)
#pragma pack(1) //構造体のメンバ境界を1バイトにする
typedef struct BMP_FILE {
	BITMAPFILEHEADER	header;
	BITMAPINFOHEADER	info;
	RGBQUAD				pal[1];
} BMP_FILE;
#pragma pack(pop)

//ヘッダチェック等に必要なサイズ.2KB以内で.
#define HEADBUF_SIZE sizeof(BMP_FILE)

BOOL IsSupportedEx(char *filename, char *data);
int GetPictureInfoEx(long datasize, char *data, struct PictureInfo *lpInfo);
int GetPictureEx(long datasize, HANDLE *pHBInfo, HANDLE *pHBm,
			 SPI_PROGRESS lpPrgressCallback, long lData, char *data);

/* エントリポイント */
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}

/***************************************************************************
 * 画像形式依存の処理.
 * ～Ex関数を画像形式に合わせて書き換える.
 ***************************************************************************/

int getDIBFromWebP(uint8_t *webpdata, long filesize, BITMAPFILEHEADER *fileHeader, BITMAPINFOHEADER *infoHeader, uint8_t **data)
{
	int width, height;
	unsigned long bitLength, bitWidth;
	uint8_t *bitmapdata;
	*data = WebPDecodeBGRA(webpdata, filesize, &width, &height);
	bitWidth = width * 4;
	bitLength = bitWidth;
	
	bitmapdata = (uint8_t*)malloc(sizeof(uint8_t)*bitLength*height);
	memset(bitmapdata, '\0', bitLength*height);
	for (int i=0;i<height;i++) {
		memcpy(bitmapdata+i*bitLength, *data+(height-i-1)*bitWidth, bitWidth);
	}
	free(*data);
	*data = bitmapdata;
	memset(fileHeader, 0, sizeof(fileHeader));
	memset(infoHeader, 0, sizeof(infoHeader));

	fileHeader->bfType = 'M'*256+'B';
	fileHeader->bfSize = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+sizeof(uint8_t)*bitLength*height;
	fileHeader->bfOffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
	fileHeader->bfReserved1 = 0;
	fileHeader->bfReserved2 = 0;

	infoHeader->biSize = 40;
	infoHeader->biWidth = width;
	infoHeader->biHeight = height;
	infoHeader->biPlanes = 1;
	infoHeader->biBitCount = 32;
	infoHeader->biCompression = 0;
	infoHeader->biSizeImage = fileHeader->bfSize;
	infoHeader->biXPelsPerMeter = infoHeader->biYPelsPerMeter = 0;
	infoHeader->biClrUsed = 0;
	infoHeader->biClrImportant = 0;
	return 0;
}
 
/*
ヘッダを見て対応フォーマットか確認.
対応しているものならtrue,そうでなければfalseを返す.
filnameはファイル名(NULLが入っていることもある).
dataはファイルのヘッダで,サイズは HEADBUF_SIZE.
*/
BOOL IsSupportedEx(char *filename, char *data)
{
	char header[] = {'R', 'I', 'F', 'F', 0x00, 0x00, 0x00, 0x00, 'W', 'E', 'B', 'P', 'V', 'P', '8'};
	for (int i=0;i<0x0f;i++) {
		if (header[i] == 0x00) continue;
		if (data[i] != header[i]) {
			return FALSE;
		}
	}
	return TRUE;
}

/*
画像ファイルの情報を書き込みエラーコードを返す.
dataはファイルイメージで,サイズはdatasizeバイト.
*/
int GetPictureInfoEx(long datasize, char *data, struct PictureInfo *lpInfo)
{
	int width, height;
	WebPGetInfo((uint8_t*)data, datasize, &width, &height);

	lpInfo->left		= 0;
	lpInfo->top			= 0;
	lpInfo->width		= width;
	lpInfo->height		= height;
	lpInfo->x_density	= 0;
	lpInfo->y_density	= 0;
	lpInfo->colorDepth	= 32;
	lpInfo->hInfo		= NULL;

	return SPI_ALL_RIGHT;
}

/*
画像ファイルをDIBに変換し,エラーコードを返す.
dataはファイルイメージで,サイズはdatasizeバイト.
*/
int GetPictureEx(long datasize, HANDLE *pHBInfo, HANDLE *pHBm,
			 SPI_PROGRESS lpPrgressCallback, long lData, char *data)
{
	uint8_t *webpdata_u8 = (uint8_t *)data;
	uint8_t *data_u8;
	BITMAPINFOHEADER infoHeader;
	BITMAPFILEHEADER fileHeader;
	BITMAPINFO *pinfo;
	unsigned char *pbmdat;

	if (lpPrgressCallback != NULL)
		if (lpPrgressCallback(1, 1, lData)) /* 100% */
			return SPI_ABORT;

	getDIBFromWebP(webpdata_u8, datasize, &fileHeader, &infoHeader, &data_u8);
	*pHBInfo = LocalAlloc(LMEM_MOVEABLE, sizeof(BITMAPINFOHEADER));
	*pHBm    = LocalAlloc(LMEM_MOVEABLE, fileHeader.bfSize-fileHeader.bfOffBits);
	if (*pHBInfo == NULL || *pHBm == NULL) {
		if (*pHBInfo != NULL) LocalFree(*pHBInfo);
		if (*pHBm != NULL) LocalFree(*pHBm);
		return SPI_NO_MEMORY;
	}
	pinfo  = (BITMAPINFO *)LocalLock(*pHBInfo);
	pbmdat = (unsigned char *)LocalLock(*pHBm);
	if (pinfo == NULL || pbmdat == NULL) {
		LocalFree(*pHBInfo);
		LocalFree(*pHBm);
		return SPI_MEMORY_ERROR;
	}
	/* *pHBInfoにはBITMAPINFOを入れる */
	pinfo->bmiHeader.biSize				= sizeof(BITMAPINFOHEADER);
	pinfo->bmiHeader.biWidth			= infoHeader.biWidth;
	pinfo->bmiHeader.biHeight			= infoHeader.biHeight;
	pinfo->bmiHeader.biPlanes			= 1;
	pinfo->bmiHeader.biBitCount			= 32;
	pinfo->bmiHeader.biCompression		= BI_RGB;
	pinfo->bmiHeader.biSizeImage		= 0;
	pinfo->bmiHeader.biXPelsPerMeter	= 0;
	pinfo->bmiHeader.biYPelsPerMeter	= 0;
	pinfo->bmiHeader.biClrUsed			= 0;
	pinfo->bmiHeader.biClrImportant		= 0;
	/* *pHBmにはビットマップデータを入れる */
	memcpy(pbmdat, data_u8, fileHeader.bfSize-fileHeader.bfOffBits);

	//メモリをアンロック
	LocalUnlock(*pHBInfo);
	LocalUnlock(*pHBm);
	free(data_u8);

	if (lpPrgressCallback != NULL)
		if (lpPrgressCallback(1, 1, lData)) /* 100% */
			return SPI_ABORT;

	return SPI_ALL_RIGHT;

#if 0
	unsigned int infosize;
	int height;
	unsigned int width, bit, linesize, imgsize;
	BITMAPINFO *pinfo;
	unsigned char *pbmdat;
	BMP_FILE *pbmp = (BMP_FILE *)data;

	if (lpPrgressCallback != NULL)
		if (lpPrgressCallback(0, 1, lData)) /* 0% */
			return SPI_ABORT;

	width = pbmp->info.biWidth;
	height = pbmp->info.biHeight;
	bit = pbmp->info.biBitCount;
	infosize = sizeof(BITMAPINFOHEADER);
	if (bit == 8) {
		infosize += sizeof(RGBQUAD)*256;
	}
	linesize = (width * (bit / 8) +3) & ~3; /*4byte境界*/
	imgsize = linesize * abs(height);

	*pHBInfo = LocalAlloc(LMEM_MOVEABLE, infosize);
	*pHBm    = LocalAlloc(LMEM_MOVEABLE, imgsize);

	if (*pHBInfo == NULL || *pHBm == NULL) {
		if (*pHBInfo != NULL) LocalFree(*pHBInfo);
		if (*pHBm != NULL) LocalFree(*pHBm);
		return SPI_NO_MEMORY;
	}

	pinfo  = (BITMAPINFO *)LocalLock(*pHBInfo);
	pbmdat = (unsigned char *)LocalLock(*pHBm);
	if (pinfo == NULL || pbmdat == NULL) {
		LocalFree(*pHBInfo);
		LocalFree(*pHBm);
		return SPI_MEMORY_ERROR;
	}

	/* *pHBInfoにはBITMAPINFOを入れる */
	pinfo->bmiHeader.biSize				= sizeof(BITMAPINFOHEADER);
	pinfo->bmiHeader.biWidth			= width;
	pinfo->bmiHeader.biHeight			= height;
	pinfo->bmiHeader.biPlanes			= 1;
	pinfo->bmiHeader.biBitCount			= bit;
	pinfo->bmiHeader.biCompression		= BI_RGB;
	pinfo->bmiHeader.biSizeImage		= 0;
	pinfo->bmiHeader.biXPelsPerMeter	= 0;
	pinfo->bmiHeader.biYPelsPerMeter	= 0;
	pinfo->bmiHeader.biClrUsed			= 0;
	pinfo->bmiHeader.biClrImportant		= 0;
	if (bit == 8) {
		memcpy(pinfo->bmiColors, pbmp->pal, sizeof(RGBQUAD)*256);
	}
	/* *pHBmにはビットマップデータを入れる */
	memcpy(pbmdat, data +pbmp->header.bfOffBits, imgsize);

	//メモリをアンロック
	LocalUnlock(*pHBInfo);
	LocalUnlock(*pHBm);

	if (lpPrgressCallback != NULL)
		if (lpPrgressCallback(1, 1, lData)) /* 100% */
			return SPI_ABORT;

	return SPI_ALL_RIGHT;
#endif
}
//---------------------------------------------------------------------------

/***************************************************************************
 * SPI関数
 * 決まり切った処理はこちらで済ませている.
 ***************************************************************************/

int __stdcall GetPluginInfo(int infono, LPSTR buf, int buflen)
{
	if (infono < 0 || infono >= (sizeof(pluginfo) / sizeof(char *))) 
		return 0;

	lstrcpyn(buf, pluginfo[infono], buflen);

	return lstrlen(buf);
}

int __stdcall IsSupported(LPSTR filename, DWORD dw)
{
	char *data;
	char buff[HEADBUF_SIZE];
	DWORD ReadBytes;

	if ((dw & 0xFFFF0000) == 0) {
	/* dwはファイルハンドル */
		if (!ReadFile((HANDLE)dw, buff, HEADBUF_SIZE, &ReadBytes, NULL)) {
			return 0;
		}
		data = buff;
	} else {
	/* dwはバッファへのポインタ */
		data = (char *)dw;
	}


	/* フォーマット確認 */
	if (IsSupportedEx(filename, data)) return 1;

	return 0;
}

int __stdcall GetPictureInfo
(LPSTR buf, long len, unsigned int flag, struct PictureInfo *lpInfo)
{
	int ret = SPI_OTHER_ERROR;
	char headbuf[HEADBUF_SIZE];
	char *data;
	char *filename;
	long datasize;
	HANDLE hf;
	DWORD ReadBytes;

	if ((flag & 7) == 0) {
	/* bufはファイル名 */
		hf = CreateFile(buf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hf == INVALID_HANDLE_VALUE) return SPI_FILE_READ_ERROR;
		datasize = GetFileSize(hf, NULL) -len;
		if (datasize < HEADBUF_SIZE) {
			CloseHandle(hf);
			return SPI_NOT_SUPPORT;
		}
		if (SetFilePointer(hf, len, NULL, FILE_BEGIN) != (DWORD)len) {
			CloseHandle(hf);
			return SPI_FILE_READ_ERROR;
		}
		data = (char *)LocalAlloc(LMEM_FIXED, datasize);
		if (data == NULL) {
			CloseHandle(hf);
			return SPI_NO_MEMORY;
		}
		if (!ReadFile(hf, data, datasize, &ReadBytes, NULL)) {
			CloseHandle(hf);
			LocalFree(data);
			return SPI_FILE_READ_ERROR;
		}
		CloseHandle(hf);
		if (ReadBytes != (DWORD)datasize) {
			LocalFree(data);
			return SPI_FILE_READ_ERROR;
		}
		filename = buf;
	} else {
	/* bufはファイルイメージへのポインタ */
		data = buf;
		datasize = len;
		filename = NULL;
	}
#if 0
	if ((flag & 7) == 0) {
	/* bufはファイル名 */
		hf = CreateFile(buf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hf == INVALID_HANDLE_VALUE) return SPI_FILE_READ_ERROR;
		if (SetFilePointer(hf, len, NULL, FILE_BEGIN) != (DWORD)len) {
			CloseHandle(hf);
			return SPI_FILE_READ_ERROR;
		}
		if (!ReadFile(hf, headbuf, HEADBUF_SIZE, &ReadBytes, NULL)) {
			CloseHandle(hf);
			return SPI_FILE_READ_ERROR;
		}
		CloseHandle(hf);
		if (ReadBytes != HEADBUF_SIZE) return SPI_NOT_SUPPORT;
		data = headbuf;
		filename = buf;
	} else {
	/* bufはファイルイメージへのポインタ */
		if (len < HEADBUF_SIZE) return SPI_NOT_SUPPORT;
		data = (char *)buf;
		filename = NULL;
	}
#endif

	/* 一応フォーマット確認 */
	if (!IsSupportedEx(filename, data)) {
		ret = SPI_NOT_SUPPORT;
	} else {
		ret = GetPictureInfoEx(datasize, data, lpInfo);
	}

	return ret;
}

int __stdcall GetPicture(
		LPSTR buf, long len, unsigned int flag, 
		HANDLE *pHBInfo, HANDLE *pHBm,
		SPI_PROGRESS lpPrgressCallback, long lData)
{
	int ret = SPI_OTHER_ERROR;
	char *data;
	char *filename;
	long datasize;
	HANDLE hf;
	DWORD ReadBytes;

	if ((flag & 7) == 0) {
	/* bufはファイル名 */
		hf = CreateFile(buf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hf == INVALID_HANDLE_VALUE) return SPI_FILE_READ_ERROR;
		datasize = GetFileSize(hf, NULL) -len;
		if (datasize < HEADBUF_SIZE) {
			CloseHandle(hf);
			return SPI_NOT_SUPPORT;
		}
		if (SetFilePointer(hf, len, NULL, FILE_BEGIN) != (DWORD)len) {
			CloseHandle(hf);
			return SPI_FILE_READ_ERROR;
		}
		data = (char *)LocalAlloc(LMEM_FIXED, datasize);
		if (data == NULL) {
			CloseHandle(hf);
			return SPI_NO_MEMORY;
		}
		if (!ReadFile(hf, data, datasize, &ReadBytes, NULL)) {
			CloseHandle(hf);
			LocalFree(data);
			return SPI_FILE_READ_ERROR;
		}
		CloseHandle(hf);
		if (ReadBytes != (DWORD)datasize) {
			LocalFree(data);
			return SPI_FILE_READ_ERROR;
		}
		filename = buf;
	} else {
	/* bufはファイルイメージへのポインタ */
		data = buf;
		datasize = len;
		filename = NULL;
	}

	/* 一応フォーマット確認 */
	if (!IsSupportedEx(filename, data)) {
		ret = SPI_NOT_SUPPORT;
	} else {
		ret = GetPictureEx(datasize, pHBInfo, pHBm, lpPrgressCallback, lData, data);
	}

	if ((flag & 7) == 0) LocalFree(data);

	return ret;
}

int __stdcall GetPreview(
	LPSTR buf, long len, unsigned int flag,
	HANDLE *pHBInfo, HANDLE *pHBm,
	SPI_PROGRESS lpPrgressCallback, long lData)
{
	return SPI_NO_FUNCTION;
}

