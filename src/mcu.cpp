/*
 * ReaKontrol
 * Support for MCU-based MIDI protocol used by Komplete Kontrol Mk1
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2019 James Teh
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <sstream>
#include <cstring>
#include "reaKontrol.h"

using namespace std;

const unsigned char MIDI_SYSEX_END = 0xF7;

class McuSurface: public BaseSurface {
	public:
	McuSurface(int inDev, int outDev)
	: BaseSurface(inDev, outDev) {
	}

	virtual const char* GetTypeString() override {
		return "KompleteKontrolMcu";
	}

	virtual const char* GetDescString() override {
		return "Komplete Kontrol S-series Mk1";
	}

	protected:
	void _onMidiEvent(MIDI_event_t* event) override {
	}

};

IReaperControlSurface* createMcuSurface(int inDev, int outDev) {
	return new McuSurface(inDev, outDev);
}
