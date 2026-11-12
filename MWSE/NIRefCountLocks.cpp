#include "NIRefCountLocks.h"

#include "Log.h"
#include "MemoryUtil.h"

namespace NI {
	const RefCountLocks::Site RefCountLocks::sites[SITE_COUNT] = {
		{ 0x6FE39F, 5, "NiPropertyState::reset", "default alpha" },
		{ 0x6FE3BF, 5, "NiPropertyState::reset", "default fog" },
		{ 0x6FE3DF, 5, "NiPropertyState::reset", "default material" },
		{ 0x6FE3FF, 5, "NiPropertyState::reset", "default stencil" },
		{ 0x6FE41F, 5, "NiPropertyState::reset", "default texturing" },
		{ 0x6FE43F, 5, "NiPropertyState::reset", "default vertex colour" },
		{ 0x6FE45F, 5, "NiPropertyState::reset", "default wireframe" },
		{ 0x6FE475, 7, "NiPropertyState::reset", "the old zbuffer (release)" },
		{ 0x6FE489, 5, "NiPropertyState::reset", "default zbuffer" },
		{ 0x6FE49F, 7, "NiPropertyState::reset", "the old renderer specific (release)" },
		{ 0x6FE4B3, 5, "NiPropertyState::reset", "default renderer specific" },
		{ 0x6FE2E6, 7, "NiPropertyState::ctor_copy", "the old property (release)" },
		{ 0x6FE2FC, 5, "NiPropertyState::ctor_copy", "each property of the copied state" },
		{ 0x471B25, 5, "NiPointer::set", "the new object" },
		{ 0x471B12, 5, "NiPointer::set", "the old object (release)" },
		{ 0x40A886, 5, "NiPointer::dtor", "the object (release)" },
		{ 0x404630, 5, "NiObject::release", "the object (release)" },
		{ 0x6EB230, 5, "NiAVObject::PushLocalProperties", "the parent state" },
		{ 0x6EB30D, 15, "NiAVObject::PushLocalProperties", "the parent state (release)" },
		{ 0x6EB333, 5, "NiAVObject::PushLocalProperties", "the new state" },
		{ 0x6EB349, 7, "NiAVObject::PushLocalProperties", "the new state (release)" },
		{ 0x6C8D82, 5, "NiNode::UpdateProperties", "the state" },
		{ 0x6C8D94, 15, "NiNode::UpdateProperties", "the local state (release)" },
		{ 0x6C8E05, 7, "NiNode::UpdateProperties", "the state (release)" },
		{ 0x6F41F6, 7, "NiGeometry::UpdatePropertiesDownward", "the geometry's old state (release)" },
		{ 0x6F420F, 5, "NiGeometry::UpdatePropertiesDownward", "the geometry's state" },
		{ 0x6F4224, 15, "NiGeometry::UpdatePropertiesDownward", "the state argument (release)" },
		{ 0x6EB125, 5, "NiAVObject::UpdateProperties", "the state" },
		{ 0x6EB137, 13, "NiAVObject::UpdateProperties", "the local state (release)" },
		{ 0x6EB19A, 7, "NiAVObject::UpdateProperties", "the state (release)" },
		{ 0x6F3F5E, 7, "NiGeometry::pushStateToRenderer", "the old current property state (release)" },
		{ 0x6F3F6D, 10, "NiGeometry::pushStateToRenderer", "the shape's property state" },
		{ 0x6F3F88, 7, "NiGeometry::pushStateToRenderer", "the old current effect state (release)" },
		{ 0x6F3F97, 10, "NiGeometry::pushStateToRenderer", "the shape's effect state" },
		{ 0x6F5393, 7, "NiRenderer::BeginBatch", "the old current property state (release)" },
		{ 0x6F53A2, 10, "NiRenderer::BeginBatch", "the batch's property state" },
		{ 0x6F53BB, 7, "NiRenderer::BeginBatch", "the old current effect state (release)" },
		{ 0x6F53CA, 10, "NiRenderer::BeginBatch", "the batch's effect state" },
		{ 0x6A9EEB, 7, "NiDX8Renderer::dtor", "the current property state (release)" },
		{ 0x6EAC1A, 7, "NiAVObject::dtor", "the first property of the list (release)" },
	};

	// The modrm bytes of the [r+4] operand of a site's increment.
	static constexpr BYTE INCREMENT_MODRMS[] = { 0x40, 0x41, 0x45, 0x46, 0x47 };

	// The modrm bytes of [r+4] with reg eax in a plain release: r is ecx, ebp or esi.
	static constexpr BYTE PLAIN_RELEASE_MODRMS[] = { 0x41, 0x45, 0x46 };

	// The registers that hold the count in an interleaved release: edx, esi and edi.
	static constexpr BYTE INTERLEAVED_REGISTERS[] = { 2, 6, 7 };

	// The registers that hold the state in a store before an increment: esi and edi.
	static constexpr BYTE STORE_REGISTERS[] = { 6, 7 };

	static constexpr BYTE EDX = 2;

	template <size_t N>
	static bool contains(const BYTE(&values)[N], BYTE value) {
		return std::find(std::begin(values), std::end(values), value) != std::end(values);
	}

	void RefCountLocks::Stub::append(std::initializer_list<BYTE> bytes) {
		for (const auto byte : bytes) {
			if (size < MAX_SIZE) {
				code[size] = byte;
			}
			++size;
		}
	}

	void RefCountLocks::Stub::appendRelative(DWORD stubAddress, DWORD target) {
		const auto next = stubAddress + size + 4;
		const auto offset = target - next;
		append({ BYTE(offset), BYTE(offset >> 8), BYTE(offset >> 16), BYTE(offset >> 24) });
	}

	RefCountLocks::Stub RefCountLocks::buildStub(DWORD siteAddress, const BYTE* siteBytes, DWORD stubAddress) {
		Stub stub;
		const auto matched = buildIncrementOrDecrement(siteAddress, siteBytes, stubAddress, stub)
			|| buildPlainRelease(siteBytes, stub)
			|| buildStoreThenIncrement(siteBytes, stub)
			|| buildInterleavedRelease(siteBytes, stub);
		if (!matched) {
			return {};
		}

		// jmp back to the end of the site.
		stub.append({ 0xE9 });
		stub.appendRelative(stubAddress, siteAddress + stub.siteLength);
		if (stub.size > Stub::MAX_SIZE) {
			return {};
		}
		return stub;
	}

	// jz +3; inc dword [r+4] (5 bytes), or dec dword [ecx+4]; jnz +n (5 bytes).
	bool RefCountLocks::buildIncrementOrDecrement(DWORD siteAddress, const BYTE* siteBytes, DWORD stubAddress, Stub& stub) {
		if (siteBytes[0] == 0x74 && siteBytes[1] == 0x03 && siteBytes[2] == 0xFF && contains(INCREMENT_MODRMS, siteBytes[3]) && siteBytes[4] == 0x04) {
			// jz over the lock inc; lock inc dword [r+4]
			stub.append({ 0x74, 0x04, 0xF0, 0xFF, siteBytes[3], 0x04 });
			stub.siteLength = 5;
			return true;
		}

		if (siteBytes[0] == 0xFF && siteBytes[1] == 0x49 && siteBytes[2] == 0x04 && siteBytes[3] == 0x75) {
			const auto taken = siteAddress + 5 + DWORD(int(static_cast<signed char>(siteBytes[4])));
			// lock dec dword [ecx+4]; jnz taken
			stub.append({ 0xF0, 0xFF, 0x49, 0x04, 0x0F, 0x85 });
			stub.appendRelative(stubAddress, taken);
			stub.siteLength = 5;
			return true;
		}

		return false;
	}

	// mov eax, [r+4]; dec eax; mov [r+4], eax: 7 bytes before a jnz.
	bool RefCountLocks::buildPlainRelease(const BYTE* siteBytes, Stub& stub) {
		const auto modrm = siteBytes[1];
		const BYTE form[] = { 0x8B, modrm, 0x04, 0x48, 0x89, modrm, 0x04, 0x75 };
		if (!contains(PLAIN_RELEASE_MODRMS, modrm) || !std::equal(std::begin(form), std::end(form), siteBytes)) {
			return false;
		}

		// mov eax, -1; lock xadd [r+4], eax; dec eax
		stub.append({ 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0x0F, 0xC1, modrm, 0x04, 0x48 });
		stub.siteLength = 7;
		return true;
	}

	// test r, r; mov [b+d8], r; jz +3; inc dword [r+4]: 10 bytes, r is esi or edi.
	bool RefCountLocks::buildStoreThenIncrement(const BYTE* siteBytes, Stub& stub) {
		const BYTE r = siteBytes[1] & 7;
		const auto store = siteBytes[3];
		const BYTE increment[] = { 0x74, 0x03, 0xFF, BYTE(0x40 | r), 0x04 };
		const auto matches = siteBytes[0] == 0x85
			&& contains(STORE_REGISTERS, r)
			&& siteBytes[1] == BYTE(0xC0 | (r << 3) | r)
			&& siteBytes[2] == 0x89
			&& store >> 6 == 1
			&& ((store >> 3) & 7) == r
			&& (store & 7) != 4
			&& std::equal(std::begin(increment), std::end(increment), siteBytes + 5);
		if (!matches) {
			return false;
		}

		// test r, r; mov [b+d8], r; jz over the lock inc; lock inc dword [r+4]
		stub.append({ siteBytes[0], siteBytes[1], siteBytes[2], siteBytes[3], siteBytes[4] });
		stub.append({ 0x74, 0x04, 0xF0, 0xFF, BYTE(0x40 | r), 0x04 });
		stub.siteLength = 10;
		return true;
	}

	// mov c, [eax+4]; mov ecx, eax; add eax, 4; dec c; (mov edx, c); test edx, edx; mov [eax], c: 15 bytes before a
	// jnz, or 13 when c is edx and the mov edx, c is absent.
	bool RefCountLocks::buildInterleavedRelease(const BYTE* siteBytes, Stub& stub) {
		const BYTE c = (siteBytes[1] >> 3) & 7;
		if (!contains(INTERLEAVED_REGISTERS, c)) {
			return false;
		}

		BYTE form[16] = {};
		size_t formLength = 0;
		const auto add = [&form, &formLength](std::initializer_list<BYTE> bytes) {
			for (const auto byte : bytes) {
				form[formLength++] = byte;
			}
		};
		add({ 0x8B, BYTE(0x40 | (c << 3)), 0x04, 0x8B, 0xC8, 0x83, 0xC0, 0x04, BYTE(0x48 + c) });
		if (c != EDX) {
			add({ 0x8B, BYTE(0xD0 + c) });
		}
		add({ 0x85, 0xD2, 0x89, BYTE(c << 3), 0x75 });
		if (!std::equal(form, form + formLength, siteBytes)) {
			return false;
		}

		// mov ecx, eax; mov c, -1; lock xadd [eax+4], c; dec c; add eax, 4; (mov edx, c); test edx, edx
		stub.append({ 0x8B, 0xC8, BYTE(0xB8 + c), 0xFF, 0xFF, 0xFF, 0xFF });
		stub.append({ 0xF0, 0x0F, 0xC1, BYTE(0x40 | (c << 3)), 0x04, BYTE(0x48 + c), 0x83, 0xC0, 0x04 });
		if (c != EDX) {
			stub.append({ 0x8B, BYTE(0xD0 + c) });
		}
		stub.append({ 0x85, 0xD2 });
		stub.siteLength = static_cast<unsigned int>(formLength - 1);
		return true;
	}

	void RefCountLocks::logRefusal(const Site& site, const char* reason) {
		auto& log = mwse::log::getLog();
		log << "reference counts: 0 of " << SITE_COUNT << " sites locked: the bytes at 0x" << std::hex << site.address
			<< " (" << site.function << ", " << site.slot << ") are";
		const auto bytes = reinterpret_cast<const BYTE*>(site.address);
		for (auto i = 0u; i < site.length; ++i) {
			log << " " << std::setw(2) << std::setfill('0') << int(bytes[i]);
		}
		log << std::dec << std::setfill(' ') << ", " << reason << "; all sites are refused." << std::endl;
	}

	size_t RefCountLocks::install() {
		// Check every site before the first write.
		for (const auto& site : sites) {
			const auto bytes = reinterpret_cast<const BYTE*>(site.address);
			if (buildStub(site.address, bytes, 0).siteLength != site.length) {
				logRefusal(site, "not the game's");
				return 0;
			}
		}

		// One block for all stubs, never freed.
		const auto block = static_cast<BYTE*>(VirtualAlloc(nullptr, SITE_COUNT * Stub::MAX_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (block == nullptr) {
			mwse::log::getLog() << "reference counts: 0 of " << SITE_COUNT << " sites locked: no memory for the stubs (error " << GetLastError() << ")." << std::endl;
			return 0;
		}
		std::fill_n(block, SITE_COUNT * Stub::MAX_SIZE, BYTE(0xCC));

		auto& log = mwse::log::getLog();
		log << "reference counts: " << SITE_COUNT << " of " << SITE_COUNT << " sites locked:" << std::hex;
		for (auto i = 0u; i < SITE_COUNT; ++i) {
			const auto& site = sites[i];
			const auto stubAddress = reinterpret_cast<DWORD>(block + i * Stub::MAX_SIZE);
			const auto stub = buildStub(site.address, reinterpret_cast<const BYTE*>(site.address), stubAddress);
			std::copy_n(stub.code, stub.size, block + i * Stub::MAX_SIZE);
			se::memory::genJumpUnprotected(site.address, stubAddress, site.length);
			log << " 0x" << site.address;
		}
		log << std::dec << std::endl;

		DWORD oldProtect = 0;
		VirtualProtect(block, SITE_COUNT * Stub::MAX_SIZE, PAGE_EXECUTE_READ, &oldProtect);
		return SITE_COUNT;
	}
}
