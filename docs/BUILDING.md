# building from source

## requirements

| tool | version | install |
|---|---|---|
| git | any | package manager |
| python | 3.10+ | python.org |
| arm-none-eabi-gcc | 12.x | see below |
| scons | 4.x | `pip install scons` |

### arm toolchain

**ubuntu/debian**
```
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi
```

**mac**
```
brew install --cask gcc-arm-embedded
```

**windows**

download from https://developer.arm.com/downloads/-/gnu-rm and add to PATH.

## build steps

```bash
git clone https://github.com/twosteppy/pixmin.dev
cd pixmin.dev
python3 installer/install.py build
```

the installer will:
1. clone official flipper firmware at the pinned tag
2. copy pixmin application overlays into the firmware tree
3. run fbt to produce the updater package
4. (optionally) flash to connected flipper

## manual steps

if the installer doesn't work on your system:

```bash
git clone --depth 1 --branch 1.1.2 --recurse-submodules \
  https://github.com/flipperdevices/flipperzero-firmware flipper-fw

cp -r firmware/applications/external/* flipper-fw/applications/external/

cd flipper-fw
./fbt updater_package
```

output lands in `flipper-fw/dist/`.

## flashing manually

via qFlipper:
- open qFlipper, click install from file, select the `.dfu` or `.tgz` package

via dfu-util:
```bash
dfu-util -d 0483:5740 -a 0 -s 0x08000000 -D pixmin.dfu
```

via ufbt (external apps only):
```bash
cd firmware/applications/external/subghz_bruteforce
ufbt launch
```

## external apps only (no full rebuild)

to compile and sideload a single pixmin app without rebuilding the whole firmware:

```bash
pip install ufbt
cd firmware/applications/external/subghz_bruteforce
ufbt
ufbt launch
```

this works with stock firmware or any other custom firmware running on the flipper.
