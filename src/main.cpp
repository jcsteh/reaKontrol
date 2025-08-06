/*
 * ReaKontrol
 * Main plug-in code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2018-2019 James Teh
 * License: GNU General Public License version 2.0
 */

#include <cstring>
#include <regex>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <initguid.h>
#include <SetupAPI.h>
#include <Usbiodef.h>
#else
#include "swell.h"
#endif
#define REAPERAPI_IMPLEMENT
#include "reaKontrol.h"

using namespace std;

const char* KK_DEVICE_NAME_SUFFIXES[] = {
	"Komplete Kontrol DAW - 1", // Mk1 and Mk2
	"Komplete Kontrol A DAW",
	"Komplete Kontrol M DAW",
#ifdef _WIN32
	"2 (KONTROL S49 MK3)", // e.g. MIDIIN2(KONTROL S49 MK3)
	"2 (KONTROL S61 MK3)",
	"2 (KONTROL S88 MK3)",
#else
	"MK3 - DAW", // e.g. Native Instruments - KONTROL S61 MK3 - DAW
#endif
};
const char KKMK1_HWID_PREFIX[] = "USB\\VID_17CC&PID_";
const size_t USB_PID_LEN = 4;
const char* KKMK1_USB_PIDS[] = {"1340", "1350", "1360", "1410"};

int getKkMidiDevice(auto countFunc, auto getFunc) {
	int count = countFunc();
	for (int dev = 0; dev < count; ++dev) {
		char rawName[100];
		const bool present = getFunc(dev, rawName, sizeof(rawName));
		if (!present) {
			continue;
		}
		const string_view name(rawName);
		for (const char* suffix: KK_DEVICE_NAME_SUFFIXES) {
			if (name.ends_with(suffix)) {
				return dev;
			}
		}
	}
	return -1;
}

bool isMk1Connected() {
#ifdef _WIN32
	HDEVINFO infoSet = SetupDiGetClassDevsA(&GUID_DEVINTERFACE_USB_DEVICE,
		nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
	for (DWORD i = 0; i < 256; ++i) {
		SP_DEVICE_INTERFACE_DATA intData;
		intData.cbSize = sizeof(intData);
		if (!SetupDiEnumDeviceInterfaces(infoSet, nullptr,
				&GUID_DEVINTERFACE_USB_DEVICE, i, &intData)) {
			break;
		}
		SP_DEVINFO_DATA devInfo;
		devInfo.cbSize = sizeof(devInfo);
		if (!SetupDiGetDeviceInterfaceDetailA(infoSet, &intData, nullptr, 0,
				nullptr, &devInfo)) {
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
				break;
			}
		}
		char hwId[200];
		if (!SetupDiGetDeviceRegistryPropertyA(infoSet, &devInfo,
				SPDRP_HARDWAREID, nullptr, (PBYTE)&hwId, sizeof(hwId), nullptr)) {
			break;
		}
		if (strncmp(hwId, KKMK1_HWID_PREFIX, sizeof(KKMK1_HWID_PREFIX) - 1) == 0) {
			char* devPid = hwId + sizeof(KKMK1_HWID_PREFIX) - 1;
			for (int pidIndex = 0; pidIndex < ARRAYSIZE(KKMK1_USB_PIDS); ++pidIndex) {
				if (strncmp(devPid, KKMK1_USB_PIDS[pidIndex], USB_PID_LEN) == 0) {
					SetupDiDestroyDeviceInfoList(infoSet);
					return true;
				}
			}
		}
	}
	SetupDiDestroyDeviceInfoList(infoSet);
#endif
	return false;
}

const string getKkInstanceName(MediaTrack* track, bool stripPrefix) {
	int fxCount = TrackFX_GetCount(track);
	for (int fx = 0; fx < fxCount; ++fx) {
		// Check for a matching instance name. For Komplete Kontrol, this will be the
		// first parameter. For Kontakt, Maschine, etc., it is the last real
		// parameter. However, REAPER appends a heap of MIDI control parameters, as
		// well as the bypass, delta and wet parameters. Therefore, use a set of
		// known parameter indices which will likely need to be updated in future.
		for (int param : {0, 128, 2048, 4096}) {
			// The focus follow ID always begins with the prefix NIXX (where XX are any
			// alphabetical letters), and follows with a multiple digit instance number.
			char paramName[15];
			TrackFX_GetParamName(track, fx, param, paramName, sizeof(paramName));
			static const regex RE_NAME("NI[a-zA-Z]{2,}(\\d{2,})");
			cmatch m;
			regex_match(paramName, m, RE_NAME);
			if (m.empty()) {
				continue;
			}
			if (stripPrefix) {
				return m.str(1);
			}
			return paramName;
		}
	}
	return "";
}

BaseSurface::BaseSurface(int inDev, int outDev) {
	this->_midiIn = CreateMIDIInput(inDev);
	this->_midiOut = CreateMIDIOutput(outDev, false, nullptr);
	if (this->_midiOut) {
		this->_midiIn->start();
	}
}

BaseSurface::~BaseSurface() {
	if (this->_midiIn)  {
		this->_midiIn->stop();
		delete this->_midiIn;
	}
	if (this->_midiOut) {
		delete this->_midiOut;
	}
}

void BaseSurface::Run() {
	if (!this->_midiIn) {
		return;
	}
	this->_midiIn->SwapBufs(timeGetTime());
	MIDI_eventlist* list = this->_midiIn->GetReadBuf();
	MIDI_event_t* evt;
	int i = 0;
	while ((evt = list->EnumItems(&i))) {
		this->_onMidiEvent(evt);
	}
}

IReaperControlSurface* surface = nullptr;

void connect() {
	int inDev = getKkMidiDevice(GetNumMIDIInputs, GetMIDIInputName);
	if (inDev == -1) {
		return;
	}
	int outDev = getKkMidiDevice(GetNumMIDIOutputs, GetMIDIOutputName);
	if (outDev == -1) {
		return;
	}
	if (isMk1Connected()) {
		surface = createMcuSurface(inDev, outDev);
	} else {
		surface = createNiMidiSurface(inDev, outDev);
	}
	plugin_register("csurf_inst", (void*)surface);
}

void disconnect() {
	if (surface) {
		plugin_register("-csurf_inst", (void*)surface);
		delete surface;
		surface = nullptr;
	}
}

int CMD_RECONNECT = 0;

bool handleCommand(KbdSectionInfo* section, int command, int val, int valHw,
	int relMode, HWND hwnd
) {
	if (command == CMD_RECONNECT) {
		disconnect();
		connect();
		return true;
	}
	return false;
}

extern "C" {

REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec) {
	if (rec) {
		// Load.
		if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc || REAPERAPI_LoadAPI(rec->GetFunc) != 0) {
			return 0; // Incompatible.
		}
		connect();
		const int MAIN_SECTION = 0;
		custom_action_register_t action = {MAIN_SECTION, "REAKONTROL_RECONNECT",
			"ReaKontrol: Reconnect"};
		CMD_RECONNECT = rec->Register("custom_action", &action);
		rec->Register("hookcommand2", (void*)handleCommand);
		return 1;
	} else {
		// Unload.
		disconnect();
		return 0;
	}
}

}
