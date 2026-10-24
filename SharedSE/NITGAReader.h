#pragma once

#include "NIFile.h"
#include "NIPixelFormat.h"

namespace NI {
	struct TGAReader {
		void* vTable; // 0x0
		unsigned char idLength; // 0x4
		unsigned char colorMapType; // 0x5
		unsigned char imageType; // 0x6
		unsigned short minPaletteIndex; // 0x8
		unsigned short paletteLength; // 0xA
		unsigned char colorSize; // 0xC
		short originX; // 0xE
		short originY; // 0x10
		unsigned short width; // 0x12
		unsigned short height; // 0x14
		unsigned char pixelSize; // 0x16
		unsigned char attributeBits; // 0x17
		unsigned char bytesPerPixel; // 0x18
		bool alpha; // 0x19
		bool colorMapped; // 0x1A
		bool flipVertical; // 0x1B
		bool runLengthEncoded; // 0x1C
		PixelFormat format; // 0x20
		int rawDataMax; // 0x40
		unsigned char* rawData; // 0x44
		int colorMapMax; // 0x48
		unsigned char* colorMap; // 0x4C
		void(__thiscall* unpacker)(TGAReader*, unsigned char*, unsigned char*); // 0x50
		int runLengthCount; // 0x54
		unsigned char runLengthEncodedRun; // 0x58
		unsigned char runLengthBits[4]; // 0x59

#if defined(SE_NI_TGAREADER_FNADDR_DECODEHEADER) && SE_NI_TGAREADER_FNADDR_DECODEHEADER > 0
		bool decodeHeader(File* file, unsigned int* outWidth, unsigned int* outHeight, PixelFormat* outFormat, bool* outMipmaps);
		static constexpr auto _decodeHeader = reinterpret_cast<bool(__thiscall*)(TGAReader*, File*, unsigned int*, unsigned int*, PixelFormat*, bool*)>(SE_NI_TGAREADER_FNADDR_DECODEHEADER);
#endif
	};
	static_assert(sizeof(TGAReader) == 0x60, "NI::TGAReader failed size validation");
}
