# ReaKontrol

<!--beginDownload-->
[Downloads available on the ReaKontrol website](https://reakontrol.jantrid.net/)
<!--endDownload-->

- Author: James Teh &lt;jamie@jantrid.net&gt; & other contributors
- Copyright: 2018-2025 James Teh & other contributors
- License: GNU General Public License version 2.0

ReaKontrol is a [REAPER](https://www.reaper.fm/) extension which provides advanced host integration for [Native Instruments Komplete Kontrol keyboards](https://www.native-instruments.com/en/products/komplete/keyboards/).
It runs on both Windows and Mac and requires REAPER 5.92 or later.

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

## Download and Installation
For now, there is no installer.
You can download the latest build using the links at the top of this page.

Once downloaded, on Windows, simply copy the `reaper_kontrol.dll` file you downloaded to the `%appdata%\REAPER\UserPlugins` folder using Windows File Explorer.
You can get to this folder by copying the name above and pasting it into either the Windows Run dialog or the File Explorer address bar.
On Mac, copy the `reaper_kontrol.dylib` file to the `Library/Application Support/REAPER/UserPlugins` folder inside your home folder.

You do not need to add a control surface or perform any other configuration in REAPER.
Komplete Kontrol Host integration should work as soon as you start REAPER with a Komplete Kontrol keyboard connected.

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
