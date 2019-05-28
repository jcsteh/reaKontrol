/*
 * ReaKontrol
 * Support for MIDI protocol used by Komplete Kontrol S-series Mk2, A-series
 * and M-Series
 * Authors: James Teh <jamie@jantrid.net>, Leonard de Ruijter, brumbear@pacificpeaks
 * Copyright 2018-2019 James Teh
 * Copyright 2019 Pacific Peaks Studio
 * License: GNU General Public License version 2.0
 */

#define DEBUG_DIAGNOSTICS
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
const unsigned char CMD_COUNT = 0x13; // ToDo: ?
const unsigned char CMD_STOP = 0x14;
const unsigned char CMD_CLEAR = 0x15; // ToDo: ?
const unsigned char CMD_LOOP = 0x16;
const unsigned char CMD_METRO = 0x17;
const unsigned char CMD_TEMPO = 0x18;
const unsigned char CMD_UNDO = 0x20;
const unsigned char CMD_REDO = 0x21;
const unsigned char CMD_QUANTIZE = 0x22; // ToDo: ?
const unsigned char CMD_AUTO = 0x23; // ToDo: Toggle between automation modes
const unsigned char CMD_NAV_TRACKS = 0x30;
const unsigned char CMD_NAV_BANKS = 0x31;
const unsigned char CMD_NAV_CLIPS = 0x32;
const unsigned char CMD_NAV_SCENES = 0x33; // not used
const unsigned char CMD_MOVE_TRANSPORT = 0x34;
const unsigned char CMD_MOVE_LOOP = 0x35; // not used
const unsigned char CMD_TRACK_AVAIL = 0x40;
const unsigned char CMD_SET_KK_INSTANCE = 0x41;
const unsigned char CMD_TRACK_SELECTED = 0x42;
const unsigned char CMD_TRACK_MUTED = 0x43;
const unsigned char CMD_TRACK_SOLOED = 0x44;
const unsigned char CMD_TRACK_ARMED = 0x45;
const unsigned char CMD_TRACK_VOLUME_TEXT = 0x46;
const unsigned char CMD_TRACK_PAN_TEXT = 0x47;
const unsigned char CMD_TRACK_NAME = 0x48;
const unsigned char CMD_TRACK_VU = 0x49;
const unsigned char CMD_TRACK_MUTED_BY_SOLO = 0x4A; // ToDo: Check back with newer KK firmware if this gets implemented correctly
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
const unsigned char CMD_PLAY_CLIP = 0x60; // Used here to switch Mixer view to the bank containing the currently focused (= selected) track
const unsigned char CMD_STOP_CLIP = 0x61; // not used
const unsigned char CMD_PLAY_SCENE = 0x62; // not used
const unsigned char CMD_RECORD_SESSION = 0x63; // not used
const unsigned char CMD_CHANGE_SEL_TRACK_VOLUME = 0x64;
const unsigned char CMD_CHANGE_SEL_TRACK_PAN = 0x65;
const unsigned char CMD_TOGGLE_SEL_TRACK_MUTE = 0x66;
const unsigned char CMD_TOGGLE_SEL_TRACK_SOLO = 0x67;
const unsigned char CMD_SEL_TRACK_AVAILABLE = 0x68; // ToDo: ?
const unsigned char CMD_SEL_TRACK_MUTED_BY_SOLO = 0x69; // ToDo: ?

const unsigned char TRTYPE_UNSPEC = 1;
const unsigned char TRTYPE_MASTER = 6;

// State Information
// ToDo: Rather than using glocal variables consider moving these into the BaseSurface class declaration
static int g_trackInFocus = 0;
static bool g_anySolo = false;
static int g_soloStateBank[BANK_NUM_TRACKS] = { 0 };

signed char convertSignedMidiValue(unsigned char value) {
	// Convert a signed 7 bit MIDI value to a signed char.
	// That is, convertSignedMidiValue(127) will return -1.
	if (value <= 63) {
		return value;
	}
	return value - 128;
}

static unsigned char panToChar(double pan)
{
	pan = (pan + 1.0)*63.5;

	if (pan < 0.0)pan = 0.0;
	else if (pan > 127.0)pan = 127.0;

	return (unsigned char)(pan + 0.5);
}

static unsigned char volToChar_KkMk2(double volume) {
	// Direct linear logarithmic conversion to KK Mk2 Meter Scaling.
	// Contrary to Reaper's Peak Volume Meters the dB interval spacing on KK Mk2 displays is NOT linear.
	// It is assumed that other NI Keyboards use the same scaling for the meters.
	// Midi #127 = +6dB #106 = 0dB, #68 = -12dB, #38 = -24dB, #16 = -48dB, #2 = -96dB, #1 = -infinite
	constexpr double minus48dB = 0.00398107170553497250;
	constexpr double minus96dB = 1.5848931924611134E-05;
	constexpr double m = (16.0 - 2.0) / (minus48dB - minus96dB);
	constexpr double n = 16.0 - m * minus48dB;
	constexpr double a = -3.2391538612390192E+01;
	constexpr double b = 3.0673618643561021E+01;
	constexpr double c = 8.6720798984917224E+01;
	constexpr double d = 4.4920143012996103E+00;
	double result = 0.0;
	if (volume > minus48dB) {
		result = a + b * log(c * volume + d); // KK Mk2 display has meaningful resolution only above -48dB
	}
	else if (volume > minus96dB) {
		result = m * volume + n; // linear approximation between -96dB and -48dB
	}
	else {
		result = 0.5; // will be rounded to 1
	}
	if (result > 126.5) {
		result = 126.5;
	}
	return (unsigned char)(result + 0.5); // rounding and make sure that minimum value returned is 1
}

// ToDo: Some Reaper callbacks (Set....) are called unecessarily (?) often in Reaper, see various forum reports & comments from schwa
// => Consider state checking statements at beginning of every callback to return immediately if call is not necessary -> save CPU

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
		if (rec) {
			this->_sendCc(CMD_REC, 1);
		}
		else {
			this->_sendCc(CMD_REC, 0);
		}
		if (pause) {
			this->_sendCc(CMD_PLAY, 1);
			this->_sendCc(CMD_STOP, 1); // since there is no Pause button on KK we indicate it with both Play and Stop lit
		}
		else if (play) {
			this->_sendCc(CMD_PLAY, 1);
			this->_sendCc(CMD_STOP, 0);
		}
		else {
			this->_sendCc(CMD_PLAY, 0);
			this->_sendCc(CMD_STOP, 1);
		}
	}
		
	virtual void SetRepeatState(bool rep) override {
		// Update repeat (aka loop) button light
		if (rep) {
			this->_sendCc(CMD_LOOP, 1);
		}
		else {
			this->_sendCc(CMD_LOOP, 0);
		}
	}

	// ToDo: add more button lights: AUTO, tbd: undo/redo, clear, quantize, tempo
		
	virtual void SetTrackListChange() override {
		// If tracklist changes update Mixer View and ensure sanity of track and bank focus
		int numTracks = CSurf_NumTracks(false);
		// Protect against loosing track focus that could impede track navigation. Set focus on last track in this case.
		if (g_trackInFocus > numTracks) {
			g_trackInFocus = numTracks; 
			// Unfortunately we cannot afford to explicitly select the last track automatically because this could screw up
			// running actions or macros. The plugin must not manipulate track selection without the user deliberately triggering
			// track selection/navigation on the keyboard (or from within Reaper).
		}
		// Protect against loosing bank focus. Set focus on last bank in this case.
		if (this->_bankStart > numTracks) {
			int lastInLastBank = numTracks % BANK_NUM_TRACKS;
			this->_bankStart = numTracks - lastInLastBank;
		}
		// If no track is selected at all (e.g. if previously selected track got removed), then this will now also show up in the
		// Mixer View. However, KK instance focus may still be present! This can be a little bit confusing for the user as typically
		// the track holding the focused KK instance will also be selected. This situation gets resolved as soon as any form of
		// track navigation/selection happens (from keyboard or from within Reaper).
		this->_allMixerUpdate();
	}
		
	virtual void SetSurfaceSelected(MediaTrack* track, bool selected) override {
		// Using SetSurfaceSelected() rather than OnTrackSelection():
		// SetSurfaceSelected() is less economical because it will be called multiple times (also for unselecting tracks).
		// However, SetSurfaceSelected() is the more robust choice because of: https://forum.cockos.com/showpost.php?p=2138446&postcount=15
		// See also notes in SetSurfaceRecArm
		if (selected) {
			int id = CSurf_TrackToID(track, false);			
			int numInBank = id % BANK_NUM_TRACKS;
			int oldBankStart = this->_bankStart;
			this->_bankStart = id - numInBank;
			if (this->_bankStart != oldBankStart) {
				// Update everything
				this->_allMixerUpdate();
			}
			else {
				// Update track names
				this->_namesMixerUpdate();
			}
			// Let Keyboard know about changed track selection
			this->_sendSysex(CMD_TRACK_SELECTED, 1, numInBank);
			// Set KK Instance Focus
			g_trackInFocus = id;
			this->_sendSysex(CMD_SET_KK_INSTANCE, 0, 0,
				getKkInstanceName(track));
		}
	}
	
	virtual void SetSurfaceVolume(MediaTrack* track, double volume) override {		
		int id = CSurf_TrackToID(track, false);
		if ((id >= this->_bankStart) && (id < this->_bankStart + BANK_NUM_TRACKS)) {
			int numInBank = id % BANK_NUM_TRACKS;
			char volText[64] = { 0 };
			mkvolstr(volText, volume);
			this->_sendSysex(CMD_TRACK_VOLUME_TEXT, 0, numInBank, volText);
			this->_sendCc((CMD_KNOB_VOLUME0 + numInBank), volToChar_KkMk2(volume * 1.05925));
		}
	}

	virtual void SetSurfacePan(MediaTrack* track, double pan) override {
		int id = CSurf_TrackToID(track, false);
		if ((id >= this->_bankStart) && (id < this->_bankStart + BANK_NUM_TRACKS)) {
			int numInBank = id % BANK_NUM_TRACKS;
			char panText[64];
			mkpanstr(panText, pan);
			this->_sendSysex(CMD_TRACK_PAN_TEXT, 0, numInBank, panText); // KK firmware 0.5.7 uses internal text
			this->_sendCc((CMD_KNOB_PAN0 + numInBank), panToChar(pan));
		}
	}
		
	virtual void SetSurfaceMute(MediaTrack* track, bool mute) override {
		int id = CSurf_TrackToID(track, false);
		if (id == g_trackInFocus) {
			this->_sendCc(CMD_TOGGLE_SEL_TRACK_MUTE, mute ? 1 : 0); // ToDo: does not work yet - why? Is an extra track_available required? Or instance? Or SysEx?			
		}
		if ((id >= this->_bankStart) && (id < this->_bankStart + BANK_NUM_TRACKS)) {
			int numInBank = id % BANK_NUM_TRACKS;
			this->_sendSysex(CMD_TRACK_MUTED, mute ? 1 : 0, numInBank);
		}
	}
		
	virtual void SetSurfaceSolo(MediaTrack* track, bool solo) override {
		// Note: Solo in Reaper can have different meanings (Solo In Place, Solo In Front and much more -> Reaper Preferences)
		int id = CSurf_TrackToID(track, false);
		// Ignore solo on master, id = 0 is only used as an "any track is soloed" indicator
		if (id == 0) {
			g_anySolo = solo;
			return;
		}
		if (id == g_trackInFocus) {
			this->_sendCc(CMD_TOGGLE_SEL_TRACK_SOLO, solo ? 1 : 0); // ToDo: does not work yet - why? Is an extra track_available required? Or instance? Or SysEx?			
		}
		if ((id >= this->_bankStart) && (id < this->_bankStart + BANK_NUM_TRACKS)) {
			int numInBank = id % BANK_NUM_TRACKS;
			if (solo) {
				g_soloStateBank[numInBank] = 1;
				this->_sendSysex(CMD_TRACK_SOLOED, 1, numInBank);
				this->_sendSysex(CMD_SEL_TRACK_MUTED_BY_SOLO, 0, numInBank);
			}
			else {
				g_soloStateBank[numInBank] = 0;
				this->_sendSysex(CMD_TRACK_SOLOED, 0, numInBank);
				this->_sendSysex(CMD_SEL_TRACK_MUTED_BY_SOLO, g_anySolo ? 1 : 0, numInBank);
			}
		}
	}

	virtual void SetSurfaceRecArm(MediaTrack* track, bool armed) override {
		// ToDo: ISSUE: it seems (confirmed by forum reports) that record arm in Reaper leads to a cascade of callbacks
		// In our case it seems that also the SetSurfaceSelected gets triggered although no track selection change happened
		// This may even depend on settings in preferences? 
		// => Consider filtering this somehow with state variables in SetSurfaceSelected or just live with it...
		int id = CSurf_TrackToID(track, false);
		if ((id >= this->_bankStart) && (id < this->_bankStart + BANK_NUM_TRACKS)) {
			int numInBank = id % BANK_NUM_TRACKS;
			this->_sendSysex(CMD_TRACK_ARMED, armed ? 1 : 0, numInBank);
		}
	}

	protected:
	void _onMidiEvent(MIDI_event_t* event) override {
		if (event->midi_message[0] != MIDI_CC) {
			return;
		}
		unsigned char& command = event->midi_message[1];
		unsigned char& value = event->midi_message[2];
		
#ifdef DEBUG_DIAGNOSTICS
		ostringstream s;
		s << "Diagnostic: MIDI " << showbase << hex
			<< (int)event->midi_message[0] << " "
			<< (int)event->midi_message[1] << " "
			<< (int)event->midi_message[2] << " Focus Track "
			<< g_trackInFocus << " Bank Start "
			<< this->_bankStart << " anySolo "
			<< g_anySolo << endl;
		ShowConsoleMsg(s.str().c_str());
#endif

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
			case CMD_TRACK_MUTED:
				// Toggle mute for a a track from current bank in Mixer Mode with top row buttons
				this->_onTrackMute(value);
				break;
			case CMD_TRACK_SOLOED:
				// Toggle solo for a a track from current bank in Mixer Mode with top row buttons
				this->_onTrackSolo(value);
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
			case CMD_PLAY_CLIP:
				// We use this for a different purpose: switch Mixer view to the bank containing the currently focused (= selected) track
				this->_bankStart = (int)(g_trackInFocus / BANK_NUM_TRACKS) * BANK_NUM_TRACKS;
				this->_allMixerUpdate();
				break;
			case CMD_CHANGE_SEL_TRACK_VOLUME:
				this->_onSelTrackVolumeChange(convertSignedMidiValue(value));
				break;
			case CMD_CHANGE_SEL_TRACK_PAN:
				this->_onSelTrackPanChange(convertSignedMidiValue(value));
				break;
			case CMD_TOGGLE_SEL_TRACK_MUTE:
				this->_onSelTrackMute();
				break;
			case CMD_TOGGLE_SEL_TRACK_SOLO:
				this->_onSelTrackSolo();
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
		// A value of 0 will result in stopping to refresh meters further to right as it is interpretated as "end of string".
		// peakBank[0]..peakBank[31] are used for data. The array needs one additional last char peakBank[32] set as "end of string" marker.
		char peakBank[(BANK_NUM_TRACKS * 2) + 1];
		peakBank[(BANK_NUM_TRACKS * 2)] = '\0'; // "end of string" marker
		int j = 0;
		double peakValue = 0;		
		int numInBank = 0;
		int bankEnd = this->_bankStart + BANK_NUM_TRACKS - 1;
		int numTracks = CSurf_NumTracks(false); 		
		if (bankEnd > numTracks) {
			bankEnd = numTracks;
			int lastInLastBank = numTracks % BANK_NUM_TRACKS;
			peakBank[(lastInLastBank + 1) * 2] = '\0'; // end of string (no tracks available further to the right)
		}
		for (int id = this->_bankStart; id <= bankEnd; ++id, ++numInBank) {
			MediaTrack* track = CSurf_TrackFromID(id, false);
			if (!track) {
				break;
			}
			j = 2 * numInBank;

			// ToDo: Check with KK firmware improvements if CMD_TRACK_MUTED_BY_SOLO will eventually be implemented correctly.
		    // In that case the meter bars should be grey and we do not have to supress them entirely as implemented now.

			// If any track is soloed then only soloed tracks and the master show peaks (irrespective of their mute state)
			if (g_anySolo) {
				if ((g_soloStateBank[numInBank] == 0) && (numInBank != 0)) {
					peakBank[j] = 1;
					peakBank[j + 1] = 1;
				}
				else {
					peakValue = Track_GetPeakInfo(track, 0); // left channel
					peakBank[j] = volToChar_KkMk2(peakValue); // returns value between 1 and 127
					peakValue = Track_GetPeakInfo(track, 1); // right channel
					peakBank[j + 1] = volToChar_KkMk2(peakValue); // returns value between 1 and 127
				}
			}
			// If no tracks are soloed then muted tracks shall show no peaks
			else {
				if (*(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr)) {
					peakBank[j] = 1;
					peakBank[j + 1] = 1;
				}
				else {
					peakValue = Track_GetPeakInfo(track, 0); // left channel
					peakBank[j] = volToChar_KkMk2(peakValue); // returns value between 1 and 127
					peakValue = Track_GetPeakInfo(track, 1); // right channel
					peakBank[j + 1] = volToChar_KkMk2(peakValue); // returns value between 1 and 127					
				}
			}
		}	
		this->_sendSysex(CMD_TRACK_VU, 2, 0, peakBank);
	}

	// Copyright (c) 2010 and later Tim Payne (SWS), Jeffos
	void* GetConfigVar(const char* cVar) {
		int sztmp;
		void* p = NULL;
		if (int iOffset = projectconfig_var_getoffs(cVar, &sztmp))
		{
			p = projectconfig_var_addr(EnumProjects(-1, NULL, 0), iOffset);
		}
		else
		{
			p = get_config_var(cVar, &sztmp);
		}
		return p;
	}

	// ToDo: try to replace this whole polling scheme with CSURF_EXT_SETMETRONOME 0x00010002 // parm1=0 <-> disable, !0 <-> enable
	// Ideally this should work as a callback, i.e. interrupt driven rather than polling
	void _metronomeUpdate() override {
		this->_sendCc(CMD_METRO, (*(int*)GetConfigVar("projmetroen") & 1));
	}

	private:
	int _protocolVersion = 0;
	int _bankStart = -1;

	void _allMixerUpdate() {
		int numInBank = 0;
		int bankEnd = this->_bankStart + BANK_NUM_TRACKS - 1; // avoid ambiguity: track counting always zero based
		int numTracks = CSurf_NumTracks(false); 
		// Set bank select button lights
		int bankLights = 3; // left and right on
		if (numTracks < BANK_NUM_TRACKS) {
			bankLights = 0; // left and right off
		}
		else if (this->_bankStart == 0) {
			bankLights = 2; // left off, right on
		}
		else if (bankEnd >= numTracks) {
			bankLights = 1; // left on, right off
		}
		this->_sendCc(CMD_NAV_BANKS, bankLights);
		if (bankEnd > numTracks) {
			bankEnd = numTracks;
			// Mark additional bank tracks as not available
			int lastInLastBank = numTracks % BANK_NUM_TRACKS;
			for (int i = 7; i > lastInLastBank; --i) {
				this->_sendSysex(CMD_TRACK_AVAIL, 0, i);				
			}
		}
		for (int id = this->_bankStart; id <= bankEnd; ++id, ++numInBank) {
			// ToDo: While not really necessary consider removing some info polls for master track (i.e. record arm, solo)
			MediaTrack* track = CSurf_TrackFromID(id, false);
			if (!track) {
				break;
			}			
			if (id == 0) {
				this->_sendSysex(CMD_TRACK_AVAIL, TRTYPE_MASTER, numInBank);
			}
			else {
				this->_sendSysex(CMD_TRACK_AVAIL, TRTYPE_UNSPEC, numInBank);
			}			
			int selected = *(int*)GetSetMediaTrackInfo(track, "I_SELECTED", nullptr);
			this->_sendSysex(CMD_TRACK_SELECTED, selected, numInBank);
			int soloState = *(int*)GetSetMediaTrackInfo(track, "I_SOLO", nullptr);
			if (soloState == 0) {
				g_soloStateBank[numInBank] = 0;
				this->_sendSysex(CMD_TRACK_SOLOED, 0, numInBank);
				this->_sendSysex(CMD_SEL_TRACK_MUTED_BY_SOLO, g_anySolo ? 1 : 0, numInBank);
			}
			else {
				g_soloStateBank[numInBank] = 1;
				this->_sendSysex(CMD_TRACK_SOLOED, 1, numInBank);
				this->_sendSysex(CMD_SEL_TRACK_MUTED_BY_SOLO, 0, numInBank);
			}
			bool muted = *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr);
			this->_sendSysex(CMD_TRACK_MUTED, muted ? 1 : 0, numInBank);
			int armed = *(int*)GetSetMediaTrackInfo(track, "I_RECARM", nullptr);
			this->_sendSysex(CMD_TRACK_ARMED, armed, numInBank);
			double volume = *(double*)GetSetMediaTrackInfo(track, "D_VOL", nullptr);
			char volText[64];
			mkvolstr(volText, volume);
			this->_sendSysex(CMD_TRACK_VOLUME_TEXT, 0, numInBank, volText);
			this->_sendCc((CMD_KNOB_VOLUME0 + numInBank), volToChar_KkMk2(volume * 1.05925));			
			double pan = *(double*)GetSetMediaTrackInfo(track, "D_PAN", nullptr);
			char panText[64];	
			mkpanstr(panText, pan);
			this->_sendSysex(CMD_TRACK_PAN_TEXT, 0, numInBank, panText); // KK firmware 0.5.7 uses internal text
			this->_sendCc((CMD_KNOB_PAN0 + numInBank), panToChar(pan));
			char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
			if (!name) {
				name = "";
			}
			this->_sendSysex(CMD_TRACK_NAME, 0, numInBank, name);
		}	
	}

	void _namesMixerUpdate() {
		int numInBank = 0;
		int bankEnd = this->_bankStart + BANK_NUM_TRACKS - 1; // avoid ambiguity: track counting always zero based
		int numTracks = CSurf_NumTracks(false);
		if (bankEnd > numTracks) {
			bankEnd = numTracks;
		}
		for (int id = this->_bankStart; id <= bankEnd; ++id, ++numInBank) {
			MediaTrack* track = CSurf_TrackFromID(id, false);
			if (!track) {
				break;
			}
			char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
			if (!name) {
				name = "";
			}
			this->_sendSysex(CMD_TRACK_NAME, 0, numInBank, name);
		}
	}

	void _clearAllSelectedTracks() {
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
		_clearAllSelectedTracks(); 
		GetSetMediaTrackInfo(track, "I_SELECTED", &iSel);
	}

	void _onTrackNav(signed char value) {
		// Move to next/previous track relative to currently focused track = last selected track
		int numTracks = CSurf_NumTracks(false);
		// Backstop measure to protect against unreported track removal that was not captured in SetTrackListChange callback due to race condition
		if (g_trackInFocus > numTracks) { 
			g_trackInFocus = numTracks; 
		}
		if (((g_trackInFocus <= 1) && (value < 0)) ||
			((g_trackInFocus >= numTracks) && (value >0 ))) {
			return;
		}
		int id = g_trackInFocus + value;
		MediaTrack* track = CSurf_TrackFromID(id, false);
		int iSel = 1; // "Select"
		_clearAllSelectedTracks();
		GetSetMediaTrackInfo(track, "I_SELECTED", &iSel);
	}

	void _onBankSelect(signed char value) {
		// Manually switch the bank visible in Mixer View WITHOUT influencing track selection
		int newBankStart = this->_bankStart + (value * BANK_NUM_TRACKS);
		int numTracks = CSurf_NumTracks(false); 
		if ((newBankStart < 0) || (newBankStart > numTracks)) {
			return;
		}
		this->_bankStart = newBankStart;
		this->_allMixerUpdate();
	}

	void _onTrackMute(unsigned char numInBank) {
		int id = this->_bankStart + numInBank;
		if ((id == 0) || (id > CSurf_NumTracks(false))) {
			return;
		}
		MediaTrack* track = CSurf_TrackFromID(id, false);
		bool muted = *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr);
		CSurf_OnMuteChange(track, muted ? 0 : 1);
	}

	void _onTrackSolo(unsigned char numInBank) {		
		int id = this->_bankStart + numInBank;
		if ((id == 0) || (id > CSurf_NumTracks(false))) {
			return;
		}
		MediaTrack* track = CSurf_TrackFromID(id, false);
		int soloState = *(int*)GetSetMediaTrackInfo(track, "I_SOLO", nullptr);
		CSurf_OnSoloChange(track, soloState == 0 ? 1 : 0); // ToDo: Consider settings solo state to 2 (soloed in place)
	}

	void _onKnobVolumeChange(unsigned char command, signed char value) {
		int numInBank = command - CMD_KNOB_VOLUME0;
		double dvalue = static_cast<double>(value);
		MediaTrack* track = CSurf_TrackFromID(numInBank + this->_bankStart, false);
		if (!track) {
			return;
		}
		CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, dvalue * 0.007874, true), nullptr); 
		// scaling by dividing by 127 (0.007874)
		// ToDo: non linear behaviour especially in the lower volumes would be preferable. 
	}

	void _onKnobPanChange(unsigned char command, signed char value) {
		int numInBank = command - CMD_KNOB_PAN0;
		double dvalue = static_cast<double>(value);
		MediaTrack* track = CSurf_TrackFromID(numInBank + this->_bankStart, false);
		if (!track) {
			return;
		}
		CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, dvalue * 0.00098425, true), nullptr); 
		// scaling by dividing by 127*8 (0.00098425)
	}

	void _onSelTrackVolumeChange(signed char value) {
		double dvalue = static_cast<double>(value);
		MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
		if (!track) {
			return;
		}
		CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, dvalue * 0.007874, true), nullptr);
		// ToDo: Consider different scaling than with KnobVolumeChange
		// ToDo: non linear behaviour especially in the lower volumes would be preferable. 
	}

	void _onSelTrackPanChange(signed char value) {
		double dvalue = static_cast<double>(value);
		MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
		if (!track) {
			return;
		}
		CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, dvalue * 0.00098425, true), nullptr);
	}
	
	void _onSelTrackMute() {
		if (g_trackInFocus > 0) {
			MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
			if (!track) {
				return;
			}
			bool muted = *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr);
			CSurf_OnMuteChange(track, muted ? 0 : 1);
		}		
	}
	
	void _onSelTrackSolo() {
		if (g_trackInFocus > 0) {
			MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
			if (!track) {
				return;
			}
			int soloState = *(int*)GetSetMediaTrackInfo(track, "I_SOLO", nullptr);
			CSurf_OnSoloChange(track, soloState == 0 ? 1 : 0); // ToDo: Consider settings solo state to 2 (soloed in place)
		}		
	}

	// ToDo: toggle automation mode

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
