# ReaKontrol

- Author: James Teh &lt;jamie@jantrid.net&gt; & other contributors
- Copyright: 2018-2019 James Teh & other contributors
- License: GNU General Public License version 2.0

ReaKontrol is a [REAPER](https://www.reaper.fm/) extension which provides advanced host integration for [Native Instruments Komplete Kontrol keyboards](https://www.native-instruments.com/en/products/komplete/keyboards/).
It currently only runs on Windows and requires REAPER 5.92 or later.

ReaKontrol supports Komplete Kontrol S-series Mk2, A-series and M-series keyboards.
While some initial work has been done to support S-series Mk1 keyboards, this is not yet functional.

## Supported Functionality
The following functionality is currently supported:

- Focus follow; i.e. the Komplete Kontrol instance is switched automatically when a track is selected.
- Transport buttons: Play, Restart, Record, Stop, Metronome, Tempo
- Edit buttons: Undo, Redo
- Track navigation
- Clip navigation: moves between project markers
- Mixer view: volume/pan adjustment with the 8 knobs
- The track name and mute, solo and armed states are displayed as appropriate.

## Download and Installation
For now, there is no installer.
You can [download the latest build of the extension here](https://osara.reaperaccessibility.com/reaper_kontrol.dll).

Once downloaded, simply copy the `reaper_kontrol.dll` file you downloaded to the `%appdata%\REAPER\UserPlugins` folder using Windows File Explorer.
You can get to this folder by copying the name above and pasting it into either the Windows Run dialog or the File Explorer address bar.

You do not need to add a control surface or perform any other configuration in REAPER.
Komplete Kontrol Host integration should work as soon as you start REAPER with a Komplete Kontrol keyboard connected.

## Reporting Issues
Issues should be reported [on GitHub](https://github.com/jcsteh/reaKontrol/issues).

## Building
This section is for those interested in building ReaKontrol from source code.

### Getting the Source Code
The ReaKontrol Git repository is located at https://github.com/jcsteh/reaKontrol.git.
You can clone it with the following command, which will place files in a directory named reaKontrol:

```
git clone https://github.com/jcsteh/reaKontrol.git
```

### Dependencies
To build ReaKontrol, you will need:

- Microsoft Visual Studio 2017 Community:
	* Visual Studio 2019 is not yet supported.
	* [Download Visual Studio 2017 Community](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=15)
	* When installing Visual Studio, you need to enable the following:
		- On the Workloads tab, in the Windows group: Desktop development with C++
- Python, version 2.7:
	* This is needed by SCons.
	* Python 3 and later are not yet supported.
	* [Download Python](https://www.python.org/downloads/)
- [SCons](https://www.scons.org/), version 3.0.4 or later:
	* Once Python is installed, you should be able to install SCons by simply running this at the command line: `pip install scons`

### How to Build
To build ReaKontrol, from a command prompt, simply change to the ReaKontrol checkout directory and run `scons`.
The resulting dll can be found in the `build` directory.

## Contributors
- James Teh
- Leonard de Ruijter
