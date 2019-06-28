/*
 * ReaKontrol
 * Main header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2018-2019 James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once

#include <windows.h>

// ToDo: Cleanup REAPERAPI WANT list
#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_GetNumMIDIInputs
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
#define REAPERAPI_WANT_CSurf_OnTrackSelection
#define REAPERAPI_WANT_GetLastTouchedTrack
#define REAPERAPI_WANT_CSurf_OnPlay
#define REAPERAPI_WANT_ShowConsoleMsg
#define REAPERAPI_WANT_TrackFX_GetCount
#define REAPERAPI_WANT_TrackFX_GetFXName
#define REAPERAPI_WANT_TrackFX_GetParamName
#define REAPERAPI_WANT_CSurf_GoStart
#define REAPERAPI_WANT_CSurf_OnStop
#define REAPERAPI_WANT_CSurf_OnRecord
#define REAPERAPI_WANT_Main_OnCommand
#define REAPERAPI_WANT_CSurf_ScrubAmt
#define REAPERAPI_WANT_GetSetMediaTrackInfo
#define REAPERAPI_WANT_CSurf_SetTrackListChange
#define REAPERAPI_WANT_CSurf_SetSurfaceVolume
#define REAPERAPI_WANT_CSurf_SetSurfacePan
#define REAPERAPI_WANT_CSurf_SetPlayState
#define REAPERAPI_WANT_CSurf_SetRepeatState
#define REAPERAPI_WANT_CSurf_SetSurfaceMute
#define REAPERAPI_WANT_CSurf_SetSurfaceSolo
#define REAPERAPI_WANT_CSurf_SetSurfaceRecArm
#define REAPERAPI_WANT_CSurf_OnVolumeChange
#define REAPERAPI_WANT_CSurf_OnPanChange
#define REAPERAPI_WANT_CSurf_OnMuteChange
#define REAPERAPI_WANT_CSurf_OnSoloChange
#define REAPERAPI_WANT_GetPlayState
#define REAPERAPI_WANT_GetGlobalAutomationOverride
#define REAPERAPI_WANT_SetGlobalAutomationOverride
#define REAPERAPI_WANT_Track_GetPeakInfo
#define REAPERAPI_WANT_mkvolstr
#define REAPERAPI_WANT_mkpanstr
#define REAPERAPI_WANT_get_config_var
#define REAPERAPI_WANT_projectconfig_var_getoffs
#define REAPERAPI_WANT_projectconfig_var_addr
#define REAPERAPI_WANT_EnumProjects
#define REAPERAPI_WANT_SetMixerScroll
#define REAPERAPI_WANT_GetTrackStateChunk


#include <reaper/reaper_plugin.h>
#include <reaper/reaper_plugin_functions.h>
#include <reaper/WDL/db2val.h>

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
