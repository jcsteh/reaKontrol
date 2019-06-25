/*
 * ReaKontrol
 * Support for MCU-based MIDI protocol used by Komplete Kontrol Mk1
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2019 James Teh
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <sstream>
#include <cstring>
#include "reaKontrol.h"

using namespace std;

const unsigned char MIDI_NOTE_ON = 0x90;
const unsigned char MIDI_CC = 0xB0;
const unsigned char MIDI_VAL_OFF = 0;
const unsigned char MIDI_VAL_ON = 0x7F;
const unsigned char MIDI_SYSEX_BEGIN[] = {
	0xF0, 0x00, 0x00, 0x66, 0x14, 0x12, 0x00};
const unsigned char MIDI_SYSEX_SEPARATOR = 0x19;
const unsigned char MIDI_SYSEX_END = 0xF7;

const unsigned char CMD_NAV_LEFT = 0x14;
const unsigned char CMD_NAV_RIGHT = 0x15;
const unsigned char CMD_REWIND = 0x5B;
const unsigned char CMD_FAST_FORWARD = 0x5C;
const unsigned char CMD_STOP = 0x5D;
const unsigned char CMD_PLAY = 0x5E;
const unsigned char CMD_RECORD = 0x5F;

class McuSurface: public BaseSurface {
	public:
	McuSurface(int inDev, int outDev)
	: BaseSurface(inDev, outDev) {
	}

	virtual const char* GetTypeString() override {
		return "KompleteKontrolMcu";
	}

	virtual const char* GetDescString() override {
		return "Komplete Kontrol S-series Mk1";
	}

	protected:
	void _onMidiEvent(MIDI_event_t* event) override {
		ostringstream s;
		s << "MIDI message " << showbase << hex
			<< (int)event->midi_message[0] << " "
			<< (int)event->midi_message[1] << " "
			<< (int)event->midi_message[2] << endl;
		ShowConsoleMsg(s.str().c_str());
		if ((event->midi_message[0] != MIDI_NOTE_ON
				&& event->midi_message[0] != MIDI_CC)
			|| event->midi_message[2] == MIDI_VAL_OFF
		) {
			return;
		}
		switch (event->midi_message[1]) { // Command
			case CMD_NAV_LEFT:
				Main_OnCommand(40286, 0); // Track: Go to previous track
				break;
			case CMD_NAV_RIGHT:
				Main_OnCommand(40285, 0); // Track: Go to next track
				break;
			case CMD_STOP:
				CSurf_OnStop();
				break;
			case CMD_PLAY:
				CSurf_OnPlay();
				break;
			case CMD_RECORD:
				CSurf_OnRecord();
				break;
		}
	}

	virtual void SetSurfaceSelected(MediaTrack* track, bool selected) override {
		if (!selected) {
			return;
		}
		string message((char*)MIDI_SYSEX_BEGIN, sizeof(MIDI_SYSEX_BEGIN));
		char* trackName = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
		if (!trackName || !trackName[0]) {
			trackName = "unnamed";
		}
		message += trackName;
		message += MIDI_SYSEX_SEPARATOR;
		int id = CSurf_TrackToID(track, false);
		ostringstream idStr;
		idStr << id;
		message += idStr.str();
		const string kkInst = getKkInstanceName(track, true);
		if (!kkInst.empty()) {
			message += MIDI_SYSEX_SEPARATOR;
			message += "Komplete Kontrol VST";
			message += MIDI_SYSEX_SEPARATOR;
			message += kkInst;
		}
		message += MIDI_SYSEX_END;
		this->_sendRaw(message);
	}
	
	private:
	void _sendRaw(const string& message) {
		// MIDI_event_t includes 4 bytes for the message, but we need more.
		MIDI_event_t* event = (MIDI_event_t*)new unsigned char[
			sizeof(MIDI_event_t) - 4 + message.length()];
		event->frame_offset = 0;
		event->size = message.length();
		memcpy(event->midi_message, message.c_str(), message.length());
		ostringstream s;
		s << "send raw" << hex << showbase;
		for (int i = 0; i < event->size; ++i) {
			s << " " << (int)event->midi_message[i];
		}
		s << endl;
		ShowConsoleMsg(s.str().c_str());
		this->_midiOut->SendMsg(event, -1);
		delete event;
	}

};

IReaperControlSurface* createMcuSurface(int inDev, int outDev) {
	return new McuSurface(inDev, outDev);
}
