#include <fstream>
#include <regex>
#include <string>
#include "fxMap.h"
#include "reaKontrol.h"

FxMap::FxMap(MediaTrack* track, int fx) : _track(track), _fx(fx) {
	char name[100] = "";
	TrackFX_GetFXName(this->_track, this->_fx, name, sizeof(name));
	if (!name[0]) {
		// This will happen when there are no FX on this track.
		return;
	}
	std::string path(GetResourcePath());
	path += "/reaKontrol/fxMaps/";
	path += name;
	path += ".rkfm";
	std::ifstream input(path);
	if (!input) {
		log("no FX map for " << name);
		return;
	}
	log("loading FX map " << path);
	std::string line;
	while (std::getline(input, line)) {
		static const std::regex RE_LINE(
			// Ignore any space at the start of a line.
			"^\\s*"
			"(?:"
			// Group 1: the line may be a parameter number.
			"(\\d+)"
			// Group 2: or a page break indicator.
			"|(---)"
			// Group 3: or a section name ending with a colon.
			"|([^#]+):"
			")?"
			// This may be followed by optional space and an optional comment starting
			// with "#".
			"\\s*(?:#.*)?$"
		);
		std::smatch m;
		std::regex_search(line, m, RE_LINE);
		if (m.empty()) {
			log("invalid FX map line: " << line);
			continue;
		}
		const std::string numStr = m.str(1);
		if (!numStr.empty()) {
			const int rp = std::atoi(numStr.c_str());
			const int mp = this->_reaperParams.size();
			this->_reaperParams.push_back(rp);
			this->_mapParams.insert({rp, mp});
			continue;
		}
		if (!m.str(2).empty()) {
			// A page break has been requested. Any remaining slots on this page
			// should be empty.
			while (this->_reaperParams.size() % BANK_NUM_SLOTS != 0) {
				this->_reaperParams.push_back(-1);
			}
			continue;
		}
		const std::string section = m.str(3);
		if (!section.empty()) {
			this->_sections.insert({this->_reaperParams.size(), section});
		}
	}
	log("loaded " << this->_mapParams.size() << " params from FX map");
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
