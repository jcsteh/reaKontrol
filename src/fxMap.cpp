#include "fxMap.h"
#include "reaKontrol.h"

FxMap::FxMap(MediaTrack* track, int fx) : _track(track), _fx(fx) {}

int FxMap::getParamCount() const {
	return TrackFX_GetNumParams(this->_track, this->_fx);
}

int FxMap::getReaperParam(int mapParam) const {
	return mapParam;
}

int FxMap::getMapParam(int reaperParam) const {
	return reaperParam;
}
