#include "TES3Game.h"

#include "LuaActivationTargetChangedEvent.h"

#include "LuaManager.h"

#include "Log.h"
#include "WindowsUtil.h"

namespace TES3 {
	void Game::setGamma(float value) {
		vTable->SetGamma(this, value);
	}

	const auto TES3_Game_readSettings = reinterpret_cast<bool(__thiscall*)(Game*)>(0x4F54F0);
	bool Game::readSettings() {
		const auto result = TES3_Game_readSettings(this);
		applyApplicationIniOverrides();
		mwse::log::getLog() << "Application settings: " << windowWidth << "x" << windowHeight << ", refresh rate " << refreshRate << ", adapter " << adapter << ", fullscreen " << fullscreen << "; Morrowind.ini [Application] overrides " << registryValuesOfIniOverrides.size() << " settings of the registry." << std::endl;
		return result;
	}

	const auto TES3_Game_writeSettings = reinterpret_cast<void(__thiscall*)(Game*)>(0x4F5840);
	void Game::writeSettings() {
		// The registry keeps its own values of the settings that Morrowind.ini overrides.
		swapRegistryValues();
		TES3_Game_writeSettings(this);
		swapRegistryValues();
	}

	const auto TES3_Game_initialize = reinterpret_cast<bool(__thiscall *)(Game*)>(0x417880);
	bool Game::initialize() {
		return TES3_Game_initialize(this);
	}

	const auto TES3_Game_clearTarget = reinterpret_cast<bool(__thiscall *)(Game*)>(0x41CD00);
	void Game::clearTarget() {
		TES3_Game_clearTarget(this);
	}

	const auto TES3_Game_savePlayerOptions = reinterpret_cast<void(__thiscall*)(Game*)>(0x4293A0);
	void Game::savePlayerOptions() {
		TES3_Game_savePlayerOptions(this);
	}

	const auto TES3_Game_saveKeyframeExtractTime = reinterpret_cast<void(__cdecl*)()>(0x47CDC0);
	void Game::saveKeyframeExtractTime() {
		TES3_Game_saveKeyframeExtractTime();
	}

	const auto TES3_Game_deleteEmptyWarningsFile = reinterpret_cast<void(__cdecl*)()>(0x476DD0);
	void Game::deleteEmptyWarningsFile() {
		TES3_Game_deleteEmptyWarningsFile();
	}

	const auto TES3_Game_renderNextFrame = reinterpret_cast<void(__thiscall*)(Game*, int)>(0x41BE90);
	void Game::renderNextFrame(int renderType) {
		TES3_Game_renderNextFrame(this, renderType);
	}

	void Game::applyApplicationIniOverrides() {
		constexpr auto morrowindIniFile = ".\\Morrowind.ini";

		registryValuesOfIniOverrides.clear();

		applyApplicationIniOverride(windowWidth, se::windows::ini::GetInt(morrowindIniFile, "Application", "Screen Width"));
		applyApplicationIniOverride(windowHeight, se::windows::ini::GetInt(morrowindIniFile, "Application", "Screen Height"));
		applyApplicationIniOverride(screenDepth, se::windows::ini::GetInt(morrowindIniFile, "Application", "Screen Depth"));
		applyApplicationIniOverride(backBuffers, se::windows::ini::GetInt(morrowindIniFile, "Application", "Backbuffers"));
		applyApplicationIniOverride(multiSamples, se::windows::ini::GetInt(morrowindIniFile, "Application", "Multisamples"));
		applyApplicationIniOverride(mipmapSkipLevel, se::windows::ini::GetInt(morrowindIniFile, "Application", "Mipmap Skip Level"));
		applyApplicationIniOverride(vertexProcessing, se::windows::ini::GetInt(morrowindIniFile, "Application", "Vertex Processing"));
		applyApplicationIniOverride(swapEffect, se::windows::ini::GetInt(morrowindIniFile, "Application", "Swap Effect"));
		applyApplicationIniOverride(refreshRate, se::windows::ini::GetInt(morrowindIniFile, "Application", "Refresh Rate"));
		applyApplicationIniOverride(presentationInterval, se::windows::ini::GetInt(morrowindIniFile, "Application", "Presentation Interval"));
		applyApplicationIniOverride(adapter, se::windows::ini::GetInt(morrowindIniFile, "Application", "Adapter"));
		applyApplicationIniOverride(gamma, se::windows::ini::GetFloat(morrowindIniFile, "Application", "Gamma"));
		applyApplicationIniOverride(fullscreen, se::windows::ini::GetBool(morrowindIniFile, "Application", "Fullscreen"));
		applyApplicationIniOverride(pixelShader, se::windows::ini::GetBool(morrowindIniFile, "Application", "Pixelshader"));
		applyApplicationIniOverride(stencil, se::windows::ini::GetBool(morrowindIniFile, "Application", "Stencil"));
		applyApplicationIniOverride(mipmap, se::windows::ini::GetBool(morrowindIniFile, "Application", "Mipmap"));
		applyApplicationIniOverride(hardware, se::windows::ini::GetBool(morrowindIniFile, "Application", "Hardware"));
		applyApplicationIniOverride(multiPass, se::windows::ini::GetBool(morrowindIniFile, "Application", "Multipass"));

		gamma = std::clamp(gamma, 0.0f, 2.0f);
	}

	std::vector<Game::RegistryValue> Game::registryValuesOfIniOverrides;

	void Game::keepRegistryValue(void* setting, size_t size) {
		RegistryValue kept = {};
		kept.offset = reinterpret_cast<BYTE*>(setting) - reinterpret_cast<BYTE*>(this);
		kept.size = size;
		std::memcpy(&kept.value, setting, size);
		registryValuesOfIniOverrides.push_back(kept);
	}

	void Game::swapRegistryValues() {
		for (auto& kept : registryValuesOfIniOverrides) {
			const auto setting = reinterpret_cast<BYTE*>(this) + kept.offset;
			DWORD current = 0;
			std::memcpy(&current, setting, kept.size);
			std::memcpy(setting, &kept.value, kept.size);
			kept.value = current;
		}
	}

	void Game::setPlayerTarget(Reference* reference) {
		if (reference != playerTarget) {
			const auto previous = playerTarget;
			playerTarget = reference;
			if (mwse::lua::event::ActivationTargetChangedEvent::getEventEnabled()) {
				const auto stateHandle = mwse::lua::LuaManager::getInstance().getThreadSafeStateHandle();
				stateHandle.triggerEvent(new mwse::lua::event::ActivationTargetChangedEvent(reference));
			}
		}
	}

	void Game::restoreGamma() {
		setGamma(1.0f);
	}

	void Game::endProcessAtQuit() {
		savePlayerOptions();
		saveKeyframeExtractTime();
		deleteEmptyWarningsFile();
		restoreGamma();

		// Flush every C runtime's open files.
		mwse::log::getLog() << "quit: process ended before teardown" << std::endl;
		_flushall();
		for (const auto runtime : { L"msvcrt.dll", L"ucrtbase.dll" }) {
			const auto module = GetModuleHandleW(runtime);
			const auto flushAll = module ? reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module, "_flushall")) : nullptr;
			if (flushAll) {
				flushAll();
			}
		}

		TerminateProcess(GetCurrentProcess(), 0);
		mwse::log::getLog() << "quit: TerminateProcess failed (" << GetLastError() << "); the normal exit follows." << std::endl;
	}

	Game* Game::get() {
		return *reinterpret_cast<TES3::Game**>(0x7C6CDC);
	}
}
