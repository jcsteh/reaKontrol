/*
 * ReaKontrol
 * FX Map header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2026 James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once

#include <map>
#include <string>
#include <vector>

class MediaTrack;

class FxMap {
	public:
	FxMap() = default;
	FxMap(MediaTrack* track, int fx);
	std::string getMapName() const;
	int getParamCount() const;
	int getReaperParam(int mapParam) const;
	int getMapParam(int reaperParam) const;
	std::string getParamName(int mapParam) const;
	double getParamMultiplier(int mapParam) const;
	std::string getSection(int mapParam) const;
	std::string getSectionsForPage(int mapParam) const;

	static std::string getMapNameFor(MediaTrack* track, int fx);
	static void generateMapFileForSelectedFx();

	private:
	std::string _mapName;
	MediaTrack* _track = nullptr;
	int _fx = -1;
	std::vector<int> _reaperParams;
	std::map<int, std::string> _paramNames;
	std::map<int, double> _paramMultipliers;
	std::map<int, int> _mapParams;
	std::map<int, std::string> _sections;
};
