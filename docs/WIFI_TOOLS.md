# wifi tools reference

all wifi attacks require an ESP32 with Marauder-compatible firmware connected to the flipper GPIO header.

## compatible firmware for esp32

| firmware | link | notes |
|---|---|---|
| flipper zero marauder | github.com/justcallmekoko/ESP32Marauder | primary, most features |
| blackmarlin | github.com/G4lile0/BlackMarlin | marauder fork, extra features |
| esp32 evil portal | github.com/bigbrodude6119/flipper-zero-evil-portal | captive portal focused |
| flipper http | github.com/jblanked/FlipperHTTP | http request bridge |
| esp32 bruteforce | community | wps bruteforce |

## flashing esp32

with pixmin installed, use GPIO > ESP32 Flasher to flash Marauder directly from the flipper.

or manually:

```bash
pip install esptool
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
  write_flash -z 0x1000 marauder.bin
```

## attack reference

### deauthentication

deauth frames are 802.11 management frames (type 0, subtype 12). they require no authentication to send. the receiver has no way to verify the sender is legitimate (PMF/802.11w partially mitigates this, but most APs and clients don't enforce it).

**targeted deauth**

picks a specific AP BSSID. all connected clients get deauth'd. use for targeted disruption or handshake capture.

marauder cmd: `attack -t deauth -b <bssid>`

**broadcast deauth**

sends deauth to broadcast address (FF:FF:FF:FF:FF:FF). hits all clients on all APs in range.

marauder cmd: `attack -t deauth -a`

**channel sweep**

marauder hops channels 1-13 (or 1-11 for US), deauthing on each. slower but broader coverage.

marauder cmd: `attack -t deauth -a -ch 0`

### handshake capture

**PMKID**

the PMKID is derived from: PMKID = HMAC-SHA1-128(PMK, "PMK Name" || AP_MAC || CLIENT_MAC)

captured from the first EAPOL frame without needing a connected client. feed the output to hashcat:

```
hashcat -m 22000 capture.hc22000 wordlist.txt
```

marauder cmd: `attack -t pmkid -b <bssid>`

**4-way eapol**

1. trigger deauth to force client disconnect
2. capture the 4-way handshake as client reconnects
3. hashcat -m 22000 or -m 2500 (legacy) against the capture

marauder cmd: `sniffesp` (monitors passively, deauth separately)

### evil twin

the flipper displays the target AP's SSID and channel so you can confirm the clone is broadcasting correctly before victims connect.

**open clone**

no password. devices with auto-reconnect will associate. useful for MITM on open networks or forcing association.

**wpa2 clone**

presents the same SSID and channel as the real AP with WPA2 enabled. when a client connects and enters the password, you capture a handshake equivalent.

**captive portal (evil portal firmware)**

open AP + embedded web server. customize the portal page (company login, wifi portal, etc.). credentials typed into the page are logged to the SD card.

default evil portal page templates: corporate login, ISP wifi portal, hotel portal.

### beacon spam

**ssid flood**

generates up to 100 fake access points with custom SSIDs. floods channel with beacons. useful for: congesting a channel, obscuring real APs, confusion attacks.

pixmin ships with some pre-built SSID lists (see scripts/badusb/misc/ for rickroll variants, add your own to SD).

**probe flood**

transmit probe requests for a given SSID. forces APs broadcasting that SSID to respond, revealing hidden APs.

### ble spam

**apple continuity**

sends BLE advertisement packets mimicking apple devices using the continuity protocol (0x004c manufacturer data). triggers pairing / notification popups on nearby iOS 17+ and macOS 14+ devices. continuous mode sends different device types in rotation.

**android fast pair**

google's fast pair uses 0x00FE service UUID with specific model IDs. pixmin ships with 20+ known device model IDs for maximum coverage.

**windows swift pair**

swift pair advertisements use 0x0006 manufacturer data. triggers "new device found" notifications on windows 10/11 with bluetooth enabled.

## notes on PMF / 802.11w

802.11w management frame protection makes deauth attacks harder but not impossible:
- PMF-capable APs and clients drop unsigned deauth frames
- devices without PMF are still vulnerable (most IoT, older routers)
- broadcast deauth still reaches non-PMF clients even on PMF APs
- pixie dust / wps attacks bypass PMF entirely (different attack surface)
