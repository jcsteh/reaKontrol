# ReaKontrol

<!--beginDownload-->
[Downloads available on the ReaKontrol website](https://reakontrol.jantrid.net/)
<!--endDownload-->

- Author: James Teh &lt;jamie@jantrid.net&gt; & other contributors
- Copyright: 2018-2026 James Teh & other contributors
- License: GNU General Public License version 2.0

ReaKontrol is a [REAPER](https://www.reaper.fm/) extension which provides advanced host integration for [Native Instruments Komplete Kontrol keyboards](https://www.native-instruments.com/en/products/komplete/keyboards/).
It runs on both Windows and Mac and requires REAPER 6.37 or later.

ReaKontrol supports Komplete Kontrol S-series Mk2, S-series Mk3, A-series and M-series keyboards.
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
- Adjustment of non- NKS FX parameters.
- Adjustment of non-NKS presets (Kontrol S MK3 only).

## Download and Installation
For now, there is no installer.
You can download the latest build using the links at the top of this page.

Once downloaded, on Windows, simply copy the `reaper_kontrol.dll` file you downloaded to the `%appdata%\REAPER\UserPlugins` folder using Windows File Explorer.
You can get to this folder by copying the name above and pasting it into either the Windows Run dialog or the File Explorer address bar.
On Mac, copy the `reaper_kontrol.dylib` file to the `Library/Application Support/REAPER/UserPlugins` folder inside your home folder.

Usually, you do not need to add a control surface or perform any other configuration in REAPER.
Komplete Kontrol Host integration should work as soon as you start REAPER with a Komplete Kontrol keyboard connected.
The one exception is if you have previously set an alias for the MIDI input or output device used for Kontrol host integration (e.g. Komplete Kontrol M DAW) in REAPER's Preferences.
ReaKontrol searches for the original device name, so an alias will likely interfere with automatic detection of the device.
If you have done this, you must first remove the alias (or aliases) and then reconnect ReaKontrol or restart REAPER.

## Non-NKS FX Parameters
Kontrol S MK3 has official support for non-NKS FX parameters.
Press the Plug-in button to access them.
You then use the knobs to adjust parameters, the page buttons to move through banks of parameters and buttons 1 through 8 to switch plugins.

Earlier Kontrol keyboards do not officially support non-NKS FX parameters.
However, ReaKontrol provides support for this.
To access them, press the Mixer button on S MK2, or Track button on A/M series keyboards, then press the 4-d encoder.
You then use the knobs to adjust parameters.
If you're using a screen reader, you should disable OSARA's "Report changes made via control surfaces" setting, as ReaKontrol will report parameter changes itself.
Note also that touching a knob will not provide any feedback.
Instead, you can quickly turn a knob first to report its parameter and value.
Subsequent turns will adjust the value.
On S MK2, you can use the page buttons to move through banks of parameters.
On A and M series keyboards, pulling the 4-d encoder right and left moves through banks instead.
Use 4-d encoder down and up to switch plugins.
To return to normal operation, press the 4-d encoder again.

## Configurable Mapping of Non-NKS FX Parameters
By default, all FX parameters are mapped to pages and knobs on the Kontrol keyboard.
For plugins with many parameters, this might not be efficient.
ReaKontrol enables you to specify how parameters are mapped to Kontrol knobs for specific plugins using map files.
ReaKontrol will load these maps from `reaKontrol/fxMaps/plugin name.rkfm` inside the REAPER resource folder.
Slash, backslash and colon are removed from the plugin name.
For example, on Windows, you might have `%appdata%\REAPER\reaKontrol\fxMaps\VST ReaComp (Cockos).rkfm`.
The file format is as follows:

```
# This is a comment.
ReaComp: # This is optional and overrides the FX name reported to users.
0 # Parameter 0 will be mapped to the first knob on the first page.
2 # Parameter 2 will be mapped to the second knob.
4 Pre comp # Overrides the name reported to the user for this third knob.
--- # Page break. The remaining knobs on this page will be unassigned.
[mix] # This is a section name for the following knobs.
6 # Parameter 6 will be mapped to the first knob on the second page.
7 /8 penguin # Adjustments will be 8 times smaller than usual for this parameter.
8 *8 goat # Adjustments will be 8 times larger than usual for this parameter.
```

ReaKontrol can help you create these maps.
First, select the FX you want to work with using your Kontrol keyboard.
Note that the first FX (if any) will be automatically selected when you navigate to a track.
Then, run the "ReaKontrol: Generate map file for selected FX" action and it will generate the appropriate map file with all the parameter numbers and their names as comments.

## Reconnecting
ReaKontrol will connect to a Kontrol keyboard when REAPER starts.
If the keyboard isn't turned on or connected when REAPER starts, ReaKontrol is not currently able to detect when the keyboard is connected.
This is also true if the keyboard is disconnected and then reconnected while REAPER is running.
If this occurs, you can use the ReaKontrol: Reconnect action available in the REAPER actions list.
This will make ReaKontrol reconnect to the keyboard without needing to restart REAPER.

## Reporting Issues
Issues should be reported [on GitHub](https://github.com/jcsteh/reaKontrol/issues).

## Building
This section is for those interested in building ReaKontrol from source code.

### Getting the Source Code
The ReaKontrol Git repository is located at https://github.com/jcsteh/reaKontrol.git.
You can clone it with the following command, which will place files in a directory named reaKontrol:

```
git clone --recursive https://github.com/jcsteh/reaKontrol.git
```

The `--recursive` option is needed to retrieve Git submodules we use.
If you didn't pass this option to `git clone`, you will need to run `git submodule update --init`.
Whenever a required submodule commit changes (e.g. after git pull), you will need to run `git submodule update`.
If you aren't sure, run `git submodule update` after every git pull, merge or checkout.

### Dependencies
To build ReaKontrol, you will need:

- Several git submodules used by ReaKontrol.
	See the note about submodules in the previous section.

#### Windows
- [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)

	Visual Studio 2022 Community/Professional/Enterprise is also supported.
	However, Preview versions of Visual Studio will not be detected and cannot be used.

	Whether installing Build Tools or Visual Studio, you must enable the following:

	* In the list on the Workloads tab, in the Desktop & Mobile grouping: Desktop development with C++
	* Then in the Installation details tree view, under Desktop development with C++ > Optional:
		- C++ Clang tools for Windows
		- Windows 11 SDK (10.0.22000.0)

- [Python](https://www.python.org/downloads/), version 3.7 or later:
- [SCons](https://www.scons.org/), version 3.0.4 or later:
	* Once Python is installed, you should be able to install SCons by running this at the command line:

	`py -3 -m pip install scons`

#### Mac OS
- Xcode 13: download from the [Mac App Store](https://apps.apple.com/us/app/xcode/id497799835?ls=1&mt=12).
	* Please run `xcode` at least once to make sure the latest command line tools are installed on your system.
- Homebrew: download and install using the instructions at the [Homebrew website](http://brew.sh)
	* Verify the installation with the `brew doctor` and `brew update` commands.
- download and install `python` and `scons` using the `brew install` command.

### How to Build
To build ReaKontrol, from a command prompt, simply change to the ReaKontrol checkout directory and run `scons`.
The resulting extension can be found in the `build` directory.

## Contributors
- James Teh
- Leonard de Ruijter
- brumbear@pacificpeaks
