# NeuralBlender

A simple amp modeling plugin based on RTNeural and NeuralAmpModeler (NAM)

Features:
  - Supports nam A1, nam A2, aidax, and json model files.
  - Standalone app and LV2 plugin
  - Can load up to 4 models simultaneously 
  - Can either blend them (normal) or switch between them like "channels" (exclusive mode)
  - VU meters can be disabled (global setting) to save a tiny bit of DSP load
  - Calibration target dB is now a user (global) setting
  - "Linked" calibration mode follows loudest model which has calib. enabled
  - Calibration can be tuned for guitar or bass
  - Each model slot / lane has:
    - input gain
    - output gain
    - pre-delay for phasing correction/effects
    - optional DC flip for more phasing effects
    - optional level calibration

On my Intel Core7 ultra, it loads 4 models in the middle of a busy live session, 64 sample buffers / 3 periods, and DSP load typically stays below 50%, no xruns. (with DSP threads pinned to p-cores)

Features considered for future versions: VST plugin, series mode(s), optionally more than 4 lanes, lane groups, impulse response / convolution, etc...

![NeuralBlender custom UI](data/screenshot-ui.png)
![NeuralBlender in Ardour](data/screenshot-ardour.png)

Compiles and installs with cmake.

Required libraries:
  - eigen3
  - lv2 (for LV2 plugin)
  - jack (for standalone)
  - cairo/x11 (for GUI)
  
See CMakeLists.txt for more details.

To build and install, from an empty build directory run something like:
```
cmake wherver/is/src/neuralblender
make -j `nproc`
sudo make install
```
For standalone version, see --help text for more info/options

## Supported systems

Should compile and work fine on any POSIX compliant OS including Linux, MacOS (to be fixed), FreeBSD, etc.

Tested on:
  - Void Linux
  - Linux Mint
  - FreeBSD

MacOS: Expected to work soon, currently needs to be fixed.

Compiling/running this on w**dows: Short answer, don't do microsuck malware. It's bad for you (and for everyone else).

Rationale: Seriously, i don't think many people would run a live rig that just randomly stops working right in the middle of a show - because its manufacturer just decided you need yet another "update". Also definitely not worth all the headaches and $$$ cost of porting to that joke of an API.

## License

NeuralBlender is licensed under the GNU General Public License v3.0 (GPL-3.0-or-later).
See the LICENSE file for full license text.
