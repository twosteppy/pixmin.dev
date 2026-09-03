# subghz script templates

these are template .sub files with placeholder keys. use them to understand the file format for each protocol before capturing or generating real codes.

## format reference

```
Filetype: Flipper SubGhz Key File
Version: 1
Frequency: <hz>
Preset: FuriHalSubGhzPresetOok650Async
Protocol: <name>
Bit: <count>
Key: <hex bytes, space separated, 8 bytes total right-padded>
Te: <time element in microseconds>
```

## supported presets

- `FuriHalSubGhzPresetOok270Async` - narrowband OOK
- `FuriHalSubGhzPresetOok650Async` - standard OOK (most fixed code)
- `FuriHalSubGhzPreset2FSKDev238Async` - FSK narrow
- `FuriHalSubGhzPreset2FSKDev476Async` - FSK wide

## raw format

for captured signals without protocol recognition:

```
Filetype: Flipper SubGhz RAW File
Version: 1
Frequency: 433920000
Preset: FuriHalSubGhzPresetOok650Async
Protocol: RAW
RAW_Data: 200 -400 200 -400 600 -200 ...
```

values are pulse/gap durations in microseconds. positive = carrier on, negative = carrier off.
