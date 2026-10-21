#pragma once

#include "MWSENativeApi.h"

namespace mwse::native {
	// The calls of an engine function at its call sites. MWSE redirects all of them or none.
	class CallSites {
	public:
		CallSites(std::initializer_list<DWORD> sites, DWORD engineFunction);

		// True when each site still calls the engine function.
		bool areUnchanged() const;
		// Redirects each site to `to`. Call only after areUnchanged.
		void redirect(DWORD to) const;
		// The first site that does not call the engine function, or 0.
		DWORD findChangedSite() const;

		DWORD getEngineFunction() const;

	private:
		std::vector<DWORD> m_sites;
		DWORD m_engineFunction;
	};

	// An engine point with native listeners. The list of listeners is replaced as a whole at a
	// change, so a fire on any thread takes no lock.
	class EventPoint {
	public:
		using Installer = MwseNativeStatus(*)();

		EventPoint(uint32_t id, const char* name, Installer installer);

		MwseNativeStatus subscribe(const char* module, MwseNativeListener listener, void* user, int32_t priority, MwseNativeHandle handle);
		bool unsubscribe(MwseNativeHandle handle);
		void fire(const void* packet) const;

		uint32_t getID() const;
		const char* getName() const;

	private:
		struct Listener {
			MwseNativeListener function;
			void* user;
			int32_t priority;
			MwseNativeHandle handle;
			std::string module;
		};
		using ListenerList = std::vector<Listener>;

		void publish(std::unique_ptr<ListenerList> list);

		uint32_t m_id;
		const char* m_name;
		Installer m_installer;
		bool m_installed = false;
		std::atomic<const ListenerList*> m_listeners = nullptr;
		// The lists that a fire on another thread can still read. They are kept until exit.
		std::vector<std::unique_ptr<ListenerList>> m_lists;
	};

	// An engine function that one module can replace.
	class OverrideSlot {
	public:
		using Installer = MwseNativeStatus(*)();

		OverrideSlot(uint32_t id, const char* name, void* engineFunction, Installer installer);

		MwseNativeStatus claim(const char* module, void* function, void** engineFunction, MwseNativeHandle handle);
		bool release(MwseNativeHandle handle);

		// The owner's function, or the engine function when no module owns the slot.
		template <typename T>
		T getFunction() const {
			const auto owner = m_owner.load(std::memory_order_acquire);
			return reinterpret_cast<T>(owner ? owner : m_engineFunction);
		}

		uint32_t getID() const;

	private:
		uint32_t m_id;
		const char* m_name;
		void* m_engineFunction;
		Installer m_installer;
		bool m_installed = false;
		std::atomic<void*> m_owner = nullptr;
		MwseNativeHandle m_handle = 0;
		std::string m_module;
	};

	// The scale of the UI that a module requested.
	class UIScale {
	public:
		MwseNativeStatus request(const char* module, float scale);
		void query(MwseNativeUIScaleV1* packet) const;

		bool isRequested() const;
		float getScale() const;

	private:
		MwseNativeStatus installPickRay();

		float m_scale = 1.0f;
		bool m_requested = false;
		bool m_pickRayInViewSize = false;
		bool m_configureInstalled = false;
	};

	class Registry {
	public:
		static Registry& get();

		// The table of the export, or null for a version newer than this MWSE has.
		static const MwseNativeApiV1* getTable(uint32_t version);

		void setMainThread(DWORD threadID);
		bool isMainThread() const;

		EventPoint cameraTransformsUpdated;
		EventPoint uiConfigured;
		OverrideSlot dynamicLightTest;
		UIScale uiScale;

	private:
		Registry();

		static MwseNativeStatus __cdecl subscribe(const char* module, uint32_t eventId, MwseNativeListener listener, void* user, int32_t priority, MwseNativeHandle* handle);
		static void __cdecl unsubscribe(MwseNativeHandle handle);
		static MwseNativeStatus __cdecl claim(const char* module, uint32_t overrideId, void* function, void** engineFunction, MwseNativeHandle* handle);
		static void __cdecl release(MwseNativeHandle handle);
		static MwseNativeStatus __cdecl request(const char* module, uint32_t requestId, const void* packet);
		static MwseNativeStatus __cdecl query(uint32_t queryId, void* packet);

		EventPoint* findEvent(uint32_t id);
		OverrideSlot* findOverride(uint32_t id);
		MwseNativeHandle nextHandle();

		DWORD m_mainThread = 0;
		MwseNativeHandle m_lastHandle = 0;
		std::unordered_map<MwseNativeHandle, EventPoint*> m_eventHandles;
		std::unordered_map<MwseNativeHandle, OverrideSlot*> m_overrideHandles;
	};
}
