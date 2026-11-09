/*
 * ReaKontrol
 * FX Map code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2026 James Teh
 * License: GNU General Public License version 2.0
 */

#ifdef _WIN32
#include <WDL/win32_utf8.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <sstream>
#include "fxMap.h"
#include "reaKontrol.h"

// Strip leading and trailing space, as well as comments.
static const std::regex RE_STRIP(R"(^\s+|\s*#.*$|\s+$)");
// The map name, ending with a colon.
static const std::regex RE_MAP_NAME("(.*):");
// A parameter number or named config parameter identifier, optionally followed
// by space and a scaling factor (/n or *n), optionally followed by space and a
// name.
static const std::regex RE_PARAM(R"((\d+|[A-Za-z_]\w*)(?:\s+([/*])(\d+))?(?:\s+(.+))?)");
// A section name in square brackets.
static const std::regex RE_SECTION(R"(\[(.+)\])");

// The generateMapFileForSelectedFx action can't access the FxMap instance,
// so we cache the last selected FX here.
static MediaTrack* lastTrack = nullptr;
static int lastFx = -1;

static std::filesystem::path getFxMapDir() {
	std::filesystem::path path(std::u8string_view((char8_t*)GetResourcePath()));
	path /= "reaKontrol";
	path /= "fxMaps";
	return path;
}

static std::filesystem::path getFxMapFileName(MediaTrack* track, int fx) {
	char name[100] = "";
	TrackFX_GetFXName(track, fx, name, sizeof(name));
	if (!name[0]) {
		// This will happen when there are no FX on this track.
		return "";
	}
	std::filesystem::path path = getFxMapDir();
	path /= "";
	// std::filesystem::path treats appended characters as complete Unicode code
	// points, even if they're char8_t. This breaks multi-byte UTF-8 characters.
	// Thus, we need to filter the string first and then append it to the path.
	std::u8string fileName;
	for (char* c = name; *c; ++c) {
		if (*c == '/' || *c == '\\' || *c == ':') {
			continue;
		}
		fileName += (char8_t)*c;
	}
	path += fileName;
	path += ".rkfm";
	return path;
}

static std::string getLine(std::ifstream& input) {
	std::string line;
	while (std::getline(input, line)) {
		line = std::regex_replace(line, RE_STRIP, "");
		if (!line.empty()) {
			// Not a blank line, only space or a comment.
			return line;
		}
	}
	return "";
}

FxMap::FxMap(MediaTrack* track, int fx) : _track(track), _fx(fx),
		_namedConfigParams(_getNamedConfigParamsFor(track, fx)) {
	lastTrack = track;
	lastFx = fx;
	const std::filesystem::path path = getFxMapFileName(track, fx);
	if (path.empty()) {
		return;
	}
	std::ifstream input(path);
	if (!input) {
		log("no FX map " << path);
		return;
	}
	log("loading FX map " << path);
	for (std::string line = getLine(input); !line.empty(); line = getLine(input)) {
		std::smatch m;
		std::regex_search(line, m, RE_MAP_NAME);
		if (!m.empty()) {
			if (!this->_mapName.empty()) {
				log("map name specified more than once, ignoring: " << line);
				continue;
			}
			this->_mapName = m.str(1);
			log("map name: " << this->_mapName);
			continue;
		}
		std::regex_search(line, m, RE_PARAM);
		if (!m.empty()) {
			const std::string paramId = m.str(1);
			int rp = -1;
			if (std::isdigit((unsigned char)paramId[0])) {
				rp = std::atoi(paramId.c_str());
			} else {
				rp = this->_getNamedConfigReaperParam(paramId);
				if (rp == -1) {
					log("invalid FX map line: " << line);
					continue;
				}
			}
			const int mp = this->_reaperParams.size();
			this->_reaperParams.push_back(rp);
			this->_mapParams.insert({rp, mp});
			const std::string scaleType = m.str(2);
			if (!scaleType.empty()) {
				const int factor = std::atoi(m.str(3).c_str());
				this->_paramMultipliers.insert({mp,
					scaleType == "/" ? (1.0 / factor) : factor});
			}
			const std::string paramName = m.str(4);
			if (!paramName.empty()) {
				this->_paramNames.insert({mp, paramName});
			}
			continue;
		}
		if (line == "---") {
			// A page break has been requested. Any remaining slots on this page
			// should be empty.
			while (this->_reaperParams.size() % BANK_NUM_SLOTS != 0) {
				this->_reaperParams.push_back(-1);
			}
			continue;
		}
		std::regex_search(line, m, RE_SECTION);
		if (!m.empty()) {
			this->_sections.insert({this->_reaperParams.size(), m.str(1)});
			continue;
		}
		log("invalid FX map line: " << line);
	}
	log("loaded " << this->_mapParams.size() << " params from FX map");
}

std::vector<FxMap::NamedConfigParam> FxMap::_getNamedConfigParamsFor(
	MediaTrack* track, int fx
) {
	static const NamedConfigParamValues TOGGLE_VALUES = {
		{"off", "0"},
		{"on", "1"},
	};
	static const NamedConfigParamValues REAEQ_BAND_TYPE_VALUES = {
		{"low shelf", "0"},
		{"high shelf", "1"},
		{"band", "8"},
		{"low pass", "3"},
		{"high pass", "4"},
		{"all pass", "5"},
		{"notch", "6"},
		{"band pass", "7"},
		{"parallel band pass", "10"},
		{"band (alt)", "9"},
		{"band (alt 2)", "2"},
	};

	std::vector<NamedConfigParam> params;
	if (!track || fx < 0) {
		return params;
	}
	char fxName[100] = "";
	TrackFX_GetFXName(track, fx, fxName, sizeof(fxName));
	if (std::strcmp(fxName, "VST: ReaEQ (Cockos)") == 0) {
		for (int band = 0; ; ++band) {
			std::ostringstream name;
			name << "BANDENABLED" << band;
			char type[2] = "";
			if (!TrackFX_GetNamedConfigParm(track, fx, name.str().c_str(),
					type, sizeof(type))) {
				// This band doesn't exist.
				break;
			}
			std::ostringstream dispName;
			dispName << "Band " << (band + 1) << " enable";
			params.push_back({dispName.str(), name.str(), &TOGGLE_VALUES});
			name.str("");
			name << "BANDTYPE" << band;
			dispName.str("");
			dispName << "Band " << (band + 1) << " type";
			params.push_back({dispName.str(), name.str(), &REAEQ_BAND_TYPE_VALUES});
		}
	}
	return params;
}

int FxMap::_getNamedConfigReaperParam(const std::string& configName) const {
	for (size_t i = 0; i < this->_namedConfigParams.size(); ++i) {
		if (this->_namedConfigParams[i].configName == configName) {
			return -2 - (int)i;
		}
	}
	return -1;
}

std::string FxMap::getMapName() const {
	if (this->_mapName.empty()) {
		char name[100];
		TrackFX_GetFXName(this->_track, this->_fx, name, sizeof(name));
		return name;
	}
	return this->_mapName;
}

int FxMap::getParamCount() const {
	if (this->_reaperParams.empty()) {
		return (int)this->_namedConfigParams.size() +
			TrackFX_GetNumParams(this->_track, this->_fx);
	}
	return this->_reaperParams.size();
}

int FxMap::getReaperParam(int mapParam) const {
	if (this->_reaperParams.empty()) {
		const int namedCount = (int)this->_namedConfigParams.size();
		if (mapParam < namedCount) {
			return -2 - mapParam;
		}
		return mapParam - namedCount;
	}
	return this->_reaperParams[mapParam];
}

int FxMap::getMapParam(int reaperParam) const {
	if (this->_reaperParams.empty()) {
		if (isNamedConfigParam(reaperParam)) {
			return -2 - reaperParam;
		}
		return (int)this->_namedConfigParams.size() + reaperParam;
	}
	auto it = this->_mapParams.find(reaperParam);
	if (it == this->_mapParams.end()) {
		return -1;
	}
	return it->second;
}

std::string FxMap::getParamName(int mapParam) const {
	auto it = this->_paramNames.find(mapParam);
	if (it == this->_paramNames.end()) {
		const int rp = this->getReaperParam(mapParam);
		if (const auto* spec = this->_getNamedConfigParam(rp)) {
			return spec->displayName;
		}
		char name[100] = "";
		TrackFX_GetParamName(this->_track, this->_fx, rp, name, sizeof(name));
		return name;
	}
	return it->second;
}

const FxMap::NamedConfigParam* FxMap::_getNamedConfigParam(int reaperParam) const {
	if (!isNamedConfigParam(reaperParam)) {
		return nullptr;
	}
	const int index = -2 - reaperParam;
	if (index < 0 || index >= (int)this->_namedConfigParams.size()) {
		return nullptr;
	}
	return &this->_namedConfigParams[index];
}

bool FxMap::isNamedConfigParamToggle(int reaperParam) const {
	const auto* spec = this->_getNamedConfigParam(reaperParam);
	return spec && spec->values && spec->values->size() == 2;
}

double FxMap::getNamedConfigParamNormalized(int reaperParam) const {
	const auto* spec = this->_getNamedConfigParam(reaperParam);
	if (!spec || !spec->values || spec->values->size() < 2) {
		return 0.0;
	}
	char valueStr[50] = "";
	if (!TrackFX_GetNamedConfigParm(this->_track, this->_fx,
			spec->configName.c_str(), valueStr, sizeof(valueStr)) || !valueStr[0]) {
		return 0.0;
	}
	for (size_t v = 0; v < spec->values->size(); ++v) {
		if (std::strcmp(valueStr, (*spec->values)[v].second) == 0) {
			return (double)v / (double)(spec->values->size() - 1);
		}
	}
	return 0.0;
}

std::string FxMap::getNamedConfigParamValueText(int reaperParam, double normValue) const {
	const auto* spec = this->_getNamedConfigParam(reaperParam);
	if (!spec || !spec->values || spec->values->empty()) {
		return "";
	}
	const int maxIndex = (int)spec->values->size() - 1;
	int index = (int)std::round(normValue * maxIndex);
	index = std::clamp(index, 0, maxIndex);
	return (*spec->values)[index].first;
}

bool FxMap::setNamedConfigParamNormalized(int reaperParam, double normValue,
	double* outSnappedNorm
) const {
	const auto* spec = this->_getNamedConfigParam(reaperParam);
	if (!spec || !spec->values || spec->values->empty()) {
		return false;
	}
	const int maxIndex = (int)spec->values->size() - 1;
	int index = (int)std::round(normValue * maxIndex);
	index = std::clamp(index, 0, maxIndex);
	if (outSnappedNorm) {
		*outSnappedNorm = maxIndex > 0 ? ((double)index / (double)maxIndex) : 0.0;
	}
	const char* valueStr = (*spec->values)[index].second;
	char curValueStr[50] = "";
	if (TrackFX_GetNamedConfigParm(this->_track, this->_fx,
			spec->configName.c_str(), curValueStr, sizeof(curValueStr)) &&
			std::strcmp(curValueStr, valueStr) == 0) {
		return false;
	}
	// ReaEQ returns false from TrackFX_SetNamedConfigParm for BANDENABLED and
	// BANDTYPE even when the change succeeds.
	TrackFX_SetNamedConfigParm(this->_track, this->_fx,
		spec->configName.c_str(), valueStr);
	return true;
}

double FxMap::getParamMultiplier(int mapParam) const {
	auto it = this->_paramMultipliers.find(mapParam);
	if (it == this->_paramMultipliers.end()) {
		return 1.0;
	}
	return it->second;
}

std::string FxMap::getSection(int mapParam) const {
	auto it = this->_sections.find(mapParam);
	if (it == this->_sections.end()) {
		return "";
	}
	return it->second;
}

std::string FxMap::getSectionsForPage(int mapParam) const {
	std::ostringstream s;
	int bankEnd = mapParam + BANK_NUM_SLOTS;
	if (bankEnd > this->_reaperParams.size()) {
		bankEnd = this->_reaperParams.size();
	}
	for (; mapParam < bankEnd; ++mapParam) {
		std::string section = this->getSection(mapParam);
		if (!section.empty()) {
			if (s.tellp() > 0) {
				s << ", ";
			}
			s << section;
		}
	}
	return s.str();
}

std::string FxMap::getMapNameFor(MediaTrack* track, int fx) {
	auto getOrigName = [&]() -> std::string {
		char name[100] = "";
		TrackFX_GetFXName(track, fx, name, sizeof(name));
		return name;
	};
	const std::filesystem::path path = getFxMapFileName(track, fx);
	if (path.empty()) {
		return getOrigName();
	}
	std::ifstream input(path);
	if (!input) {
		return getOrigName();
	}
	for (std::string line = getLine(input); !line.empty(); line = getLine(input)) {
		std::smatch m;
		std::regex_search(line, m, RE_MAP_NAME);
		if (m.empty()) {
			// The map name must be the first non-comment, non-blank line. If we hit
			// anything else, there's no map name, so don't process any further.
			break;
		}
		return m.str(1);
	}
	return getOrigName();
}

void FxMap::generateMapFileForSelectedFx() {
	const std::filesystem::path fn = getFxMapFileName(lastTrack, lastFx);
	if (fn.empty()) {
		// No selected FX.
		return;
	}
	const std::filesystem::path dir = getFxMapDir();
	std::filesystem::create_directories(dir);
	std::ofstream output(fn);
	if (!output) {
		return;
	}
	for (const auto& spec : _getNamedConfigParamsFor(lastTrack, lastFx)) {
		output << spec.configName << " # " << spec.displayName << std::endl;
	}
	const int count = TrackFX_GetNumParams(lastTrack, lastFx);
	for (int p = 0; p < count; ++p) {
		char name[100] = "";
		TrackFX_GetParamName(lastTrack, lastFx, p, name, sizeof(name));
		output << p << " # " << name << std::endl;
	}
	output.close();
	// Locate the file in Explorer/Finder.
	std::u8string params(u8"/select,");
	params += fn.u8string();
	ShellExecute(nullptr, "open", "explorer.exe", (const char*)params.c_str(),
		nullptr, SW_SHOW);
}
