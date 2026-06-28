// AudioController::loadSoundFile and createSoundBufferFromPcm, isolated here so
// the dr_libs decoder implementations don't bloat the core audio files.

#include "TES3AudioController.h"
#include "TES3Sound.h"

#include "Log.h"
#include "MWSEConfig.h"

#pragma comment(lib, "dxguid.lib") // IID_IDirectSound3DBuffer

// Vendored single-header decoders (deps/dr_libs submodule); warnings suppressed.
#pragma warning(push, 0)
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#pragma warning(pop)

namespace TES3 {

	// The raw engine loader. loadSoundFile falls back to this for files it doesn't
	// transcode, calling it directly so it never recurses into itself.
	static const auto TES3_AudioController_loadSoundFile = reinterpret_cast<SoundBuffer*(__thiscall*)(AudioController*, const char*, bool)>(0x401DB0);

	// Captured at DLL load (main thread).
	static const DWORD g_mainThreadId = GetCurrentThreadId();
	static bool onMainThread() { return GetCurrentThreadId() == g_mainThreadId; }

	// Perf logging is opt-in (MWSE MCM: "Log flexible audio loads") and main-thread
	// only -- the decode worker must not touch the non-thread-safe MWSE log, and its
	// loads no longer stall the frame so their timing is uninteresting.
	static bool shouldLogLoad() { return mwse::Configuration::LogFlexibleAudioLoads && onMainThread(); }

	static long long elapsedUs(std::chrono::steady_clock::time_point start) {
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	}

	static SoundBuffer* logDecodeFailed(const char* kind, const char* filename) {
		if (shouldLogLoad()) mwse::log::getLog() << "[MWSE] flexible audio: " << kind << ' ' << filename << " decode-failed\n";
		return nullptr;
	}

	// Interleaved 16-bit PCM, the only format DirectSound (and createSoundBufferFromPcm) accepts.
	struct DecodedPcm {
		std::vector<drwav_int16> samples; // interleaved
		unsigned int channels = 0;
		unsigned int sampleRate = 0;
		bool ok() const { return !samples.empty() && channels > 0 && sampleRate > 0; }
	};

	static bool endsWithCI(const char* s, const char* suffix) {
		if (!s) return false;
		const size_t ls = std::strlen(s), lf = std::strlen(suffix);
		if (lf > ls) return false;
		return _strnicmp(s + (ls - lf), suffix, lf) == 0;
	}

	// Matches the engine's own voiceover probe at 0x48C5F3.
	static bool isVoiceoverPath(const char* filename) {
		return std::strstr(filename, "vo\\") != nullptr
			|| std::strstr(filename, "Vo\\") != nullptr
			|| std::strstr(filename, "vO\\") != nullptr
			|| std::strstr(filename, "VO\\") != nullptr;
	}

	static void downmixToMono(std::vector<drwav_int16>& samples, unsigned int channels) {
		if (channels <= 1) return;
		const size_t frames = samples.size() / channels;
		for (size_t i = 0; i < frames; ++i) {
			int acc = 0;
			const drwav_int16* frame = &samples[i * channels];
			for (unsigned int c = 0; c < channels; ++c) acc += frame[c];
			samples[i] = static_cast<drwav_int16>(acc / static_cast<int>(channels));
		}
		samples.resize(frames);
	}

	static bool decodeMp3(const char* filename, DecodedPcm& out) {
		drmp3_config config = {};
		drmp3_uint64 frames = 0;
		drmp3_int16* data = drmp3_open_file_and_read_pcm_frames_s16(filename, &config, &frames, nullptr);
		if (!data) return false;
		out.channels = config.channels;
		out.sampleRate = config.sampleRate;
		out.samples.assign(data, data + frames * config.channels);
		drmp3_free(data, nullptr);
		return out.ok();
	}

	static bool decodeFlac(const char* filename, DecodedPcm& out) {
		unsigned int channels = 0, sampleRate = 0;
		drflac_uint64 frames = 0;
		drflac_int16* data = drflac_open_file_and_read_pcm_frames_s16(filename, &channels, &sampleRate, &frames, nullptr);
		if (!data) return false;
		out.channels = channels;
		out.sampleRate = sampleRate;
		out.samples.assign(data, data + frames * channels);
		drflac_free(data, nullptr);
		return out.ok();
	}

	// Mirrors LoadSoundFile's WAV path (flags/3D/volume) but owns its memory, so the
	// engine's CreateSoundBuffer-failure handle leak can't occur. On any failure the
	// partially built SoundBuffer (null COM/rawAudio fields) is freed via its no-op dtor.
	SoundBuffer* AudioController::createSoundBufferFromPcm(const short* samples, size_t sampleCount, unsigned int channels, unsigned int sampleRate, bool isPointSource) {
		if (!directSound || sampleCount == 0) return nullptr;
		const DWORD byteCount = static_cast<DWORD>(sampleCount * sizeof(short));

		auto* soundBuffer = new SoundBuffer(); // engine heap, zero-initialized

		auto* format = reinterpret_cast<WAVEFORMATEX*>(soundBuffer->fileHeader);
		format->wFormatTag = WAVE_FORMAT_PCM;
		format->nChannels = static_cast<WORD>(channels);
		format->nSamplesPerSec = sampleRate;
		format->wBitsPerSample = 16;
		format->nBlockAlign = static_cast<WORD>(channels * sizeof(short));
		format->nAvgBytesPerSec = sampleRate * format->nBlockAlign;
		format->cbSize = 0;

		// Flags match the engine (PatchUtil's DS_FLAGS_*).
		constexpr DWORD DS_FLAGS_DEFAULT = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY;
		constexpr DWORD DS_FLAGS_3D = DS_FLAGS_DEFAULT | DSBCAPS_CTRL3D | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_MUTE3DATMAXDISTANCE;
		const bool use3D = isPointSource && soundQuality3D > 1;
		DWORD flags;
		if (!isPointSource) {
			flags = DS_FLAGS_DEFAULT;
		}
		else if (use3D) {
			flags = DS_FLAGS_3D;
		}
		else {
			flags = DS_FLAGS_DEFAULT | DSBCAPS_CTRLPAN;
		}
		if (mwse::Configuration::UseGlobalAudio) flags |= DSBCAPS_GLOBALFOCUS;
		if (getHasStreamingBuffers()) flags |= DSBCAPS_LOCDEFER;

		auto& description = soundBuffer->bufferDescription;
		description.dwSize = sizeof(DSBUFFERDESC);
		description.dwFlags = flags;
		description.dwBufferBytes = byteCount;
		description.lpwfxFormat = format;

		soundBuffer->isVoiceover = false;
		soundBuffer->rawAudio = nullptr;

		IDirectSoundBuffer* directSoundBuffer = nullptr;
		if (FAILED(directSound->CreateSoundBuffer(&description, &directSoundBuffer, nullptr)) || !directSoundBuffer) {
			delete soundBuffer;
			return nullptr;
		}

		void* block1 = nullptr;
		void* block2 = nullptr;
		DWORD length1 = 0;
		DWORD length2 = 0;
		if (SUCCEEDED(directSoundBuffer->Lock(0, 0, &block1, &length1, &block2, &length2, DSBLOCK_ENTIREBUFFER))) {
			if (block1 && length1) std::memcpy(block1, samples, length1);
			if (block2 && length2) std::memcpy(block2, reinterpret_cast<const char*>(samples) + length1, length2);
			directSoundBuffer->Unlock(block1, length1, block2, length2);
		}

		soundBuffer->lpSoundBuffer = directSoundBuffer;
		if (use3D) {
			IDirectSound3DBuffer* buffer3D = nullptr;
			if (SUCCEEDED(directSoundBuffer->QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<void**>(&buffer3D)))) {
				soundBuffer->lpSound3DBuffer = buffer3D;
			}
		}
		directSoundBuffer->SetVolume(0);
		return soundBuffer;
	}

	// Decodes WAV variants DirectSound can't take (24/32-bit, float, EXTENSIBLE)
	// and non-voiceover MP3/FLAC (no ACM codec linked), and downmixes stereo 3D
	// point sources to mono. Engine-compatible, missing, and voiceover files fall
	// through to the vanilla loader; a recognized-but-unplayable format returns
	// null (silent) instead of the engine's leaking failure path.
	SoundBuffer* AudioController::loadSoundFile(const char* filename, bool isPointSource) {
		if (!filename || isVoiceoverPath(filename) || disableAudio || !isDirectSoundAvailable()) {
			return TES3_AudioController_loadSoundFile(this, filename, isPointSource);
		}

		// Decode the formats the engine can't into PCM16. Engine-compatible WAVs are
		// passed straight through so the common case keeps vanilla behavior.
		DecodedPcm pcm;
		const char* kind;
		const auto decodeStart = std::chrono::steady_clock::now();
		if (endsWithCI(filename, ".mp3")) {
			kind = "mp3";
			if (!decodeMp3(filename, pcm)) return logDecodeFailed(kind, filename);
		}
		else if (endsWithCI(filename, ".flac")) {
			kind = "flac";
			if (!decodeFlac(filename, pcm)) return logDecodeFailed(kind, filename);
		}
		else {
			kind = "wav";
			drwav wav;
			if (!drwav_init_file(&wav, filename, nullptr)) {
				return TES3_AudioController_loadSoundFile(this, filename, isPointSource); // missing/garbage -> engine logs
			}
			const bool needsDownmix = isPointSource && wav.channels > 1;
			if (wav.translatedFormatTag == DR_WAVE_FORMAT_PCM && wav.bitsPerSample == 16 && !needsDownmix) {
				drwav_uninit(&wav);
				return TES3_AudioController_loadSoundFile(this, filename, isPointSource);
			}
			pcm.channels = wav.channels;
			pcm.sampleRate = wav.sampleRate;
			pcm.samples.resize(static_cast<size_t>(wav.totalPCMFrameCount) * wav.channels);
			const drwav_uint64 framesRead = drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, pcm.samples.data());
			pcm.samples.resize(static_cast<size_t>(framesRead) * wav.channels);
			drwav_uninit(&wav);
		}
		const long long decodeUs = elapsedUs(decodeStart);

		if (!pcm.ok()) return nullptr;

		const unsigned int sourceChannels = pcm.channels;
		// 3D point sources must be mono (DirectSound rejects stereo CTRL3D).
		if ((isPointSource && pcm.channels > 1) || pcm.channels > 2) {
			downmixToMono(pcm.samples, pcm.channels);
			pcm.channels = 1;
		}

		const auto buildStart = std::chrono::steady_clock::now();
		SoundBuffer* soundBuffer = createSoundBufferFromPcm(pcm.samples.data(), pcm.samples.size(), pcm.channels, pcm.sampleRate, isPointSource);
		if (shouldLogLoad()) {
			mwse::log::getLog() << "[MWSE] flexible audio: " << kind << ' ' << filename
				<< " (" << sourceChannels << "ch " << pcm.sampleRate << "Hz) " << (soundBuffer ? "ok" : "build-failed")
				<< " decode=" << decodeUs << "us build=" << elapsedUs(buildStart) << "us\n";
		}
		return soundBuffer;
	}

}
