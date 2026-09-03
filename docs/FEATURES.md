# feature reference

## subghz

full CC1101 frequency range: 300-928 MHz with no regional restrictions.

### fixed-code protocols

| protocol | bits | freq | notes |
|---|---|---|---|
| princeton | 24 | 433/315/868 | most common garage protocol |
| came | 12, 24 | 433 | european standard |
| came atomo | 18 | 433 | rolling emulation possible |
| came space | 12 | 433 | |
| nice flo | 12, 24 | 433 | nice brand |
| ansonic | 12 | 433 | |
| holtek ht12x | 12 | 433 | popular OOK encoder chip |
| pt2260 | 24 | 433 | pin-selectable |
| pt2262 | 24 | 433 | |
| smc5326 | 25 | 433 | sunplus |
| unilarm | 25 | 433 | |
| linear delta 3 | 8 | 310 | linear brand |
| mastercode | 24 | 433 | |
| elsema | 16 | 433 | |

### rolling-code protocols

| protocol | notes |
|---|---|
| security+ v1 | liftmaster legacy |
| security+ v2 | chamberlain/liftmaster current |
| keeloq | car remote base, various manufacturers |
| nice flor s | nice rolling |
| faac slh | FAAC rolling |
| bft mitto | BFT rolling |
| doorhan | doorhan rolling |
| hormann biSecure | HSM2/HSM4 |
| marantec | |
| peccinin | |
| erreka | |

### bruteforce app

sequential key transmitter. configure:
- frequency (300, 315, 433.92, 868.35 MHz, custom)
- protocol (any fixed-code protocol above)
- key range start/end
- TX delay per key
- repeat count per transmission

progress display shows current key, total coverage %, and TX count.

## wifi / bluetooth (esp32 required)

### compatible hardware

| board | notes |
|---|---|
| flipper wifi dev board (official) | plug into gpio, no wiring |
| wifi dev board v2 | direct plug |
| generic esp32 (wroom/wrover) | wire rx/tx to gpio 13/14 |

esp32 must be flashed with marauder-compatible firmware.

### deauth attack modes

- **targeted** - deauth a specific AP's clients
- **broadcast** - deauth all APs on a channel
- **channel sweep** - hop channels 1-13, deauth on each
- **timed burst** - run for configured duration
- **smart** - listen for beacons, deauth the beacon source continuously

### handshake capture modes

- **pmkid** - extract PMKID from first EAPOL frame without requiring a connected client. works against WPA2-Personal. captured hash goes to hashcat -m 22000.
- **4-way eapol** - trigger deauth to force re-authentication, capture handshake. classic approach, requires a client to be associated.
- **passive monitor** - sit and collect EAPOL frames from organic reconnections. slower but silent.

### evil twin options

- **open clone** - matches SSID and channel of target. clients with auto-reconnect will associate.
- **wpa2 clone** - presents WPA2 network. connecting clients attempt auth and reveal their hashed credential.
- **captive portal** - open AP + HTTP server presenting a login page. customize via evil portal firmware.
- **redirect portal** - catch-all redirect to legitimate site after credential collection.

### ble spam categories

apple continuity - triggers persistent popup on iOS 17+ devices:
- airpods pro pairing
- airpods max pairing
- apple watch setup
- apple TV setup

android fast pair - triggers pairing popups on android:
- google buds a-series
- pixel buds pro
- various earbuds

windows swift pair - triggers pairing notifications on win10/11:
- headphones
- keyboard
- mouse

samsung galaxy - SmartThings pairing popups

## nfc tools

on-device operations for NFC-A (ISO 14443-A):

- **mifare classic read** - sector-by-sector read with known keys + built-in key dictionary
- **mifare classic write** - write to specific blocks
- **mifare classic clone** - full dump then write to magic card
- **gen4 (CUID) operations** - write UID, change backdoor key, lock blocks
- **ntag read/write** - NTAG213/215/216 full access
- **uid emulation** - emulate any 4/7 byte UID
- **emv extract** - pull card number and expiry from contactless EMV cards

## rfid (125 kHz)

reader/writer for:
- em4100 (read, clone to T5577)
- hid proxcard II 26-bit
- hid iclass (read only)
- awid 26-bit
- indala 26-bit/29-bit
- hitag2 (read + challenge mode)

fuzzer: sequential UID emitter. configure start UID, end UID, emit delay. useful for testing access control readers.

## infrared

database coverage:

| category | brands | codes |
|---|---|---|
| tv | samsung, lg, sony, tcl, hisense, philips, panasonic, sharp | 200+ |
| ac | mitsubishi, daikin, lg, samsung, gree, midea, fujitsu | 80+ |
| projector | epson, benq, optoma, viewsonic, sony | 60+ |
| media | apple tv, roku, nvidia shield, fire tv | 40+ |
| amp/avr | yamaha, denon, marantz, onkyo | 40+ |

brute mode: transmit power, source, mute, volume commands across all known manufacturer codes sequentially. useful when you don't know what brand a device is.

## badusb

duckyscript engine (flipper dialect) with full key support. badusb studio app provides:
- on-device file browser organized by category
- script preview before execution
- abort during execution (back button)
- supports: STRING, STRINGLN, DELAY, DEFAULTDELAY, GUI, CTRL, SHIFT, ALT, function keys, arrow keys, all standard keys

## gpio

uart terminal: connect to ESP32 or any serial device on GPIO 13 (TX) / 14 (RX). configurable baud rate.

nrf24 sniffer: passive 2.4GHz packet capture via SPI on GPIO header. requires nRF24L01+ module.

i2c scanner: scans 0x00-0x7F on SDA/SCL pins, lists responding addresses.

esp32 flasher: flashes Marauder to an ESP32 connected via GPIO UART. auto-detects baud rate.
