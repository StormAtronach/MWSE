#pragma once

namespace NI {
	// Locked count writes at the engine's inlined NiRefObject count changes: each site becomes a jump to a stub that
	// does the same work with a lock prefix.
	struct RefCountLocks {
		struct Site {
			DWORD address;
			unsigned int length;
			const char* function;
			const char* slot;
		};

		// The code of one stub, and the number of site bytes it stands for (0: the bytes match no form).
		struct Stub {
			static constexpr size_t MAX_SIZE = 32;

			unsigned int siteLength = 0;
			unsigned int size = 0;
			BYTE code[MAX_SIZE] = {};

			void append(std::initializer_list<BYTE> bytes);
			void appendRelative(DWORD stubAddress, DWORD target);
		};

		static constexpr size_t SITE_COUNT = 40;
		static constexpr size_t READ_LENGTH = 16;

		static const Site sites[SITE_COUNT];

		// Checks the bytes of every site, then locks all of them, or none. Returns the number of sites locked.
		static size_t install();

		// The stub for the site at siteAddress whose first READ_LENGTH bytes are siteBytes, placed at stubAddress.
		static Stub buildStub(DWORD siteAddress, const BYTE* siteBytes, DWORD stubAddress);

	private:
		static bool buildIncrementOrDecrement(DWORD siteAddress, const BYTE* siteBytes, DWORD stubAddress, Stub& stub);
		static bool buildPlainRelease(const BYTE* siteBytes, Stub& stub);
		static bool buildStoreThenIncrement(const BYTE* siteBytes, Stub& stub);
		static bool buildInterleavedRelease(const BYTE* siteBytes, Stub& stub);

		static void logRefusal(const Site& site, const char* reason);
	};
}
