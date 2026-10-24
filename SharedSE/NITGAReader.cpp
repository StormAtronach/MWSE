#include "NITGAReader.h"

namespace NI {
#if defined(SE_NI_TGAREADER_FNADDR_DECODEHEADER) && SE_NI_TGAREADER_FNADDR_DECODEHEADER > 0
	bool TGAReader::decodeHeader(File* file, unsigned int* outWidth, unsigned int* outHeight, PixelFormat* outFormat, bool* outMipmaps) {
		if (!_decodeHeader(this, file, outWidth, outHeight, outFormat, outMipmaps)) {
			return false;
		}
		return unpacker != nullptr;
	}
#endif
}
