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
#include <utility>
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

	static bool isNamedConfigParam(int reaperParam) {
		return reaperParam <= -2;
	}
	bool isNamedConfigParamToggle(int reaperParam) const;
	double getNamedConfigParamNormalized(int reaperParam) const;
	std::string getNamedConfigParamValueText(int reaperParam, double normValue) const;
	bool setNamedConfigParamNormalized(int reaperParam, double normValue,
		double* outSnappedNorm = nullptr) const;

	static std::string getMapNameFor(MediaTrack* track, int fx);
	static void generateMapFileForSelectedFx();

	private:
	// The possible values for an FX named config param. The first string is the
	// display name. The second is the value string to pass to the REAPER API.
	using NamedConfigParamValues = std::vector<std::pair<const char*, const char*>>;

	struct NamedConfigParam {
		std::string displayName;
		std::string configName;
		const NamedConfigParamValues* values = nullptr;
	};

	static std::vector<NamedConfigParam> _getNamedConfigParamsFor(
		MediaTrack* track, int fx);
	const NamedConfigParam* _getNamedConfigParam(int reaperParam) const;
	int _getNamedConfigReaperParam(const std::string& configName) const;

	std::string _mapName;
	MediaTrack* _track = nullptr;
	int _fx = -1;
	std::vector<NamedConfigParam> _namedConfigParams;
	std::vector<int> _reaperParams;
	std::map<int, std::string> _paramNames;
	std::map<int, double> _paramMultipliers;
	std::map<int, int> _mapParams;
	std::map<int, std::string> _sections;
};
