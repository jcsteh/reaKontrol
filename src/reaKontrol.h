/*
 * ReaKontrol
 * Main header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2018-2019 James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once
#define LOGGING

#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_GetNumMIDIInputs
#define REAPERAPI_WANT_GetMIDIInputName
#define REAPERAPI_WANT_GetNumMIDIOutputs
#define REAPERAPI_WANT_GetMIDIOutputName
#define REAPERAPI_WANT_CreateMIDIInput
#define REAPERAPI_WANT_CreateMIDIOutput
#define REAPERAPI_WANT_GetNumTracks
#define REAPERAPI_WANT_CSurf_NumTracks
#define REAPERAPI_WANT_CSurf_TrackToID
#define REAPERAPI_WANT_CSurf_TrackFromID
#define REAPERAPI_WANT_CSurf_OnPlay
#ifdef LOGGING
#define REAPERAPI_WANT_ShowConsoleMsg
#endif
#define REAPERAPI_WANT_TrackFX_GetCount
#define REAPERAPI_WANT_TrackFX_GetFXName
#define REAPERAPI_WANT_TrackFX_GetParamName
#define REAPERAPI_WANT_CSurf_GoStart
#define REAPERAPI_WANT_CSurf_OnStop
#define REAPERAPI_WANT_CSurf_OnRecord
#define REAPERAPI_WANT_Main_OnCommand
#define REAPERAPI_WANT_GetSetMediaTrackInfo
#define REAPERAPI_WANT_CSurf_SetSurfaceVolume
#define REAPERAPI_WANT_CSurf_SetSurfacePan
#define REAPERAPI_WANT_CSurf_OnVolumeChange
#define REAPERAPI_WANT_CSurf_OnPanChange
#define REAPERAPI_WANT_GetPlayState
#define REAPERAPI_WANT_plugin_register
#define REAPERAPI_WANT_DB2SLIDER
#define REAPERAPI_WANT_mkvolstr
#define REAPERAPI_WANT_mkpanstr
#define REAPERAPI_WANT_SetOnlyTrackSelected
#define REAPERAPI_WANT_CSurf_SetSurfaceMute
#define REAPERAPI_WANT_CSurf_OnMuteChange
#define REAPERAPI_WANT_CSurf_SetSurfaceSolo
#define REAPERAPI_WANT_CSurf_OnSoloChange
#define REAPERAPI_WANT_TrackFX_GetCount
#define REAPERAPI_WANT_TrackFX_GetFXName
#define REAPERAPI_WANT_TrackFX_GetNumParams
#define REAPERAPI_WANT_TrackFX_GetParamName
#define REAPERAPI_WANT_TrackFX_GetParamNormalized
#define REAPERAPI_WANT_TrackFX_FormatParamValueNormalized
#define REAPERAPI_WANT_TrackFX_SetParamNormalized
#define REAPERAPI_WANT_TrackFX_GetParameterStepSizes
#define REAPERAPI_WANT_TrackFX_GetPresetIndex
#define REAPERAPI_WANT_TrackFX_GetPreset
#define REAPERAPI_WANT_TrackFX_NavigatePresets
#define REAPERAPI_WANT_TrackFX_GetNamedConfigParm
#define REAPERAPI_WANT_TrackFX_GetParamFromIdent
#define REAPERAPI_WANT_CSurf_OnTempoChange
#include <reaper/reaper_plugin.h>
#include <reaper/reaper_plugin_functions.h>

#ifdef LOGGING
# include <sstream>
# define log(msg) { \
	ostringstream s; \
	s << "reaKontrol " << msg << endl; \
	ShowConsoleMsg(s.str().c_str()); \
}
#else
# define log(msg)
#endif

const std::string getKkInstanceName(MediaTrack* track, bool stripPrefix=false);

class BaseSurface: public IReaperControlSurface {
	public:
	BaseSurface(int inDev, int outDev);
	virtual ~BaseSurface();
	virtual const char* GetConfigString() override {
		return "";
	}
	virtual void Run() override;

	protected:
	midi_Input* _midiIn = nullptr;
	midi_Output* _midiOut = nullptr;
	virtual void _onMidiEvent(MIDI_event_t* event) = 0;
};

IReaperControlSurface* createNiMidiSurface(int inDev, int outDev);
IReaperControlSurface* createMcuSurface(int inDev, int outDev);
