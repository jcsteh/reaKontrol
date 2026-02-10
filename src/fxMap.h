/*
 * ReaKontrol
 * FX Map header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2026 James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once

class MediaTrack;

class FxMap {
	public:
	FxMap() = default;
	FxMap(MediaTrack* track, int fx);
	int getParamCount() const;
	int getReaperParam(int mapParam) const;
	int getMapParam(int reaperParam) const;

	private:
	MediaTrack* _track = nullptr;
	int _fx = -1;
};
