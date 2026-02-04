/*
 * ReaKontrol
 * Support for MIDI protocol used by Komplete Kontrol S-series MK2/3, A-series
 * and M-Series
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2018-2026 James Teh
 * License: GNU General Public License version 2.0
 */

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>
#include <WDL/db2val.h>
#include <cstring>
#include "reaKontrol.h"

using namespace std;

const unsigned char MIDI_CC = 0xBF;
const unsigned char MIDI_SYSEX_BEGIN[] = {
	0xF0, 0x00, 0x21, 0x09, 0x00, 0x00, 0x44, 0x43, 0x01, 0x00};
const unsigned char MIDI_SYSEX_END = 0xF7;
const int BANK_NUM_SLOTS = 8;

const unsigned char CMD_HELLO = 0x01;
const unsigned char CMD_GOODBYE = 0x02;
const unsigned char CMD_SURFACE_CONFIG = 0x03;
const unsigned char CMD_BANK_MAPPING = 0x05;
const unsigned char CMD_USE_SYSEX_PARAM = 0x06;
const unsigned char CMD_PLAY = 0x10;
const unsigned char CMD_RESTART = 0x11;
const unsigned char CMD_REC = 0x12;
const unsigned char CMD_COUNT = 0x13;
const unsigned char CMD_STOP = 0x14;
const unsigned char CMD_CLEAR = 0x15;
const unsigned char CMD_LOOP = 0x16;
const unsigned char CMD_METRO = 0x17;
const unsigned char CMD_TEMPO = 0x18;
const unsigned char CMD_SET_TEMPO = 0x19;
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
const unsigned char CMD_NAV_PRESET = 0x36;
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
const unsigned char CMD_PLAY_CLIP = 0x60;
const unsigned char CMD_CHANGE_VOLUME = 0x64;
const unsigned char CMD_CHANGE_PAN = 0x65;
const unsigned char CMD_TOGGLE_MUTE = 0x66;
const unsigned char CMD_TOGGLE_SOLO = 0x67;
const unsigned char CMD_KNOB_PARAM0 = 0x70;
const unsigned char CMD_KNOB_PARAM1 = 0x71;
const unsigned char CMD_KNOB_PARAM2 = 0x72;
const unsigned char CMD_KNOB_PARAM3 = 0x73;
const unsigned char CMD_KNOB_PARAM4 = 0x74;
const unsigned char CMD_KNOB_PARAM5 = 0x75;
const unsigned char CMD_KNOB_PARAM6 = 0x76;
const unsigned char CMD_KNOB_PARAM7 = 0x77;
const unsigned char CMD_SELECT_PLUGIN = 0x70;
const unsigned char CMD_PLUGIN_NAMES = 0x71;
const unsigned char CMD_PARAM_NAME = 0x72;
const unsigned char CMD_PARAM_VALUE_TEXT = 0x73;
const unsigned char CMD_PARAM_PAGE = 0x74;
const unsigned char CMD_PARAM_SECTION = 0x75;
const unsigned char CMD_PRESET_NAME = 0x76;
const unsigned char CMD_PARAM_HIGH_RES = 0x7F;

const unsigned char TRTYPE_UNSPEC = 1;

const unsigned char PARAM_VIS_UNIPOLAR = 0;
const unsigned char PARAM_VIS_BIPOLAR = 1;
const unsigned char PARAM_VIS_SWITCH = 2;
const unsigned char PARAM_VIS_DISCRETE = 3;

const unsigned char PARAM_GROUP_VOLUME = 0;
const unsigned char PARAM_GROUP_PAN = 1;
const unsigned char PARAM_GROUP_PLUGIN = 2;

const double CC_PAN_SCALE_FACTOR = 127 * 8;
constexpr double TEN_NS_IN_SEC = 10e-9;

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
		log("sending hello");
		this->_sendCc(CMD_HELLO, 4);
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

	void SetSurfaceSelected(MediaTrack* track, bool selected) final {
		if (!selected) {
			return;
		}
		const bool wasAlreadySelected = this->_lastSelectedTrack == track;
		this->_lastSelectedTrack = track;
		int id = CSurf_TrackToID(track, false);
		int numInBank = id % BANK_NUM_SLOTS;
		int oldBankStart = this->_trackBankStart;
		this->_trackBankStart = id - numInBank;
		if (this->_trackBankStart != oldBankStart) {
			this->_onTrackBankChange();
		} else if (wasAlreadySelected && !this->_isUsingMixerForFx()) {
			// The track might have been renamed.
			const char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
			if (!name) {
				name = "";
			}
			this->_sendSysex(CMD_TRACK_NAME, 0, numInBank, name);
		}
		if (!this->_isUsingMixerForFx()) {
			this->_sendSysex(CMD_TRACK_SELECTED, 1, numInBank);
		}
		const string kkInstance = getKkInstanceName(track);
		this->_sendSysex(CMD_SEL_TRACK_PARAMS_CHANGED, 0, 0, kkInstance);
		this->_initFx();
		if (!this->_isUsingMixerForFx()) {
			int trackLights = 0;
			// 0 is the master track. We don't allow navigation to that.
			if (id > 1) {
				// Bit 0: previous
				trackLights |= 1;
			}
			// CSurf_TrackFromID treats 0 as the master, but CSurf_NumTracks doesn't
			// count the master, so the return value is the last track, not the count.
			if (id < CSurf_NumTracks(false)) {
				// Bit 1: next
				trackLights |= 1 << 1;
			}
			this->_sendCc(CMD_NAV_TRACKS, trackLights);
		}
	}

	void SetTrackListChange() final {
		// A track has been added or removed. Send updated bank info.
		this->_onTrackBankChange();
	}

	void SetSurfaceVolume(MediaTrack* track, double volume) final {
		if (this->_isUsingMixerForFx()) {
			return;
		}
		const int numInBank = this->_getNumInBank(track);
		if (numInBank != -1) {
			char volText[64];
			mkvolstr(volText, volume);
			this->_sendSysex(CMD_TRACK_VOLUME_TEXT, 0, numInBank, volText);
			this->_sendCc(CMD_KNOB_VOLUME0 + numInBank, volToCc(volume));
		}
	}

	void SetSurfacePan(MediaTrack* track, double pan) final {
		if (this->_isUsingMixerForFx()) {
			return;
		}
		const int numInBank = this->_getNumInBank(track);
		if (numInBank != -1) {
			char panText[64];
			mkpanstr(panText, pan);
			this->_sendSysex(CMD_TRACK_PAN_TEXT, 0, numInBank, panText);
			this->_sendCc(CMD_KNOB_PAN0 + numInBank, panToCc(pan));
		}
	}

	void SetSurfaceMute(MediaTrack *track, bool mute) final {
		if (this->_isUsingMixerForFx()) {
			return;
		}
		const int numInBank = this->_getNumInBank(track);
		if (numInBank != -1) {
			this->_sendSysex(CMD_TRACK_MUTED, mute ? 1 : 0, numInBank);
		}
	}

	void SetSurfaceSolo(MediaTrack *track, bool solo) final {
		if (this->_isUsingMixerForFx()) {
			return;
		}
		const int numInBank = this->_getNumInBank(track);
		if (numInBank != -1) {
			this->_sendSysex(CMD_TRACK_SOLOED, solo ? 1 : 0, numInBank);
		}
	}

	int Extended(int call, void* parm1, void* parm2, void* parm3) final {
		if (call == CSURF_EXT_SETFXPARAM) {
			if (this->_protocolVersion < 4 && !this->_isUsingMixerForFx()) {
				return 0;
			}
			auto track = (MediaTrack*)parm1;
			if (track != this->_lastSelectedTrack) {
				return 0;
			}
			int fx = *(int*)parm2 >> 16;
			if (fx != this->_selectedFx) {
				return 0;
			}
			int param = *(int*)parm2 & 0xFFFF;
			if (param < this->_fxBankStart || param >= this->_fxBankStart + BANK_NUM_SLOTS) {
				return 0;
			}
			double normVal = *(double*)parm3;
			const int numInBank = param - this->_fxBankStart;
			this->_fxParamValueChanged(param, numInBank, normVal);
		} else if (call == CSURF_EXT_SETFXCHANGE) {
			// FX were added, removed or reordered.
			auto track = (MediaTrack*)parm1;
			if (track == this->_lastSelectedTrack) {
				this->_initFx();
			}
		} else if (call == CSURF_EXT_SETBPMANDPLAYRATE) {
			if (this->_protocolVersion < 4 || !parm1) {
				return 0;
			}
			double bpm = *(double*)parm1;
			// Kontrol wants the value as an integer representing the duration of a
			// quarter note in multiples of 10ns.
			int kTempo = 60 / bpm / TEN_NS_IN_SEC;
			// It has to be serialised as 5 7-bit values in little endian order.
			unsigned char data[5];
			for (int b = 0; b < 5; ++b) {
				data[b] = (kTempo >> (b * 7)) & 0x7F;
			}
			this->_sendSysex(CMD_SET_TEMPO, 0, 0, string((char*)data, sizeof(data)));
		}
		return 0;
	}

	protected:
	void _onMidiEvent(MIDI_event_t* event) override {
		if (event->midi_message[0] == MIDI_SYSEX_BEGIN[0]) {
			const unsigned char* data = event->midi_message + sizeof(MIDI_SYSEX_BEGIN);
			const unsigned char command = *data;
			++data;
			const unsigned char value = *data;
			++data;
			const unsigned char index = *data;
			++data;
			// infoSize is the number of bytes of additional info. We subtract the
			// sysex prefix, plus 3 bytes for the command, value and index, plus an
			// additional byte for the sysex suffix.
			int infoSize = event->size - sizeof(MIDI_SYSEX_BEGIN) - 4;
			switch (command) {
				case CMD_SET_TEMPO: {
					// The value is an integer serialised as 5 7-bit values in little
					// endian order.
					int kTempo = 0;
					for (size_t b = 0; b < 5; ++b) {
						kTempo += data[b] << (b * 7);
					}
					// The integer represents the duration of a quarter note in multiples of
					// 10ns.
					const double bpm = 60 / (kTempo * TEN_NS_IN_SEC);
					CSurf_OnTempoChange(bpm);
					break;
				}
				case CMD_SELECT_PLUGIN:
					this->_selectFx(index, data, infoSize);
					break;
				case CMD_PARAM_HIGH_RES:
					// For protocol version 4 (S MK3), we receive this instead of
					// CMD_KNOB_* CC messages.
					this->_changeParamHighRes(value, index, data[0], data[1]);
					break;
				default:
					log("Unhandled MIDI sysex command " << showbase << hex
						<< (int)command << " " << (int)value << " " << (int)index);
					break;
			}
			return;
		}

		if (event->midi_message[0] != MIDI_CC) {
			return;
		}
		unsigned char& command = event->midi_message[1];
		unsigned char& value = event->midi_message[2];
		switch (command) {
			case CMD_HELLO:
				this->_protocolVersion = value;
				log("received hello ack, protocol version " << this->_protocolVersion);
				this->_sendCc(CMD_QUANTIZE, 1);
				this->_sendCc(CMD_TEMPO, 1);
				// Strictly speaking, we should only set bit 0 when we're not at the
				// start of the project, and bit 1 when we're not at the end of the
				// project/time selection/loop area. However, this would involve polling
				// the cursor position, which isn't ideal. For now, just always enable
				// both previous and next.
				this->_sendCc(CMD_NAV_CLIPS, 3);
				// Specify vertical track navigation.
				this->_sendSysex(CMD_SURFACE_CONFIG, 1, 0, "track_orientation");
				this->_onTrackBankChange();
				if (this->_protocolVersion >= 4) {
					// For S MK3, request sysex high resolution parameter adjustments.
					this->_sendCc(CMD_USE_SYSEX_PARAM, 1);
				}
				break;
			case CMD_BANK_MAPPING:
				this->_isBankNavForTracks = value == 0;
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
			case CMD_COUNT:
				this->_toggleCountIn();
				break;
			case CMD_STOP:
				CSurf_OnStop();
				break;
			case CMD_PLAY_CLIP:
				if (this->_protocolVersion < 4) {
					// Toggle the mode where we use the mixer for FX parameters. See
					// _isUsingMixerForFx.
					this->_isBankNavForTracks = !this->_isBankNavForTracks;
					if (this->_isBankNavForTracks) {
						this->_onTrackBankChange();
					} else {
						this->_fxBankChanged();
					}
				}
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
			case CMD_QUANTIZE:
				Main_OnCommand(42033, 0); // Track: Toggle MIDI input quantize for selected tracks
				break;
			case CMD_NAV_TRACKS:
				// Value is -1 or 1.
				if (this->_isUsingMixerForFx()) {
					this->_navigateFx(value == 1);
					break;
				}
				this->_onNavigateTracks(value == 1);
				break;
			case CMD_NAV_BANKS:
				// Value is -1 or 1.
				if (this->_isBankNavForTracks) {
					this->_onTrackBankSelect(convertSignedMidiValue(value));
				} else {
					this->_navigateFxBanks(value == 1);
				}
				break;
			case CMD_NAV_CLIPS:
				// Value is -1 or 1.
				if (this->_isUsingMixerForFx()) {
					this->_navigateFxBanks(value == 1);
					break;
				}
				Main_OnCommand(value == 1 ?
					40173 : // Markers: Go to next marker/project end
					40172, // Markers: Go to previous marker/project start
				0);
				break;
			case CMD_MOVE_TRANSPORT:
				// Value is -1 or 1.
				Main_OnCommand(value == 1 ?
					40647 : // View: Move cursor right to grid division
					40646, // View: Move cursor left to grid division
				0);
				break;
			case CMD_NAV_PRESET:
				TrackFX_NavigatePresets(this->_lastSelectedTrack, this->_selectedFx,
					convertSignedMidiValue(value));
				this->_fxPresetChanged();
				break;
			case CMD_TRACK_SELECTED:
				// Select a track from current bank in Mixer Mode with top row buttons
				if (MediaTrack* track = this->_getTrackFromNumInBank(value)) {
					SetOnlyTrackSelected(track);
				}
				break;
			case CMD_TRACK_MUTED:
				if (MediaTrack* track = this->_getTrackFromNumInBank(value)) {
					CSurf_SetSurfaceMute(track, CSurf_OnMuteChange(track, -1), nullptr);
				}
				break;
			case CMD_TRACK_SOLOED:
				if (MediaTrack* track = this->_getTrackFromNumInBank(value)) {
					CSurf_SetSurfaceSolo(track, CSurf_OnSoloChange(track, -1), nullptr);
				}
				break;
			case CMD_KNOB_VOLUME0:
			case CMD_KNOB_VOLUME1:
			case CMD_KNOB_VOLUME2:
			case CMD_KNOB_VOLUME3:
			case CMD_KNOB_VOLUME4:
			case CMD_KNOB_VOLUME5:
			case CMD_KNOB_VOLUME6:
			case CMD_KNOB_VOLUME7: {
				const int numInBank = command - CMD_KNOB_VOLUME0;
				if (this->_isUsingMixerForFx()) {
					this->_changeFxParamValue(numInBank, convertSignedMidiValue(value) / 127.0
						/ 8.0);
					break;
				}
				if (MediaTrack* track = this->_getTrackFromNumInBank(numInBank)) {
					CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track,
						convertSignedMidiValue(value) / 127.0, true), nullptr);
				}
				break;
			}
			case CMD_KNOB_PAN0:
			case CMD_KNOB_PAN1:
			case CMD_KNOB_PAN2:
			case CMD_KNOB_PAN3:
			case CMD_KNOB_PAN4:
			case CMD_KNOB_PAN5:
			case CMD_KNOB_PAN6:
			case CMD_KNOB_PAN7:
				if (MediaTrack* track = this->_getTrackFromNumInBank(command - CMD_KNOB_PAN0)) {
					CSurf_SetSurfacePan(track, CSurf_OnPanChange(track,
						convertSignedMidiValue(value) / CC_PAN_SCALE_FACTOR, true), nullptr);
				}
				break;
			case CMD_CHANGE_VOLUME:
				CSurf_SetSurfaceVolume(this->_lastSelectedTrack,
					CSurf_OnVolumeChange(this->_lastSelectedTrack,
						convertSignedMidiValue(value) / 127.0, true),
					nullptr);
				break;
			case CMD_CHANGE_PAN:
				CSurf_SetSurfacePan(this->_lastSelectedTrack,
					CSurf_OnPanChange(this->_lastSelectedTrack,
						convertSignedMidiValue(value) / CC_PAN_SCALE_FACTOR, true),
					nullptr);
				break;
			case CMD_TOGGLE_MUTE:
				CSurf_SetSurfaceMute(this->_lastSelectedTrack,
					CSurf_OnMuteChange(this->_lastSelectedTrack, -1), nullptr);
				break;
			case CMD_TOGGLE_SOLO:
				CSurf_SetSurfaceSolo(this->_lastSelectedTrack,
					CSurf_OnSoloChange(this->_lastSelectedTrack, -1), nullptr);
				break;
			default:
				log("Unhandled MIDI message " << showbase << hex
					<< (int)event->midi_message[0] << " "
					<< (int)event->midi_message[1] << " "
					<< (int)event->midi_message[2]);
				break;
		}
	}

	private:
	int _protocolVersion = 0;
	int _trackBankStart = 0;
	MediaTrack* _lastSelectedTrack = nullptr;
	// If true, bank navigation messages are for tracks. If false, they are for
	// plugin parameters.
	bool _isBankNavForTracks = true;
	int _selectedFx = 0;
	int _fxBankStart = 0;
	int _lastChangedFxParam = -1;
	double _lastChangedFxParamValue = 0;

	void _onTrackBankChange() {
		if (this->_isUsingMixerForFx()) {
			return;
		}
		int numInBank = 0;
		// bankEnd is exclusive; i.e. 1 beyond the last track in the bank.
		int bankEnd = this->_trackBankStart + BANK_NUM_SLOTS;
		// CSurf_TrackFromID treats 0 as the master, but CSurf_NumTracks doesn't
		// count the master, so add 1 to the count.
		const int numTracks = CSurf_NumTracks(false) + 1;
		if (bankEnd > numTracks) {
			bankEnd = numTracks;
			// Mark additional bank tracks as not available
			const int firstUnavailable = numTracks % BANK_NUM_SLOTS;
			for (int i = firstUnavailable; i < BANK_NUM_SLOTS; ++i) {
				this->_sendSysex(CMD_TRACK_AVAIL, 0, i);
			}
		}
		for (int id = this->_trackBankStart; id < bankEnd; ++id, ++numInBank) {
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
			this->_sendSysex(CMD_TRACK_PAN_TEXT, 0, numInBank, panText);
			const char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
			if (!name) {
				name = "";
			}
			this->_sendSysex(CMD_TRACK_NAME, 0, numInBank, name);
			// todo: level meters
			this->_sendCc(CMD_KNOB_VOLUME0 + numInBank, volToCc(volume));
			this->_sendCc(CMD_KNOB_PAN0 + numInBank, panToCc(pan));
		}
		int bankLights = 0;
		if (this->_trackBankStart > 0) {
			// Bit 0: previous
			bankLights |= 1;
		}
		if (bankEnd < numTracks) {
			// Bit 1: next
			bankLights |= 1 << 1;
		}
		this->_sendCc(CMD_NAV_BANKS, bankLights);
	}

	void _onTrackBankSelect(signed char value) {
		// Manually switch the bank visible in Mixer View WITHOUT influencing track selection
		int newBankStart = this->_trackBankStart + (value * BANK_NUM_SLOTS);
		int numTracks = CSurf_NumTracks(false); // If we ever want to show just MCP tracks in KK Mixer View (param) must be (true)
		if ((newBankStart < 0) || (newBankStart > numTracks)) {
			return;
		}
		this->_trackBankStart = newBankStart;
		this->_onTrackBankChange();
	}

	void _onNavigateTracks(bool next) {
		int id = CSurf_TrackToID(this->_lastSelectedTrack, false);
		if (next) {
			++id;
		} else {
			--id;
			if (id == 0) {
				return; // Don't allow navigation to the master track.
			}
		}
		MediaTrack* track = CSurf_TrackFromID(id, false);
		if (track) {
			SetOnlyTrackSelected(track);
		}
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

	int _getNumInBank(MediaTrack* track) {
		const int id = CSurf_TrackToID(track, false);
		if (this->_trackBankStart <= id && id < this->_trackBankStart + BANK_NUM_SLOTS) {
			return id % BANK_NUM_SLOTS;
		}
		return -1;
	}

	MediaTrack* _getTrackFromNumInBank(unsigned char numInBank) {
		int id = this->_trackBankStart + numInBank;
		return CSurf_TrackFromID(id, false);
	}

	void _initFx() {
		if (this->_protocolVersion >= 4) {
			this->_sendPluginNames();
		}
		this->_selectedFx = 0;
		this->_fxChanged();
	}

	void _sendPluginNames() {
		ostringstream s;

		// This is a recursive lambda. We pass the function to itself to work
		// around the compiler error: variable 'addContainer' declared with deduced
		// type 'auto' cannot appear in its own initializer.
		auto addContainer = [&s, this](int parentFx, auto&& addContainer) -> void {
			// Containers are represented using JSON.
			s << "{\"n\":\"";
			char name[100] = "";
			TrackFX_GetFXName(this->_lastSelectedTrack, parentFx, name, sizeof(name));
			// Escape any quote characters in the name.
			for (char* n = name; *n; ++n) {
				if (*n == '"') {
					s << "\\";
				}
				s << *n;
			}
			s << "\",\"c\":[";
			for (int c = 0; ; ++c) {
				int childFx = this->_getChildFx(parentFx, c);
				if (childFx == -1) {
					s << "]}";
					break;
				}
				if (c > 0) {
					s << ",";
				}
				addContainer(childFx, addContainer);
			}
		};

		int count = TrackFX_GetCount(this->_lastSelectedTrack);
		for (int f = 0; f < count; ++f) {
			if (s.tellp() > 0) {
				s << '\0';
			}
			if (this->_getChildFx(f, 0) != -1) {
				// This is a container.
				addContainer(f, addContainer);
			} else {
				char name[100] = "";
				TrackFX_GetFXName(this->_lastSelectedTrack, f, name, sizeof(name));
				s << name;
			}
		}
		this->_sendSysex(CMD_PLUGIN_NAMES, 0, 0, s.str());
	}

	void _fxChanged() {
		if (this->_protocolVersion >= 4) {
			this->_sendSelectPlugin();
		}
		this->_fxBankStart = 0;
		this->_fxBankChanged();
		if (this->_protocolVersion >= 4) {
			this->_fxPresetChanged();
		}
		this->_lastChangedFxParam = -1;
	}

	void _sendSelectPlugin() {
		// The REAPER FX index might be in a container. Convert it to a sequence of
		// 0-based positions in each nested container. We need to start with the
		// deepest FX and walk its ancestors.
		vector<char> subIndexes;
		int fx = this->_selectedFx;
		for (; ;) {
			int parentFx = this->_getParentFx(fx);
			if (parentFx == -1) {
				break;
			}
			subIndexes.push_back(this->_getChildFxPos(parentFx, fx));
			fx = parentFx;
		}
		// subIndexes is ordered from deepest to shallowest. Kontrol needs shallowest
		// to deepest, so we iterate it in reverse.
		this->_sendSysex(CMD_SELECT_PLUGIN, 0, fx,
			string(subIndexes.rbegin(), subIndexes.rend()));
	}

	bool _isFxParamToggle(int param) {
		bool isToggle = false;
		TrackFX_GetParameterStepSizes(this->_lastSelectedTrack, this->_selectedFx,
			param, nullptr, nullptr, nullptr, &isToggle);
		if (!isToggle) {
			isToggle =
				param == TrackFX_GetParamFromIdent(this->_lastSelectedTrack,
					this->_selectedFx, ":bypass") ||
				param == TrackFX_GetParamFromIdent(this->_lastSelectedTrack,
					this->_selectedFx, ":delta");
		}
		return isToggle;
	}

	void _fxBankChanged() {
		const int count = TrackFX_GetNumParams(this->_lastSelectedTrack,
			this->_selectedFx);
		int numPages = count / BANK_NUM_SLOTS;
		if (count % BANK_NUM_SLOTS) {
			// numPages is the number of full pages. There is a final, partial page.
			++numPages;
		}
		const int page = this->_fxBankStart / BANK_NUM_SLOTS;
		const bool isMixer = this->_isUsingMixerForFx();
		if (isMixer) {
			int lights = 0;
			if (page > 0) {
				// Bit 0: previous
				lights |= 1;
			}
			if (page + 1 < numPages) {
				// Bit 1: next
				lights |= 1 << 1;
			}
			this->_sendCc(CMD_NAV_BANKS, lights);
		} else {
			this->_sendSysex(CMD_PARAM_PAGE, numPages, page);
		}
		// bankEnd is exclusive; i.e. 1 beyond the last parameter in the bank.
		const int bankEnd = min(this->_fxBankStart + BANK_NUM_SLOTS, count);
		int numInBank = 0;
		for (int p = this->_fxBankStart; p < bankEnd; ++p, ++numInBank) {
			char name[100] = "";
			TrackFX_GetParamName(this->_lastSelectedTrack, this->_selectedFx, p, name,
				sizeof(name));
			if (isMixer) {
				this->_sendSysex(CMD_TRACK_AVAIL, TRTYPE_UNSPEC, numInBank);
				this->_sendSysex(CMD_TRACK_NAME, 0, numInBank, name);
				this->_sendSysex(CMD_TRACK_SELECTED, 0, numInBank);
				this->_sendSysex(CMD_TRACK_SOLOED, 0, numInBank);
				this->_sendSysex(CMD_TRACK_MUTED, 0, numInBank);
				this->_sendSysex(CMD_TRACK_ARMED, 0, numInBank);
			} else {
				const bool isToggle = this->_isFxParamToggle(p);
				this->_sendSysex(CMD_PARAM_NAME,
					isToggle ? PARAM_VIS_SWITCH : PARAM_VIS_UNIPOLAR, numInBank, name);
			}
			double val = TrackFX_GetParamNormalized(this->_lastSelectedTrack,
				this->_selectedFx, p);
			this->_fxParamValueChanged(p, numInBank, val);
		}
		// If there aren't sufficient parameters to fill the bank, clear the
		// remaining slots.
		for (; numInBank < BANK_NUM_SLOTS; ++numInBank) {
			if (isMixer) {
				this->_sendSysex(CMD_TRACK_AVAIL, 0, numInBank);
			} else {
				this->_sendSysex(CMD_PARAM_NAME, PARAM_VIS_UNIPOLAR, numInBank);
			}
		}
	}

	void _fxParamValueChanged(int param, int numInBank, double value) {
		const bool isMixer = this->_isUsingMixerForFx();
		unsigned char command = isMixer ? CMD_KNOB_VOLUME0 : CMD_KNOB_PARAM0;
		this->_sendCc(command + numInBank,127 * value); 
		char valText[100] = "";
		TrackFX_FormatParamValueNormalized(this->_lastSelectedTrack,
			this->_selectedFx, param, value, valText, sizeof(valText));
		command = isMixer ? CMD_TRACK_VOLUME_TEXT : CMD_PARAM_VALUE_TEXT;
		this->_sendSysex(command, 0, numInBank, valText);
	}

	void _navigateFxBanks(bool next) {
		int newBankStart = this->_fxBankStart;
		if (next) {
			newBankStart += BANK_NUM_SLOTS;
		} else {
			newBankStart -= BANK_NUM_SLOTS;
		}
		const int count = TrackFX_GetNumParams(this->_lastSelectedTrack,
			this->_selectedFx);
		if (newBankStart < 0 || newBankStart >= count) {
			return;
		}
		this->_fxBankStart = newBankStart;
		this->_fxBankChanged();
	}

	void _changeFxParamValue(int numInBank, double change) {
		int param = this->_fxBankStart + numInBank;
		double val ;
		if (param == this->_lastChangedFxParam) {
			// Some parameters snap to defined values when you set them. This means that
			// the next time we retrieve the value, it might be the same, in which case
			// we would always be adjusting relative to the same value, getting nowhere.
			// To deal with this, we cache the last parameter and value we set and adjust
			// relative to that cached value.
			val = this->_lastChangedFxParamValue;
		} else {
			val = TrackFX_GetParamNormalized(this->_lastSelectedTrack,
				this->_selectedFx, param);
		}
		val += change;
		val = clamp(val, 0.0, 1.0);
		TrackFX_SetParamNormalized(this->_lastSelectedTrack, this->_selectedFx, param,
			val);
		this->_lastChangedFxParam = param;
		this->_lastChangedFxParamValue = val;
	}

	void _fxPresetChanged() {
		int presetCount = 0;
		const int preset = TrackFX_GetPresetIndex(this->_lastSelectedTrack,
			this->_selectedFx, &presetCount);
		if (presetCount == 0) {
			this->_sendCc(CMD_NAV_PRESET, 0);
			return;
		}
		int presetLights = 0;
		if (preset > 0) {
			// Bit 0: previous
			presetLights |= 1;
		}
		if (preset < presetCount - 1) {
			// Bit 1: next
			presetLights |= 1 << 1;
		}
		this->_sendCc(CMD_NAV_PRESET, presetLights);
		char name[100] = "";
		TrackFX_GetPreset(this->_lastSelectedTrack, this->_selectedFx, name,
			sizeof(name));
		this->_sendSysex(CMD_PRESET_NAME, 0, 0, name);
	}

	int _getChildFx(int parentFx, int childNum) {
		ostringstream s;
		s << "container_item." << childNum;
		char val[12] = "";
		TrackFX_GetNamedConfigParm(this->_lastSelectedTrack, parentFx, s.str().c_str(),
			val, sizeof(val));
		return val[0] ? atoi(val) : -1;
	}

	void _selectFx(unsigned char topIndex, const unsigned char* subIndexes,
		int numSubIndexes
	) {
		// Convert the sequence of 0-based container positions to a REAPER FX index.
		int fx = topIndex;
		for (int depth = 0; depth < numSubIndexes; ++depth) {
			fx = this->_getChildFx(fx, subIndexes[depth]);
			if (fx == -1) {
				return;
			}
		}
		this->_selectedFx = fx;
		this->_fxChanged();
	}

	int _getParentFx(int childFx) {
		char val[12] = "";
		TrackFX_GetNamedConfigParm(this->_lastSelectedTrack, childFx,
			"parent_container", val, sizeof(val));
		return val[0] ? atoi(val) : -1;
	}

	void _changeParamHighRes(unsigned char group, unsigned char index,
		const unsigned char lsb, const unsigned char msb
	) {
		double change = lsb + (msb << 7);
		if (change > 8192) {
			// Convert to signed value.
			change -= 16384;
		}
		change /= 8191.0;
		switch (group) {
			case PARAM_GROUP_VOLUME:
				if (MediaTrack* track = this->_getTrackFromNumInBank(index)) {
					CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, change, true),
						nullptr);
				}
				break;
			case PARAM_GROUP_PAN:
				if (MediaTrack* track = this->_getTrackFromNumInBank(index)) {
					CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, change, true),
						nullptr);
				}
				break;
			case PARAM_GROUP_PLUGIN:
				this->_changeFxParamValue(index, change);
				break;
		}
	}

	void _toggleCountIn() {
		int size = 0;
		const int index = projectconfig_var_getoffs("projmetroen", &size);
		int& metro = *(int*)projectconfig_var_addr(nullptr, index);
		if (metro & 16) {
			// Count-in before recording is enabled. Disable it.
			metro &= ~16;
			this->_sendCc(CMD_COUNT, 0);
		} else {
			Main_OnCommand(41745, 0); // Options: Enable metronome
			metro |= 16;
			this->_sendCc(CMD_COUNT, 1);
		}
	}

	// Anything earlier than S MK3 doesn't natively support non-NKS FX
	// parameters. We fudge this by having a mode where we abuse the mixer for FX
	// parameters. We expose each parameter as a "track", with the volume
	// adjusting the parameter's value.
	bool _isUsingMixerForFx() {
		return this->_protocolVersion < 4 && !this->_isBankNavForTracks;
	}

	// Only used when using the mixer for FX (not on MK3).
	void _navigateFx(bool next) {
		const int topCount = TrackFX_GetCount(this->_lastSelectedTrack);
		const bool isTopFx = this->_selectedFx < topCount;
		const int parentFx = isTopFx ? -1 : this->_getParentFx(this->_selectedFx);
		if (!isTopFx && parentFx == -1) {
			return;
		}
		if (next) {
			// If this is a container, move to its first child.
			const int childFx = this->_getChildFx(this->_selectedFx, 0);
			if (childFx != -1) {
				this->_selectedFx = childFx;
				this->_fxChanged();
				return;
			}
			// Otherwise, move to the next sibling.
			if (isTopFx) {
				if (this->_selectedFx + 1 < topCount) {
					++this->_selectedFx;
					this->_fxChanged();
				}
				return;
			}
			// This FX is inside a container.
			int ancestorFx = parentFx;
			int fx = this->_selectedFx;
			for (; ;) {
				// Get the next sibling.
				if (ancestorFx == -1) {
					// This is a top level FX.
					if (fx + 1 < topCount) {
						this->_selectedFx = fx + 1;
						this->_fxChanged();
					}
					return;
				}
				const int pos = this->_getChildFxPos(ancestorFx, fx);
				const int siblingFx = this->_getChildFx(ancestorFx, pos + 1);
				if (siblingFx != -1) {
					this->_selectedFx = siblingFx;
					this->_fxChanged();
					return;
				}
				// There's no next sibling, so walk to the parent and try from there.
				fx = ancestorFx;
				ancestorFx = this->_getParentFx(ancestorFx);
			}
			return;
		}
		// We are navigating previous. Get the previous sibling, if any.
		int siblingFx = -1;
		if (isTopFx) {
			if (this->_selectedFx == 0) {
				return;
			}
			siblingFx = this->_selectedFx - 1;
		} else {
			const int pos = this->_getChildFxPos(parentFx, this->_selectedFx);
			if (pos != 0) {
				siblingFx = this->_getChildFx(parentFx, pos - 1);
			}
		}
		if (siblingFx != -1) {
			// Walk to the sibling's deepest, last descendant. If there isn't one,
			// choose the sibling itself.
			this->_selectedFx = siblingFx;
			for (; ;) {
				const int childCount = this->_getFxChildCount(this->_selectedFx);
				if (childCount == 0) {
					break;
				}
				const int childFx = this->_getChildFx(this->_selectedFx, childCount - 1);
				if (childFx == -1) {
					break;
				}
				this->_selectedFx = childFx;
			}
			this->_fxChanged();
			return;
		}
		// There is no previous sibling. Walk to the parent.
		this->_selectedFx = parentFx;
		this->_fxChanged();
	}

	int _getChildFxPos(int parentFx, int childFx) {
		const int child0 = this->_getChildFx(parentFx, 0);
		if (child0 == childFx) {
			return 0;
		}
		const int child1 = this->_getChildFx(parentFx, 1);
		// The delta between child0 and child1 is the multiplier for children in
		// this container.
		const int multiplier = child1 - child0;
		return (childFx - child0) / multiplier;
	}

	int _getFxChildCount(int parentFx) {
		char val[12] = "";
		TrackFX_GetNamedConfigParm(this->_lastSelectedTrack, parentFx,
			"container_count", val, sizeof(val));
		return val[0] ? atoi(val) : 0;
	}
};

IReaperControlSurface* createNiMidiSurface(int inDev, int outDev) {
	return new NiMidiSurface(inDev, outDev);
}
