/*
 * ReaKontrol
 * Main header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2018 James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once

#include <windows.h>

#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_GetNumMIDIInputs
#define REAPERAPI_WANT_GetNumMIDIInputs
#define REAPERAPI_WANT_GetMIDIInputName
#define REAPERAPI_WANT_GetNumMIDIOutputs
#define REAPERAPI_WANT_GetMIDIOutputName
#define REAPERAPI_WANT_CreateMIDIInput
#define REAPERAPI_WANT_CreateMIDIOutput
#define REAPERAPI_WANT_CountTracks
#define REAPERAPI_WANT_CSurf_TrackToID
#define REAPERAPI_WANT_GetLastTouchedTrack
#define REAPERAPI_WANT_CSurf_OnPlay
#define REAPERAPI_WANT_ShowConsoleMsg
#define REAPERAPI_WANT_TrackFX_GetCount
#define REAPERAPI_WANT_TrackFX_GetFXName
#define REAPERAPI_WANT_TrackFX_GetParamName
#define REAPERAPI_WANT_CSurf_OnStop
#define REAPERAPI_WANT_CSurf_OnRecord
#define REAPERAPI_WANT_Main_OnCommand
#define REAPERAPI_WANT_CSurf_ScrubAmt
#include <reaper/reaper_plugin.h>
#include <reaper/reaper_plugin_functions.h>
