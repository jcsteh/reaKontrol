/*
 * ReaKontrol
 * Main plug-in code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2018-2019 James Teh
 * License: GNU General Public License version 2.0
 */

#include <oscpkt/udp.hh>
#include <windows.h>
#include <cstring>
#include <oscpkt/oscpkt.hh>
#define REAPERAPI_IMPLEMENT
#include "reaKontrol.h"

using namespace std;

const char ADDRESS[] = "127.0.0.1";
const int SEND_PORT = 7575;
const int RECV_PORT = 7576;
const char KK_FX_PREFIX[] = "VSTi: Komplete Kontrol";
const char KK_INSTANCE_PARAM_PREFIX[] = "NIKB";

const int TRACKTYPE_GENERIC = 5;

const string getKkInstanceName(MediaTrack* track) {
	int fxCount = TrackFX_GetCount(track);
	for (int fx = 0; fx < fxCount; ++fx) {
		// Find the Komplete Kontrol FX.
		char fxName[sizeof(KK_FX_PREFIX)];
		TrackFX_GetFXName(track, fx, fxName, sizeof(fxName));
		if (strcmp(fxName, KK_FX_PREFIX) != 0) {
			continue;
		}
		// Check for the instance name.
		// The first parameter should have a name in the form NIKBxx, where xx is a number.
		char paramName[7];
		TrackFX_GetParamName(track, fx, 0, paramName, sizeof(paramName));
		if (strncmp(paramName, KK_INSTANCE_PARAM_PREFIX, sizeof(KK_INSTANCE_PARAM_PREFIX) - 1) != 0) {
			return "";
		}
		return paramName;
	}
	return "";
}

class KkSurface: IReaperControlSurface {
	public:
	KkSurface() {
		this->_sendSock.connectTo(ADDRESS, SEND_PORT);
		this->_recvSock.bindTo(RECV_PORT);
		oscpkt::Message m;
		m.init("/remix/oscserver/startup").pushInt32(1);
		this->_sendMessage(m);
	}

	virtual ~KkSurface() {
		oscpkt::Message m;
		m.init("/script/shutdown").pushInt32(1);
		this->_sendMessage(m);
	}

	virtual const char* GetTypeString() override {
		return "KompleteKontrol";
	}

	virtual const char* GetDescString() override {
		return "Komplete Kontrol";
	}

	virtual const char* GetConfigString() override {
		return "";
	}

	virtual void Run() override {
		if (!this->_recvSock.receiveNextPacket(0)) {
			return;
		}
		oscpkt::PacketReader pr;
		pr.init(this->_recvSock.packetData(), this->_recvSock.packetSize());
		oscpkt::Message* inp = nullptr;
		if (!pr.isOk() || !(inp = pr.popMessage())) {
			return;
		}

		if (inp->match("/script/ping")) {
			oscpkt::Message out;
			out.init("/script/pong").pushInt32(1);
			this->_sendMessage(out);
		} else if (inp->match("/script/init")) {
			this->SetTrackListChange();
			MediaTrack* track = GetLastTouchedTrack();
			if (track) {
				this->SetSurfaceSelected(track, true);
			}
		} else if (inp->match("/live/play")) {
			CSurf_OnPlay();
		} else if (inp->match("/live/stop")) {
			CSurf_OnStop();
		} else if (inp->match("/live/session_record")) {
			CSurf_OnRecord();
		} else if (inp->match("/live/track/info")) {
			auto arg = inp->arg();
			arg.pop(); // Track type
			int trackId;
			arg.popInt32(trackId);
			// fixme: Use the actual track being requested and its actual name, states, etc.
			MediaTrack* track = GetLastTouchedTrack();
			oscpkt::Message out;
			out.init("/live/track/info");
			out.pushInt32(TRACKTYPE_GENERIC);
			out.pushInt32(trackId);
			out.pushStr("my name");
			out.pushInt32(0); // Colour
			out.pushInt32(1); // Arm
			out.pushInt32(0); // Solo
			out.pushInt32(0); // Mute
			out.pushFloat(0); // Volume
			out.pushFloat(0); // Pan
			this->_sendMessage(out);
		} else {
			ShowConsoleMsg(inp->addressPattern().c_str());
		}
	}

	virtual void SetTrackListChange() override {
		oscpkt::Message m;
		m.init("/live/size");
		m.pushInt32(CountTracks(0));
		m.pushInt32(0); // Number of scenes
		m.pushInt32(0); // Number of returns
		this->_sendMessage(m);
	}

	virtual void SetSurfaceSelected(MediaTrack* track, bool selected) override {
		if (!selected) {
			return;
		}
		oscpkt::Message m;
		m.init("/live/track");
		m.pushInt32(TRACKTYPE_GENERIC);
		m.pushInt32(CSurf_TrackToID(track, false));
		const string kkInstance = getKkInstanceName(track);
		m.pushStr(kkInstance.c_str());
		this->_sendMessage(m);
	}

	private:
	oscpkt::UdpSocket _sendSock;
	oscpkt::UdpSocket _recvSock;

	void _sendMessage(const oscpkt::Message& message) {
		oscpkt::PacketWriter pw;
		pw.init().addMessage(message);
		this->_sendSock.sendPacket(pw.packetData(), pw.packetSize());
	}

};

KkSurface* surface = nullptr;

extern "C" {

REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec) {
	if (rec) {
		// Load.
		if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc || REAPERAPI_LoadAPI(rec->GetFunc) != 0) {
			return 0; // Incompatible.
		}

		surface = new KkSurface();
		rec->Register("csurf_inst", (void*)surface);
		return 1;
	} else {
		// Unload.
		delete surface;
		return 0;
	}
}

}
