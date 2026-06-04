# Gateway

Gateway is a VST3 neural amp modeller plugin for Linux. It is based on
[NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin)
by Steven Atkinson and all contributors to the Neural Amp Modeler project.
All original copyright is retained by Steven Atkinson.

iPlug2, the framework used by the original plugin, does not currently support
Linux. Gateway is a Linux port built using [JUCE](https://juce.com). Because
JUCE is used, this project is released under the GNU General Public Licence v3.
See [LICENCE](https://github.com/rations/gateway-linux/blob/master/LICENCE) and
[NOTICE](https://github.com/rations/gateway-linux/blob/master/NOTICE) for full
details.

---

## Installing the pre-built release for testing

### System requirement

Gateway requires **glibc 2.38 or later**. This is present in:

- Devuan 6 (excalibur) / Debian 13 (trixie) or newer
- Ubuntu 24.04 LTS or newer
- Arch Linux (rolling)
- Fedora 39 or newer

Ubuntu 22.04 LTS and Debian 12 (bookworm) ship glibc 2.35/2.36 and will not
load this binary. Users on those systems should build from source (see below).

### Extract and install

Download `Gateway-0.1.0-linux-x86_64.tar.gz` from the
[Releases page](https://github.com/rations/gateway-linux/releases) and run:

```bash
mkdir -p ~/.vst3
tar -xzf Gateway-0.1.0-linux-x86_64.tar.gz -C ~/.vst3/
```

The plugin will appear as **Gateway** in any VST3-capable DAW (REAPER, Ardour,
Bitwig, Carla, etc.). No other dependencies need to be installed.

To remove:

```bash
rm -rf ~/.vst3/Gateway.vst3
```

---

## Building from source

```bash
git clone https://github.com/rations/gateway-linux.git
cd gateway-linux
git submodule update --init --recursive

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)

mkdir -p ~/.vst3
cp -r build/GatewayLinux_artefacts/Release/VST3/Gateway.vst3 ~/.vst3/
```

Required system packages (Debian/Ubuntu):

```
build-essential cmake pkg-config libx11-dev libxext-dev libxcursor-dev
libgl-dev libfreetype-dev libfontconfig-dev libpng-dev zlib1g-dev
```

---

## Credits

- [Steven Atkinson](https://github.com/sdatkinson) — Neural Amp Modeler, NeuralAmpModelerCore, AudioDSPTools
- All contributors to [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin)
- [JUCE](https://github.com/juce-framework/JUCE) — cross-platform audio application framework

---

## Licence

Gateway is free software released under the
[GNU General Public Licence v3](https://github.com/rations/gateway-linux/blob/master/LICENCE).

The Neural Amp Modeler DSP core (NeuralAmpModelerCore, AudioDSPTools) and the
original plugin code are copyright Steven Atkinson and used under the MIT
Licence. See [NOTICE](https://github.com/rations/gateway-linux/blob/master/NOTICE)
for the full attribution and third-party licence texts.
