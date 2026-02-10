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
			// Group 1: the line may start with a parameter number.
			"(\\d+)?"
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
		std::string numStr = m.str(1);
		if (!numStr.empty()) {
			const int rp = std::atoi(numStr.c_str());
			const int mp = this->_reaperParams.size();
			this->_reaperParams.push_back(rp);
			this->_mapParams.insert({rp, mp});
		}
	}
	log("loaded " << this->_reaperParams.size() << " params from FX map");
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
	return it->second;
}
