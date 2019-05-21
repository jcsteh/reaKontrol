/*
 * ReaKontrol
 * Support for MIDI protocol used by Komplete Kontrol S-series Mk2, A-series
 * and M-Series
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2018-2019 James Teh
 * License: GNU General Public License version 2.0
 */

#define BASIC_DIAGNOSTICS

#include <string>
#include <sstream>
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
const unsigned char CMD_TRACK_VOLUME_CHANGED = 0x46;
const unsigned char CMD_TRACK_PAN_CHANGED = 0x47;
const unsigned char CMD_TRACK_NAME = 0x48;
const unsigned char CMD_TRACK_VU = 0x49;
const unsigned char CMD_KNOB_VOLUME1 = 0x50;
const unsigned char CMD_KNOB_VOLUME2 = 0x51;
const unsigned char CMD_KNOB_VOLUME3 = 0x52;
const unsigned char CMD_KNOB_VOLUME4 = 0x53;
const unsigned char CMD_KNOB_VOLUME5 = 0x54;
const unsigned char CMD_KNOB_VOLUME6 = 0x55;
const unsigned char CMD_KNOB_VOLUME7 = 0x56;
const unsigned char CMD_KNOB_VOLUME8 = 0x57;
const unsigned char CMD_KNOB_PAN1 = 0x58;
const unsigned char CMD_KNOB_PAN2 = 0x59;
const unsigned char CMD_KNOB_PAN3 = 0x5a;
const unsigned char CMD_KNOB_PAN4 = 0x5b;
const unsigned char CMD_KNOB_PAN5 = 0x5c;
const unsigned char CMD_KNOB_PAN6 = 0x5d;
const unsigned char CMD_KNOB_PAN7 = 0x5e;
const unsigned char CMD_KNOB_PAN8 = 0x5f;
const unsigned char CMD_CHANGE_VOLUME = 0x64;
const unsigned char CMD_CHANGE_PAN = 0x65;
const unsigned char CMD_TOGGLE_MUTE = 0x66;
const unsigned char CMD_TOGGLE_SOLO = 0x67;

const unsigned char TRTYPE_UNSPEC = 1;
const unsigned char TRTYPE_MASTER = 6; // ToDo: consider declaring master track in Mixer View

// State Information
// Uses:
// - save CPU and MIDI bus resources by avoiding unnecessary updates or state information calls
// - filtering (e.g. lowpass smoothing) of values
// ToDo: Rather than using glocal variables consider moving these into the BaseSurface class declaration
static int g_trackInFocus = 0;
static int g_volBank_last[BANK_NUM_TRACKS] = { 0xff };
static int g_panBank_last[BANK_NUM_TRACKS] = { 0xff };

// Convert a signed 7 bit MIDI value to a signed char.
// That is, convertSignedMidiValue(127) will return -1.
signed char convertSignedMidiValue(unsigned char value) {
	if (value <= 63) {
		return value;
	}
	return value - 128;
}

//-----------------------------------------------------------------------------------------------------------------------
// The following conversion functions (C) 2006-2008 Cockos Incorporated
// ToDo: Eliminate those that will eventually not be used (e.g. volToChar)
static double charToVol(unsigned char val)
{
	double pos = ((double)val*1000.0) / 127.0;
	pos = SLIDER2DB(pos);
	return DB2VAL(pos);

}

static unsigned char volToChar(double vol)
{
	double d = (DB2SLIDER(VAL2DB(vol))*127.0 / 1000.0);
	if (d < 0.0)d = 0.0;
	else if (d > 127.0)d = 127.0;

	return (unsigned char)(d + 0.5);
}

static double charToPan(unsigned char val)
{
	double pos = ((double)val*1000.0 + 0.5) / 127.0;

	pos = (pos - 500.0) / 500.0;
	if (fabs(pos) < 0.08) pos = 0.0;

	return pos;
}

static unsigned char panToChar(double pan)
{
	pan = (pan + 1.0)*63.5;

	if (pan < 0.0)pan = 0.0;
	else if (pan > 127.0)pan = 127.0;

	return (unsigned char)(pan + 0.5);
}
// End of conversion functions (C) 2006-2008 Cockos Incorporated
//-----------------------------------------------------------------------------------------------------------------------

static unsigned char peakToChar_KkMk2(double peak)
{
	// Direct linear logarithmic conversion to KK Mk2 Meter Scaling.
	// Contrary to Reaper Peak Meters the dB interval spacing on KK Mk2 displays is NOT linear.
	// It is assumed that other NI Keyboards use the same scaling for the meters.
	// Midi #127 = +6dB #106 = 0dB, #68 = -12dB, #38 = -24dB, #16 = -48dB
	double result = 0.0;
	double a = -3.2391538612390192E+01;
	double b = 3.0673618643561021E+01;
	double c = 8.6720798984917224E+01;
	double d = 4.4920143012996103E+00;
	if (peak > 0.00398107170553497250770252305088) {
		result = a + b * log(c * peak + d); // KK Mk2 display has meaningful resolution only above -48dB
	}
	else {
		result = peak * 4019; // linear approximation below -48dB
	}
	if (result < 0.5) result = 0.5; // if the returned value is 0 then channels further to the right are ignored by KK display!
	else if (result  > 126.5) result = 126.5;
	return (unsigned char)(result + 0.5); // rounding and make sure that minimum value returned is 1
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

	virtual void SetSurfaceSelected(MediaTrack* track, bool selected) override {
		if (selected) {
			int id = CSurf_TrackToID(track, false);
			g_trackInFocus = id;
			int numInBank = id % BANK_NUM_TRACKS;
			int oldBankStart = this->_bankStart;
			this->_bankStart = id - numInBank;
			if (this->_bankStart != oldBankStart) {
				this->_allMixerUpdate();
			}

			// ====================================================================
			// THIS TEMPORARY BLOCK IS JUST FOR TESTING
			// We abuse the SetSurfaceSelected callback to update the entire Mixer
			// on every track selection change.
			// ToDo: Remove this block as soon as the proper callbacks per parameter are in place
			this->_allMixerUpdate(); 
			// ====================================================================
			
			// Let Keyboard know about changed track selection 
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
				this->_onTrackNav(convertSignedMidiValue(value));
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
				// ToDo: Scrubbing very slow. Rather than just amplifying this value
				// have to evaluate incoming MIDI stream to allow for both fine as well
				// coarse scrubbing
				CSurf_ScrubAmt(convertSignedMidiValue(value)); 
				break;
			case CMD_TRACK_SELECTED:
				// Select a track from current bank in Mixer Mode with top row buttons
				this->_onTrackSelect(value);
				break;
			case CMD_KNOB_VOLUME1:
			case CMD_KNOB_VOLUME2:
			case CMD_KNOB_VOLUME3:
			case CMD_KNOB_VOLUME4:
			case CMD_KNOB_VOLUME5:
			case CMD_KNOB_VOLUME6:
			case CMD_KNOB_VOLUME7:
			case CMD_KNOB_VOLUME8:
				this->_onKnobVolumeChange(command, convertSignedMidiValue(value));
				break;
			case CMD_KNOB_PAN1:
			case CMD_KNOB_PAN2:
			case CMD_KNOB_PAN3:
			case CMD_KNOB_PAN4:
			case CMD_KNOB_PAN5:
			case CMD_KNOB_PAN6:
			case CMD_KNOB_PAN7:
			case CMD_KNOB_PAN8:
				this->_onKnobPanChange(command, convertSignedMidiValue(value));
				break;
			default:
#ifdef BASIC_DIAGNOSTICS
				ostringstream s;
				s << "Unhandled MIDI message " << showbase << hex
					<< (int)event->midi_message[0] << " "
					<< (int)event->midi_message[1] << " "
					<< (int)event->midi_message[2] << endl;
				ShowConsoleMsg(s.str().c_str());
#endif
				break;
		}
	}

	void _peakMixerUpdate() override {
		// Peak meters. Note: Reaper reports peak, NOT VU	

		// ToDo: Peak Hold in KK display shall be erased immediately when changing bank
		// ToDo: Peak Hold in KK display shall be erased after decay time t when track muted or no signal.
		// ToDo: Explore the effect of sending CMD_SEL_TRACK_PARAMS_CHANGED after sending CMD_TRACK_VU
		
		// Meter information is sent to KK as array (string of chars) for all 16 channels (8 x stereo) of one bank.
		// A value of 0 will result in stopping to refresh meters further to right.
		// The array needs one additional char at peakBank[16] set to 0 as a "end of string" marker.
		char peakBank[(BANK_NUM_TRACKS * 2) + 1] = { 0 };
		int j = 0;
		double peakValue = 0;		
		int numInBank = 0;
		int bankEnd = this->_bankStart + BANK_NUM_TRACKS - 1;
		int numTracks = CSurf_NumTracks(false); // If we ever want to show just MCP tracks in KK Mixer View (param) must be (true)
		if (bankEnd > numTracks) {
			bankEnd = numTracks;
		}
		for (int id = this->_bankStart; id <= bankEnd; ++id, ++numInBank) {
			MediaTrack* track = CSurf_TrackFromID(id, false);
			if (!track) {
				break;
			}
			j = 2 * numInBank;
			// Muted tracks still report peak levels => ignore these
			if (*(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr)) {
				peakBank[j] = 1;
				peakBank[j+1] = 1;
			}
			else {
				peakValue = Track_GetPeakInfo(track, 0); // left channel
				if (peakValue < 0.0000000298023223876953125) {
					// No log conversion necessary if < -150dB
					peakBank[j] = 1; // if 0 then channels further to the right are ignored by KK display
				}
				else {
					peakBank[j] = peakToChar_KkMk2(peakValue); // returns value between 1 and 127
				}
				peakValue = Track_GetPeakInfo(track, 1); // right channel
				if (peakValue < 0.0000000298023223876953125) {
					// No log conversion necessary if < -150dB
					peakBank[j + 1] = 1; // if 0 then channels further to the right are ignored by KK display
				}
				else {
					peakBank[j + 1] = peakToChar_KkMk2(peakValue); // returns value between 1 and 127					
				}
			}
		}	
		this->_sendSysex(CMD_TRACK_VU, 2, 0, peakBank);
	}

	private:
	int _protocolVersion = 0;
	int _bankStart = -1;

	void _allMixerUpdate() {
		int numInBank = 0;
		int bankEnd = this->_bankStart + BANK_NUM_TRACKS - 1; // avoid ambiguity: track counting always zero based
		int numTracks = CSurf_NumTracks(false); // If we ever want to show just MCP tracks in KK Mixer View (param) must be (true)
		if (bankEnd > numTracks) {
			bankEnd = numTracks;
			// Mark additional bank tracks as not available
			int lastInBank = numTracks % BANK_NUM_TRACKS;
			for (int i = 7; i > lastInBank; --i) {
				this->_sendSysex(CMD_TRACK_AVAIL, 0, i);
			}
		}
		for (int id = this->_bankStart; id <= bankEnd; ++id, ++numInBank) {
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
			// todo: volume text, pan text
			char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
			if (!name) {
				name = "";
			}
			this->_sendSysex(CMD_TRACK_NAME, 0, numInBank, name);
			// todo: level meters, volume, pan
		}
		// todo: navigate tracks, navigate banks. NOTE: probably not here
	}

	void ClearSelected() {
		// Clear all selected tracks. Copyright (c) 2010 and later Tim Payne (SWS)
		int iSel = 0;
		for (int i = 0; i <= GetNumTracks(); i++) // really ALL tracks, hence no use of CSurf_NumTracks
			GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_SELECTED", &iSel);
	}

	void _onTrackSelect(unsigned char numInBank) {
		int id = this->_bankStart + numInBank;
		if (id > CSurf_NumTracks(false)) {
			return;
		}
		MediaTrack* track = CSurf_TrackFromID(id, false);
		int iSel = 1; // "Select"
					  // If we rather wanted to "Toggle" than just "Select" we would use:
					  // int iSel = 0;
					  // int iSel = *(int*)GetSetMediaTrackInfo(track, "I_SELECTED", nullptr) ? 0 : 1; 
		ClearSelected(); 
		GetSetMediaTrackInfo(track, "I_SELECTED", &iSel);		
	}

	void _onTrackNav(signed char value) {
		// Move to next/previous track relative to currently focused track = last selected track
		if (((g_trackInFocus <= 1) && (value < 0)) ||
			((g_trackInFocus >= CSurf_NumTracks(false))) && (value >0 )) {
			return;
		}
		int id = g_trackInFocus + value;
		MediaTrack* track = CSurf_TrackFromID(id, false);
		int iSel = 1; // "Select"
		ClearSelected();
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
		this->_allMixerUpdate();
	}

	void _onKnobVolumeChange(unsigned char command, signed char value) {
		int numInBank = command - CMD_KNOB_VOLUME1;
		double dvalue = static_cast<double>(value);
		MediaTrack* track = CSurf_TrackFromID(numInBank + this->_bankStart, false);
		if (!track) {
			return;
		}
		CSurf_OnVolumeChange(track, dvalue * 0.007874, true); // scaling by dividing by 127 (0.007874) 
	}

	void _onKnobPanChange(unsigned char command, signed char value) {
		int numInBank = command - CMD_KNOB_PAN1;
		double dvalue = static_cast<double>(value);
		MediaTrack* track = CSurf_TrackFromID(numInBank + this->_bankStart, false);
		if (!track) {
			return;
		}
		CSurf_OnPanChange(track, dvalue * 0.00098425, true); // scaling by dividing by 127*8 (0.00098425)
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
