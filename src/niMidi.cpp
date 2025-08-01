/*
 * ReaKontrol
 * Support for MIDI protocol used by Komplete Kontrol S-series Mk2, A-series
 * and M-Series
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2018-2019 James Teh
 * License: GNU General Public License version 2.0
 */

#include <algorithm>
#include <string>
#include <sstream>
#include <WDL/db2val.h>
#include <cstring>
#include "reaKontrol.h"

using namespace std;

const unsigned char MIDI_CC = 0xBF;
const unsigned char MIDI_SYSEX_BEGIN[] = {
	0xF0, 0x00, 0x21, 0x09, 0x00, 0x00, 0x44, 0x43, 0x01, 0x00};
const unsigned char MIDI_SYSEX_END = 0xF7;
const int BANK_NUM_TRACKS = 8;

const unsigned char CMD_HELLO = 0x01;
const unsigned char CMD_GOODBYE = 0x02;
const unsigned char CMD_PLAY = 0x10;
const unsigned char CMD_RESTART = 0x11;
const unsigned char CMD_REC = 0x12;
const unsigned char CMD_COUNT = 0x13;
const unsigned char CMD_STOP = 0x14;
const unsigned char CMD_CLEAR = 0x15;
const unsigned char CMD_LOOP = 0x16;
const unsigned char CMD_METRO = 0x17;
const unsigned char CMD_TEMPO = 0x18;
const unsigned char CMD_UNDO = 0x20;
const unsigned char CMD_REDO = 0x21;
const unsigned char CMD_QUANTIZE = 0x22;
const unsigned char CMD_AUTO = 0x23;
const unsigned char CMD_NAV_TRACKS = 0x30;
const unsigned char CMD_NAV_BANKS = 0x31;
const unsigned char CMD_NAV_CLIPS = 0x32;
const unsigned char CMD_NAV_SCENES = 0x33;
const unsigned char CMD_MOVE_TRANSPORT = 0x34;
const unsigned char CMD_MOVE_LOOP = 0x35;
const unsigned char CMD_TRACK_AVAIL = 0x40;
const unsigned char CMD_SEL_TRACK_PARAMS_CHANGED = 0x41;
const unsigned char CMD_TRACK_SELECTED = 0x42;
const unsigned char CMD_TRACK_MUTED = 0x43;
const unsigned char CMD_TRACK_SOLOED = 0x44;
const unsigned char CMD_TRACK_ARMED = 0x45;
const unsigned char CMD_TRACK_VOLUME_TEXT = 0x46;
const unsigned char CMD_TRACK_PAN_TEXT = 0x47;
const unsigned char CMD_TRACK_NAME = 0x48;
const unsigned char CMD_TRACK_VU = 0x49;
const unsigned char CMD_KNOB_VOLUME0 = 0x50;
const unsigned char CMD_KNOB_VOLUME1 = 0x51;
const unsigned char CMD_KNOB_VOLUME2 = 0x52;
const unsigned char CMD_KNOB_VOLUME3 = 0x53;
const unsigned char CMD_KNOB_VOLUME4 = 0x54;
const unsigned char CMD_KNOB_VOLUME5 = 0x55;
const unsigned char CMD_KNOB_VOLUME6 = 0x56;
const unsigned char CMD_KNOB_VOLUME7 = 0x57;
const unsigned char CMD_KNOB_PAN0 = 0x58;
const unsigned char CMD_KNOB_PAN1 = 0x59;
const unsigned char CMD_KNOB_PAN2 = 0x5a;
const unsigned char CMD_KNOB_PAN3 = 0x5b;
const unsigned char CMD_KNOB_PAN4 = 0x5c;
const unsigned char CMD_KNOB_PAN5 = 0x5d;
const unsigned char CMD_KNOB_PAN6 = 0x5e;
const unsigned char CMD_KNOB_PAN7 = 0x5f;
const unsigned char CMD_CHANGE_VOLUME = 0x64;
const unsigned char CMD_CHANGE_PAN = 0x65;
const unsigned char CMD_TOGGLE_MUTE = 0x66;
const unsigned char CMD_TOGGLE_SOLO = 0x67;

const unsigned char TRTYPE_UNSPEC = 1;

// Convert a signed 7 bit MIDI value to a signed char.
// That is, convertSignedMidiValue(127) will return -1.
signed char convertSignedMidiValue(unsigned char value) {
	if (value <= 63) {
		return value;
	}
	return value - 128;
}

unsigned char volToCc(double volume) {
	// Based on:
	// https://github.com/justinfrankel/reaper-sdk/blob/cde283eea2d82e19e473062649a95dc0e799fe37/reaper-plugins/reaper_csurf/csurf_01X.cpp#LL286
	// CC values range from 0 to 127. DB2SLIDER returns a value from 0 to 1000.
	double val = DB2SLIDER(VAL2DB(volume)) * 127.0 / 1000.0;
	val = clamp(val, 0.0, 127.0);
	// Round >= 0.5 up to 1.
	return (unsigned char)(val + 0.5);
}

unsigned char panToCc(double pan) {
	// Based on:
	// https://github.com/justinfrankel/reaper-sdk/blob/cde283eea2d82e19e473062649a95dc0e799fe37/reaper-plugins/reaper_csurf/csurf_01X.cpp#LL295
	// Pan ranges from -1 to 1, so add 1 to give us a range of 0 to 2. Then
	// divide by 2 to give us a fraction of 1. Finally, CC values range from 0 to
	// 127, so multiply to scale accordingly.
	double val = (pan + 1.0) / 2.0 * 127.0;
	val = clamp(val, 0.0, 127.0);
	// Round >= 0.5 up to 1.
	return (unsigned char)(val + 0.5);
}

class NiMidiSurface: public BaseSurface {
	public:
	NiMidiSurface(int inDev, int outDev)
	: BaseSurface(inDev, outDev) {
		this->_sendCc(CMD_HELLO, 0);
	}

	virtual ~NiMidiSurface() {
		this->_sendCc(CMD_GOODBYE, 0);
	}

	virtual const char* GetTypeString() override {
		return "KompleteKontrolNiMidi";
	}

	virtual const char* GetDescString() override {
		return "Komplete Kontrol S-series Mk2/A-series/M-series";
	}

	virtual void SetPlayState(bool play, bool pause, bool rec) override {
		// Update transport button lights
		this->_sendCc(CMD_REC, rec ? 1 : 0);
		if (pause) {
			// since there is no Pause button on KK we indicate it with both Play and Stop lit
			this->_sendCc(CMD_PLAY, 1);
			this->_sendCc(CMD_STOP, 1);
		} else if (play) {
			this->_sendCc(CMD_PLAY, 1);
			this->_sendCc(CMD_STOP, 0);
		} else {
			this->_sendCc(CMD_PLAY, 0);
			this->_sendCc(CMD_STOP, 1);
		}
	}

	virtual void SetRepeatState(bool rep) override {
		// Update repeat (aka loop) button light
		this->_sendCc(CMD_LOOP, rep ? 1 : 0);
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

	protected:
	void _onMidiEvent(MIDI_event_t* event) override {
		if (event->midi_message[0] != MIDI_CC) {
			return;
		}
		unsigned char& command = event->midi_message[1];
		unsigned char& value = event->midi_message[2];

		switch (command) {
			case CMD_HELLO:
				this->_protocolVersion = value;
				break;
			case CMD_PLAY:
				// Toggles between play and pause
				CSurf_OnPlay();
				break;
			case CMD_RESTART:
				CSurf_GoStart();
				if (GetPlayState() & ~1) {
					// Only play if current state is not playing
					CSurf_OnPlay();
				}
				break;
			case CMD_REC:
				CSurf_OnRecord();
				break;
			case CMD_STOP:
				CSurf_OnStop();
				break;
			case CMD_LOOP:
				Main_OnCommand(1068, 0); // Transport: Toggle repeat
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
			case CMD_NAV_BANKS:
				// Value is -1 or 1.
				this->_onBankSelect(convertSignedMidiValue(value));
				break;
			case CMD_NAV_CLIPS:
				// Value is -1 or 1.
				Main_OnCommand(value == 1 ?
					40173 : // Markers: Go to next marker/project end
					40172, // Markers: Go to previous marker/project start
				0);
				break;
			case CMD_MOVE_TRANSPORT:
				CSurf_ScrubAmt(convertSignedMidiValue(value));
				break;
			case CMD_TRACK_SELECTED:
				// Select a track from current bank in Mixer Mode with top row buttons
				this->_onTrackSelect(value);
				break;
			case CMD_KNOB_VOLUME0:
			case CMD_KNOB_VOLUME1:
			case CMD_KNOB_VOLUME2:
			case CMD_KNOB_VOLUME3:
			case CMD_KNOB_VOLUME4:
			case CMD_KNOB_VOLUME5:
			case CMD_KNOB_VOLUME6:
			case CMD_KNOB_VOLUME7:
				this->_onKnobVolumeChange(command, convertSignedMidiValue(value));
				break;
			case CMD_KNOB_PAN0:
			case CMD_KNOB_PAN1:
			case CMD_KNOB_PAN2:
			case CMD_KNOB_PAN3:
			case CMD_KNOB_PAN4:
			case CMD_KNOB_PAN5:
			case CMD_KNOB_PAN6:
			case CMD_KNOB_PAN7:
				this->_onKnobPanChange(command, convertSignedMidiValue(value));
				break;
			default:
				ostringstream s;
				s << "Unhandled MIDI message " << showbase << hex
					<< (int)event->midi_message[0] << " "
					<< (int)event->midi_message[1] << " "
					<< (int)event->midi_message[2] << endl;
				ShowConsoleMsg(s.str().c_str());
				break;
		}
	}

	private:
	int _protocolVersion = 0;
	int _bankStart = -1;

	void _onBankChange() {
		int numInBank = 0;
		int bankEnd = this->_bankStart + BANK_NUM_TRACKS;
		int numTracks = CSurf_NumTracks(false); // If we ever want to show just MCP tracks in KK Mixer View (param) must be (true)
		if (bankEnd > numTracks) {
			bankEnd = numTracks;
			// Mark additional bank tracks as not available
			int lastInBank = numTracks % BANK_NUM_TRACKS;
			for (int i = BANK_NUM_TRACKS - 1; i > lastInBank; --i) {
				this->_sendSysex(CMD_TRACK_AVAIL, 0, i);
			}
		}
		for (int id = this->_bankStart; id < bankEnd; ++id, ++numInBank) {
			MediaTrack* track = CSurf_TrackFromID(id, false);
			if (!track) {
				break;
			}
			this->_sendSysex(CMD_TRACK_AVAIL, TRTYPE_UNSPEC, numInBank);
			int selected = *(int*)GetSetMediaTrackInfo(track, "I_SELECTED", nullptr);
			this->_sendSysex(CMD_TRACK_SELECTED, selected, numInBank);
			int soloState = *(int*)GetSetMediaTrackInfo(track, "I_SOLO", nullptr);
			this->_sendSysex(CMD_TRACK_SOLOED, (soloState==0) ? 0 : 1, numInBank);
			bool muted = *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr);
			this->_sendSysex(CMD_TRACK_MUTED, muted ? 1 : 0, numInBank);
			int armed = *(int*)GetSetMediaTrackInfo(track, "I_RECARM", nullptr);
			this->_sendSysex(CMD_TRACK_ARMED, armed, numInBank);
			double volume = *(double*)GetSetMediaTrackInfo(track, "D_VOL", nullptr);
			char volText[64];
			mkvolstr(volText, volume);
			this->_sendSysex(CMD_TRACK_VOLUME_TEXT, 0, numInBank, volText);
			double pan = *(double*)GetSetMediaTrackInfo(track, "D_PAN", nullptr);
			char panText[64];
			mkpanstr(panText, pan);
			this->_sendSysex(CMD_TRACK_PAN_TEXT, 0, numInBank, volText);
			const char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
			if (!name) {
				name = "";
			}
			this->_sendSysex(CMD_TRACK_NAME, 0, numInBank, name);
			// todo: level meters
			this->_sendCc(CMD_KNOB_VOLUME0 + numInBank, volToCc(volume));
			this->_sendCc(CMD_KNOB_PAN0 + numInBank, panToCc(pan));
		}
		// todo: navigate tracks, navigate banks
	}

	void ClearSelected() {
		// Clear all selected tracks.
		int iSel = 0;
		// really ALL tracks, hence no use of CSurf_NumTracks
		for (int i = 0; i <= GetNumTracks(); i++) {
			GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_SELECTED", &iSel);
		}
	}

	void _onTrackSelect(unsigned char numInBank) {
		int id = this->_bankStart + numInBank;
		if (id > CSurf_NumTracks(false)) {
			return;
		}
		MediaTrack* track = CSurf_TrackFromID(id, false);
		ClearSelected();
		int iSel = 1; // "Select"
		GetSetMediaTrackInfo(track, "I_SELECTED", &iSel);
	}

	void _onBankSelect(signed char value) {
		// Manually switch the bank visible in Mixer View WITHOUT influencing track selection
		int newBankStart = this->_bankStart + (value * BANK_NUM_TRACKS);
		int numTracks = CSurf_NumTracks(false); // If we ever want to show just MCP tracks in KK Mixer View (param) must be (true)
		if ((newBankStart < 0) || (newBankStart > numTracks)) {
			return;
		}
		this->_bankStart = newBankStart;
		this->_onBankChange();
	}

	void _onKnobVolumeChange(unsigned char command, signed char value) {
		int numInBank = command - CMD_KNOB_VOLUME0;
		MediaTrack* track = CSurf_TrackFromID(numInBank + this->_bankStart, false);
		if (!track) {
			return;
		}
		CSurf_OnVolumeChange(track, value / 127.0, true);
	}

	void _onKnobPanChange(unsigned char command, signed char value) {
		int numInBank = command - CMD_KNOB_PAN0;
		MediaTrack* track = CSurf_TrackFromID(numInBank + this->_bankStart, false);
		if (!track) {
			return;
		}
		const double PAN_SCALE_FACTOR = 127 * 8;
		CSurf_OnPanChange(track, value / PAN_SCALE_FACTOR, true);
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

IReaperControlSurface* createNiMidiSurface(int inDev, int outDev) {
	return new NiMidiSurface(inDev, outDev);
}
