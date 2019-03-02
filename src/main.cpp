/*
 * ReaKontrol
 * Main plug-in code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2018-2019 James Teh
 * License: GNU General Public License version 2.0
 */

#include <windows.h>
#include <string>
#include <sstream>
#include <cstring>
#define REAPERAPI_IMPLEMENT
#include "reaKontrol.h"

using namespace std;

const char KKS_DEVICE_NAME[] = "Komplete Kontrol DAW - 1";
const char KKA_DEVICE_NAME[] = "Komplete Kontrol A DAW";
const unsigned char MIDI_CC = 0xBF;
const unsigned char MIDI_SYSEX_BEGIN[] = {
	0xF0, 0x00, 0x21, 0x09, 0x00, 0x00, 0x44, 0x43, 0x01, 0x00};
const unsigned char MIDI_SYSEX_END = 0xF7;
const char KK_FX_PREFIX[] = "VSTi: Komplete Kontrol";
const char KK_INSTANCE_PARAM_PREFIX[] = "NIKB";
const int BANK_NUM_TRACKS = 8;

const unsigned char CMD_HELLO = 0x01;
const unsigned char CMD_GOODBYE = 0x02;
const unsigned char CMD_PLAY = 0x10;
const unsigned char CMD_RESTART = 0x11;
const unsigned char CMD_REC = 0x12;
const unsigned char CMD_STOP = 0x14;
const unsigned char CMD_METRO = 0x17;
const unsigned char CMD_TEMPO = 0x18;
const unsigned char CMD_UNDO = 0x20;
const unsigned char CMD_REDO = 0x21;
const unsigned char CMD_NAV_TRACKS = 0x30;
const unsigned char CMD_MOVE_TRANSPORT = 0x34;
const unsigned char CMD_TRACK_AVAIL = 0x40;
const unsigned char CMD_SEL_TRACK_PARAMS_CHANGED = 0x41;
const unsigned char CMD_TRACK_SELECTED = 0x42;
const unsigned char CMD_TRACK_NAME = 0x48;

const unsigned char TRTYPE_UNSPEC = 1;

int getKkMidiInput() {
	int count = GetNumMIDIInputs();
	for (int dev = 0; dev < count; ++dev) {
		char name[30];
		bool present = GetMIDIInputName(dev, name, sizeof(name));
		if (!present) {
			continue;
		}
		if (strcmp(name, KKS_DEVICE_NAME) == 0
				|| strcmp(name, KKA_DEVICE_NAME) == 0) {
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
				|| strcmp(name, KKA_DEVICE_NAME) == 0) {
			return dev;
		}
	}
	return -1;
}

const string getKkInstanceName(MediaTrack* track) {
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
		if (strncmp(paramName, KK_INSTANCE_PARAM_PREFIX, sizeof(KK_INSTANCE_PARAM_PREFIX) - 1) != 0) {
			return "";
		}
		return paramName;
	}
	return "";
}

class KkSurface: IReaperControlSurface {
	public:
	KkSurface(int inDev, int outDev) {
		this->_midiIn = CreateMIDIInput(inDev);
		this->_midiOut = CreateMIDIOutput(outDev, false, nullptr);
		if (!this->_midiIn || !this->_midiOut) {
			return;
		}
		this->_midiIn->start();
		this->_sendCc(CMD_HELLO, 0);
	}

	virtual ~KkSurface() {
		if (this->_midiIn)  {
			this->_midiIn->stop();
			delete this->_midiIn;
		}
		if (this->_midiOut) {
			this->_sendCc(CMD_GOODBYE, 0);
			delete this->_midiOut;
		}
	}

	virtual const char* GetTypeString() override {
		return "KompleteKontrol";
	}

	virtual const char* GetDescString() override {
		return "Komplete Kontrol";
	}

	virtual const char* GetConfigString() override {
		return "";
	}

	virtual void Run() override {
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

	virtual void SetTrackListChange() override {
	}

	virtual void SetSurfaceSelected(MediaTrack* track, bool selected) override {
		if (selected) {
			int id = CSurf_TrackToID(track, false);
			int numInBank = id % BANK_NUM_TRACKS;
			int oldBankStart = this->_bankStart;
			this->_bankStart = id - numInBank;
			if (this->_bankStart != oldBankStart) {
				this->_onBankChange();
			}
			this->_sendSysex(CMD_TRACK_SELECTED, 1, numInBank);
			this->_sendSysex(CMD_SEL_TRACK_PARAMS_CHANGED, 0, 0,
				getKkInstanceName(track));
		}
	}

	private:
	midi_Input* _midiIn = nullptr;
	midi_Output* _midiOut = nullptr;
	int _protocolVersion = 0;
	int _bankStart = -1;

	void _onMidiEvent(MIDI_event_t* event) {
		if (event->midi_message[0] != MIDI_CC) {
			return;
		}
		unsigned char& value = event->midi_message[2];
		switch (event->midi_message[1]) { // Command
			case CMD_HELLO:
				this->_protocolVersion = value;
				break;
			case CMD_PLAY:
				CSurf_OnPlay();
				break;
			case CMD_REC:
				CSurf_OnRecord();
				break;
			case CMD_STOP:
				CSurf_OnStop();
				break;
			case CMD_METRO:
				Main_OnCommand(40364, 0); // Options: Toggle metronome
				break;
			case CMD_TEMPO:
				Main_OnCommand(1134, 0); // Transport: Tap tempo
				break;
			case CMD_UNDO:
				Main_OnCommand(40029, 0); // Edit: Undo
				break;
			case CMD_REDO:
				Main_OnCommand(40030, 0); // Edit: Redo
				break;
			case CMD_NAV_TRACKS:
				// Value is -1 or 1.
				Main_OnCommand(value == 1 ?
					40285 : // Track: Go to next track
					40286, // Track: Go to previous track
				0);
				break;
			case CMD_MOVE_TRANSPORT:
				// value is -1 or 1.
				double amount = (signed char)value;
				CSurf_ScrubAmt(amount);
				break;
		}
	}

	void _onBankChange() {
		int numInBank = 0;
		int bankEnd = this->_bankStart + BANK_NUM_TRACKS;
		int trackCount = CountTracks(nullptr);
		if (bankEnd >= trackCount) {
			bankEnd = trackCount - 1;
		}
		for (int id = this->_bankStart; id < bankEnd; ++id, ++numInBank) {
			MediaTrack* track = CSurf_TrackFromID(id, false);
			this->_sendSysex(CMD_TRACK_AVAIL, TRTYPE_UNSPEC, numInBank);
			int selected = *(int*)GetSetMediaTrackInfo(track, "I_SELECTED", nullptr);
			this->_sendSysex(CMD_TRACK_SELECTED, selected, numInBank);
			// todo: muted, soloed, armed, volume text, pan text
			char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
			this->_sendSysex(CMD_TRACK_NAME, 0, numInBank, name);
			// todo: level meters, volume, pan
		}
		// todo: navigate tracks, navigate banks
	}

	void _sendCc(unsigned char command, unsigned char value) {
		if (this->_midiOut) {
			this->_midiOut->Send(MIDI_CC, command, value, -1);
		}
	}

	void _sendSysex(unsigned char command, unsigned char value,
		unsigned char track, const string& info = ""
	) {
		if (!this->_midiOut) {
			return;
		}
		int length = sizeof(MIDI_SYSEX_BEGIN)
			+ 3 // command, value, track
			+ info.length()
			+ sizeof(MIDI_SYSEX_END);
		// MIDI_event_t includes 4 bytes for the message, but we need more.
		MIDI_event_t* event = (MIDI_event_t*)new unsigned char[
			sizeof(MIDI_event_t) - 4 + length];
		event->frame_offset = 0;
		event->size = length;
		memcpy(event->midi_message, MIDI_SYSEX_BEGIN,
			sizeof(MIDI_SYSEX_BEGIN));
		int messagePos = sizeof(MIDI_SYSEX_BEGIN);
		event->midi_message[messagePos++] = command;
		event->midi_message[messagePos++] = value;
		event->midi_message[messagePos++] = track;
		memcpy(event->midi_message + messagePos, info.c_str(), info.length());
		messagePos += info.length();
		event->midi_message[messagePos++] = MIDI_SYSEX_END;
		this->_midiOut->SendMsg(event, -1);
		delete event;
	}

};

KkSurface* surface = nullptr;

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
				surface = new KkSurface(inDev, outDev);
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
