/*
 * ReaKontrol
 * Main plug-in code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2018-2019 James Teh
 * License: GNU General Public License version 2.0
 */

#include <windows.h>
#ifdef _WIN32
#include <SetupAPI.h>
#include <initguid.h>
#include <Usbiodef.h>
#endif
#include <string>
#include <cstring>
#define REAPERAPI_IMPLEMENT
#include "reaKontrol.h"

using namespace std;

const char KKS_DEVICE_NAME[] = "Komplete Kontrol DAW - 1";
const char KKA_DEVICE_NAME[] = "Komplete Kontrol A DAW";
const char KKM_DEVICE_NAME[] = "Komplete Kontrol M DAW";
const char KKMK1_HWID_PREFIX[] = "USB\\VID_17CC&PID_";
const size_t USB_PID_LEN = 4;
const char* KKMK1_USB_PIDS[] = {"1340", "1350", "1360", "1410"};
const char KK_FX_PREFIX[] = "VSTi: Komplete Kontrol";
const char KK_INSTANCE_PARAM_PREFIX[] = "NIKB";

int getKkMidiInput() {
	int count = GetNumMIDIInputs();
	for (int dev = 0; dev < count; ++dev) {
		char name[30];
		bool present = GetMIDIInputName(dev, name, sizeof(name));
		if (!present) {
			continue;
		}
		if (strcmp(name, KKS_DEVICE_NAME) == 0
				|| strcmp(name, KKA_DEVICE_NAME) == 0
				|| strcmp(name, KKM_DEVICE_NAME) == 0) {
			return dev;
		}
	}
	return -1;
}

int getKkMidiOutput() {
	int count = GetNumMIDIOutputs();
	for (int dev = 0; dev < count; ++dev) {
		char name[30];
		bool present = GetMIDIOutputName(dev, name, sizeof(name));
		if (!present) {
			continue;
		}
		if (strcmp(name, KKS_DEVICE_NAME) == 0
				|| strcmp(name, KKA_DEVICE_NAME) == 0
				|| strcmp(name, KKM_DEVICE_NAME) == 0) {
			return dev;
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
		// Find the Komplete Kontrol FX.
		char fxName[sizeof(KK_FX_PREFIX)];
		TrackFX_GetFXName(track, fx, fxName, sizeof(fxName));
		if (strcmp(fxName, KK_FX_PREFIX) != 0) {
			continue;
		}
		// Check for the instance name.
		// The first parameter should have a name in the form NIKBxx, where xx is a number.
		char paramName[7];
		TrackFX_GetParamName(track, fx, 0, paramName, sizeof(paramName));
		const size_t prefixLen = sizeof(KK_INSTANCE_PARAM_PREFIX) - 1;
		if (strncmp(paramName, KK_INSTANCE_PARAM_PREFIX, prefixLen) != 0) {
			return "";
		}
		if (stripPrefix) {
			return string(paramName).substr(prefixLen);
		}
		return paramName;
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
	// ToDo: Rather than putting this in BaseSurface consider to just override Run() in NiMidiSurface and call the superclass : BaseSurface::Run()
	this->_peakMixerUpdate();
	// --------------------------------------------------------------------------------
	if (!this->_midiIn) {
		return;
	}
	this->_midiIn->SwapBufs(timeGetTime());
	MIDI_eventlist* list = this->_midiIn->GetReadBuf();
	MIDI_event_t* evt;
	int i = 0;
	while (evt = list->EnumItems(&i)) {
		this->_onMidiEvent(evt);
	}
}

IReaperControlSurface* surface = nullptr;

extern "C" {

REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec) {
	if (rec) {
		// Load.
		if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc || REAPERAPI_LoadAPI(rec->GetFunc) != 0) {
			return 0; // Incompatible.
		}

		int inDev = getKkMidiInput();
		if (inDev != -1) {
			int outDev = getKkMidiOutput();
			if (outDev != -1) {
				if (isMk1Connected()) {
					surface = createMcuSurface(inDev, outDev);
				} else {
					surface = createNiMidiSurface(inDev, outDev);
				}
			}
		}
		rec->Register("csurf_inst", (void*)surface);
		return 1;
	} else {
		// Unload.
		if (surface) {
			delete surface;
		}
		return 0;
	}
}

}
