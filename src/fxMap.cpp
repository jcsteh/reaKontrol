/*
 * ReaKontrol
 * FX Map code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2026 James Teh
 * License: GNU General Public License version 2.0
 */

#ifdef _WIN32
// Must be defined before any C++ STL header is included.
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <codecvt>
#endif

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include "fxMap.h"
#include "reaKontrol.h"

static const std::regex RE_LINE(
	// Ignore any space at the start of a line.
	"^\\s*"
	"(?:"
	// Group 1: the line may be the map name, ending with a colon.
	"([^#]+):"
	// Groups 2 and 3: or a parameter number, optionally followed by space and
	// a name.
	"|(\\d+)(?:\\s+([^#]+))?"
	// Group 4: or a page break indicator.
	"|(---)"
	// Group 5: or a section name in square brackets.
	"|\\[([^#]+)\\]"
	")?"
	// This may be followed by optional space and an optional comment starting
	// with "#".
	"\\s*(?:#.*)?$"
);

// The generateMapFileForSelectedFx action can't access the FxMap instance,
// so we cache the last selected FX here.
static MediaTrack* lastTrack = nullptr;
static int lastFx = -1;

static std::string getFxMapDir() {
	std::string path(GetResourcePath());
	path += "/reaKontrol/fxMaps";
	return path;
}

static std::string getFxMapFileName(MediaTrack* track, int fx) {
	char name[100] = "";
	TrackFX_GetFXName(track, fx, name, sizeof(name));
	if (!name[0]) {
		// This will happen when there are no FX on this track.
		return "";
	}
	std::string path = getFxMapDir();
	path += "/";
	for (char* c = name; *c; ++c) {
		if (*c == '/' || *c == '\\' || *c == ':') {
			continue;
		}
		path += *c;
	}
	path += ".rkfm";
	return path;
}

#ifdef _WIN32
static std::wstring getPathForStd(const std::string& fileName) {
	// REAPER provides UTF-8 strings. However, on Windows, C++ std will
	// interpret a narrow (8 bit) string as an ANSI string. The easiest way to
	// deal with this is to convert the string to UTF-16, which Windows will
	// interpret correctly.
	static std::wstring_convert<std::codecvt_utf8_utf16<WCHAR>, WCHAR> utf8Utf16;
	try {
		return utf8Utf16.from_bytes(fileName);
	} catch (std::range_error) {
		// Invalid UTF-8. This really shouldn't happen, but just in case, this hack
		// just widens the string without any encoding conversion. This may result in
		// strange characters, but it's better than a crash.
		std::wostringstream s;
		s << fileName.c_str();
		return s.str();
	}
}
#else
static std::string getPathForStd(const std::string& fileName) {
	return fileName;
}
#endif

FxMap::FxMap(MediaTrack* track, int fx) : _track(track), _fx(fx) {
	lastTrack = track;
	lastFx = fx;
	const std::string path = getFxMapFileName(track, fx);
	if (path.empty()) {
		return;
	}
	std::ifstream input(getPathForStd(path));
	if (!input) {
		log("no FX map " << path);
		return;
	}
	log("loading FX map " << path);
	std::string line;
	while (std::getline(input, line)) {
		std::smatch m;
		std::regex_search(line, m, RE_LINE);
		if (m.empty()) {
			log("invalid FX map line: " << line);
			continue;
		}
		if (!m.str(1).empty()) {
			if (!this->_mapName.empty()) {
				log("map name specified more than once, ignoring: " << line);
				continue;
			}
			this->_mapName = m.str(1);
			log("map name: " << this->_mapName);
			continue;
		}
		const std::string numStr = m.str(2);
		if (!numStr.empty()) {
			const int rp = std::atoi(numStr.c_str());
			const int mp = this->_reaperParams.size();
			this->_reaperParams.push_back(rp);
			this->_mapParams.insert({rp, mp});
			const std::string paramName = m.str(3);
			if (!paramName.empty()) {
				this->_paramNames.insert({mp, paramName});
			}
			continue;
		}
		if (!m.str(4).empty()) {
			// A page break has been requested. Any remaining slots on this page
			// should be empty.
			while (this->_reaperParams.size() % BANK_NUM_SLOTS != 0) {
				this->_reaperParams.push_back(-1);
			}
			continue;
		}
		const std::string section = m.str(5);
		if (!section.empty()) {
			this->_sections.insert({this->_reaperParams.size(), section});
		}
	}
	log("loaded " << this->_mapParams.size() << " params from FX map");
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
		return TrackFX_GetNumParams(this->_track, this->_fx);
	}
	return this->_reaperParams.size();
}

int FxMap::getReaperParam(int mapParam) const {
	if (this->_reaperParams.empty()) {
		return mapParam;
	}
	return this->_reaperParams[mapParam];
}

int FxMap::getMapParam(int reaperParam) const {
	if (this->_reaperParams.empty()) {
		return reaperParam;
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
		char name[100];
		const int rp = this->getReaperParam(mapParam);
		TrackFX_GetParamName(this->_track, this->_fx, rp, name, sizeof(name));
		return name;
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
	const std::string path = getFxMapFileName(track, fx);
	if (path.empty()) {
		return getOrigName();
	}
	std::ifstream input(getPathForStd(path));
	if (!input) {
		return getOrigName();
	}
	std::string line;
	while (std::getline(input, line)) {
		std::smatch m;
		std::regex_search(line, m, RE_LINE);
		if (m.empty()) {
			continue;
		}
		if (!m.str(1).empty()) {
			return m.str(1);
		}
		if (!m.str(2).empty() || !m.str(4).empty() || !m.str(5).empty()) {
			// The map name must be the first non-comment, non-blank line. If we hit
			// anything else, there's no map name, so don't process any further.
			break;
		}
	}
	return getOrigName();
}

void FxMap::generateMapFileForSelectedFx() {
	const std::string fn = getFxMapFileName(lastTrack, lastFx);
	if (fn.empty()) {
		// No selected FX.
		return;
	}
	const std::string dir = getFxMapDir();
	std::filesystem::create_directories(getPathForStd(dir));
	std::ofstream output(getPathForStd(fn));
	if (!output) {
		return;
	}
	const int count = TrackFX_GetNumParams(lastTrack, lastFx);
	for (int p = 0; p < count; ++p) {
		char name[100];
		TrackFX_GetParamName(lastTrack, lastFx, p, name, sizeof(name));
		output << p << " # " << name << std::endl;
	}
}
