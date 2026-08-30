## Downloads

- **Linux (x86_64)** — `NAMix-__VERSION__-linux-x86_64.tar.gz`, containing the VST3 plug-in and the standalone app
- **macOS (arm64/x86_64)** — `NAMix-__VERSION__-macos-<arch>.zip`, containing the VST3 plug-in, Audio Unit, and the standalone app
- **Windows (x86_64)** — `NAMix-__VERSION__-windows-x86_64.zip`, containing the VST3 plug-in and the standalone app
- **Demo models** — `large-muffin.nam`, `british-800-preamp.nam`, `cali-duo-reverb-85watt.nam`, `dirty-hamster.nam`, `clone-overdrive.nam`, `special-bundle-overdrive-183-rock-channel.nam`, `vinny-iii-drive.nam`, `brit-in-a-box.nam`, `special-bundle-overdrive-183-jazz-channel.nam`, `anxiety-drive.nam`, for trying the parametric knobs (no model is bundled)

## Before you install

**Linux**: requires **glibc 2.35 or later** (Ubuntu 22.04+, Debian 12+, Fedora 36+, and most current rolling-release distros). Ubuntu 20.04, Debian 11, RHEL/CentOS 9, and openSUSE Leap 15.x ship an older glibc and can't load these binaries -- build from source instead (see the README).

**macOS**: builds are signed with a Developer ID certificate and notarized by Apple -- no Gatekeeper warning on first launch.

**Windows**: builds are not code-signed -- the standalone `.exe` will raise a SmartScreen warning the first time you run it. Choose **More info**, then **Run anyway**.

Extract the archive, then either copy the `.vst3` (and, on macOS, `.component`) folder into your plug-in directory and rescan in your DAW, or run the standalone app/exe directly.

---

