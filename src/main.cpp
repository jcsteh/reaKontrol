/*
 * ReaKontrol
 * Main plug-in code
 * Author: brumbear@pacificpeaks
 * Copyright 2019-2020 Pacific Peaks Studio
 * Previous Authors: James Teh <jamie@jantrid.net>, Leonard de Ruijter, brumbear@pacificpeaks, Copyright 2018-2019 James Teh
 * License: GNU General Public License version 2.0
 */

#ifdef _WIN32
#include <windows.h>
#include <SetupAPI.h>
#include <initguid.h>
#include <Usbiodef.h>

#else

#include <sys/types.h>
#include <sys/time.h>

typedef unsigned int DWORD;

DWORD GetTickCount()
{
  // could switch to mach_getabsolutetime() maybe
  struct timeval tm={0,};
  gettimeofday(&tm,NULL);
  return (DWORD) (tm.tv_sec*1000 + tm.tv_usec/1000);
}

#endif

#include <string>
#include <cstring>
#include <sstream>
#define REAPERAPI_IMPLEMENT
#include "reaKontrol.h"

using namespace std;

aList g_actionList; // Global action list structure for ReaKontrol

const char KK_FX_PREFIX[] = "VSTi: Komplete Kontrol";
const char KK_INSTANCE_PARAM_PREFIX[] = "NIKB";


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

BaseSurface::BaseSurface() {
	// ToDo: ???
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

extern "C" {

REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec) {
	if (rec) {
		// Load.
		if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc || REAPERAPI_LoadAPI(rec->GetFunc) != 0) {
			return 0; // Incompatible.
		}

		// Load Global Action list from config file
		const char* pathname = GetResourcePath();
	    std::string s_filename(pathname);
		s_filename += "\\UserPlugins\\ReaKontrolConfig\\reakontrol.ini";
		s_filename.push_back('\0');
		pathname = &s_filename[0];
		if (file_exists(pathname)) {
			std::string s_keyName;
			char* key;
			char stringOut[128] = {}; // large enough for the longest possible action ID or action name
			int stringOut_sz = sizeof(stringOut);
			
			for (int i = 0; i <= 7; ++i) {
				s_keyName = "action_" + std::to_string(i) + "_ID";
				s_keyName.push_back('\0');
				key = &s_keyName[0];
				GetPrivateProfileString("reakontrol_actions", key, nullptr, stringOut, stringOut_sz, pathname); // Windows only. Mac OSX via Swell version of this?
				if (stringOut) {
					g_actionList.ID[i] = NamedCommandLookup(stringOut);
					if (g_actionList.ID[i]) {
						s_keyName = "action_" + std::to_string(i) + "_name";
						s_keyName.push_back('\0');
						key = &s_keyName[0];
						GetPrivateProfileString("reakontrol_actions", key, nullptr, stringOut, stringOut_sz, pathname); // Windows only. Mac OSX via Swell version of this?
						if (stringOut) {
							strcpy(&g_actionList.name[i][0], stringOut);
						}
						else {
							g_actionList.name[i][0] = '\0';
						}
					}
				}
				else {
					g_actionList.ID[i] = 0;
					g_actionList.name[i][0] = '\0';
				}

			}
			// DEBUG
			ostringstream s;
			for (int i = 0; i <= 7; ++i) {
				if (g_actionList.ID[i] != 0) {
					s.str("");
					s << "Action " << i << " " << g_actionList.ID[i] << " ";
					ShowConsoleMsg(s.str().c_str());
					if (g_actionList.name[i][0] != '\0') {
						ShowConsoleMsg(g_actionList.name[i]);
					}
					ShowConsoleMsg("\n");
				}
			}
		}
		else {
			ShowConsoleMsg("Message from ReaKontrol extension: \nConfiguration file not found: reakontrol.ini");
		}

		surface = createNiMidiSurface();
		rec->Register("csurf_inst", (void*)surface);
		return 1;
	} 
	else {
		// Unload.
		if (surface) {
			delete surface;
		}
		return 0;
	}
}

}
