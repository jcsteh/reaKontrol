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
#define REAPERAPI_WANT_CountTracks
#define REAPERAPI_WANT_CSurf_TrackToID
#define REAPERAPI_WANT_GetLastTouchedTrack
#define REAPERAPI_WANT_CSurf_OnPlay
#define REAPERAPI_WANT_ShowConsoleMsg
#define REAPERAPI_WANT_TrackFX_GetParamName
#define REAPERAPI_WANT_CSurf_OnStop
#define REAPERAPI_WANT_CSurf_OnRecord
#include <reaper/reaper_plugin.h>
#include <reaper/reaper_plugin_functions.h>
