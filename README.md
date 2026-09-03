# pixmin

custom flipper zero firmware. built to outrun everything else.

no region locks. no hobbled subghz range. no missing protocols. real attack tools, real databases, real scripts, pre-loaded and working out of the box.

if you've used unleashed, momentum, or xtreme before, you already know the category. pixmin goes further on every axis.

---

## install

one command. works on windows, mac, linux.

```bash
python3 installer/install.py
```

or grab the latest release from [releases](https://github.com/twosteppy/pixmin.dev/releases) and drop the `.dfu` or `.uf2` onto your flipper via [qFlipper](https://flipperzero.one/update).

---

## what you get

### subghz

full 300-928 mhz range with zero artificial restrictions. protocols supported:

| protocol | bits | use case |
|---|---|---|
| princeton | 24 | garage doors, remotes |
| came | 12, 24 | european gates |
| came atomo | 18 | rolling code gates |
| came space | 12 | rolling code |
| nice flo | 12, 24 | nice brand gates |
| nice flor s | rolling | nice rolling code |
| faac slh | rolling | rolling code gates |
| bft mitto | rolling | bft gates |
| doorhan | rolling | doorhan rolling code |
| ansonic | 12 | generic OOK |
| holtek ht12x | 12 | generic fixed code |
| pt2260 | 24 | popular OOK chips |
| pt2262 | 24 | pt2262 variant |
| smc5326 | 25 | sunplus chip |
| unilarm | 25 | unilarm devices |
| linear delta 3 | 8 | linear remotes |
| mastercode | 24 | mastercode |
| security+ v1 | rolling | liftmaster/chamberlain |
| security+ v2 | rolling | chamberlain current gen |
| hormann hsm2/4 | rolling | hormann garage |
| marantec 128 | rolling | marantec |
| keeloq | rolling | car remotes, various |
| peccinin | rolling | peccinin gates |
| erreka | rolling | erreka gates |
| power smart | 32 | power smart |
| elsema | 16 | elsema |

preloaded captures in `scripts/subghz/` across gates, automotive, and industrial categories.

### subghz bruteforce

app that sequentially bruteforces fixed-code protocols. princeton 24-bit is 16 million codes. runs continuously, shows progress. configure frequency, protocol, bit depth, and key range from the flipper menu.

### wifi + bluetooth (requires esp32 wifi dev board)

six attack categories, multiple tools each:

**deauthentication**
- targeted: single ap + specific client mac
- broadcast: all clients on target ap
- channel sweep: hop channels, deauth everything
- timed burst: configurable duration, interval
- smart deauth: beacon-triggered, stays synced

**network recon**
- active ap scan with rssi, channel, encryption
- probe sniffer (client tracking + vendor id)
- hidden ssid probe brute (common name wordlist)
- wps enumeration (shows wps-enabled aps)
- channel congestion mapper

**handshake capture**
- pmkid attack (client-free, instant)
- full 4-way eapol capture (deauth-triggered)
- passive eapol monitor (wait for natural auth)
- multi-target parallel capture

**evil twin**
- open ap clone (exact ssid/channel match)
- wpa2 clone (captures connecting client handshake)
- captive portal (credential collection page)
- redirect portal (catch all, redir to real ap)

**beacon spam**
- custom ssid flood (up to 100 fake aps)
- rickroll beacon (rick astley wifi names)
- probe flood
- hidden ap flood

**ble attacks**
- apple continuity spam (ios proximity popups)
- android fast pair spam
- windows swift pair spam  
- samsung galaxy spam
- ble scanner + rssi tracker
- ble gatt fuzzer

all wifi tools communicate with esp32 marauder-compatible firmware over uart. works with official flipper wifi dev board, wifi devboard v2, and any esp32 with marauder flashed.

### nfc

- mifare classic read/write/clone
- gen4 magic card full emulation
- ntag read/write/clone
- uid spoof (gen1a, gen2, gen4 cuid)
- emv card data extraction
- nfc-a/b/f/v enumeration
- mifare ultralight c
- felica reader
- iso 15693

### rfid (125 khz)

- em4100 read/write/emulate
- hid prox read/write
- hid iclass read
- awid read
- t5577 raw write (any protocol)
- indala read/write
- hitag2 attack mode
- rfid fuzzer (sequential uid spray)

### infrared

remote database: 400+ devices preloaded.

| category | brands |
|---|---|
| tv | samsung, lg, sony, tcl, hisense, philips, panasonic, sharp, toshiba, vizio |
| ac | mitsubishi, daikin, lg, samsung, gree, midea, fujitsu, panasonic, carrier, haier |
| projector | epson, benq, optoma, viewsonic, sony, panasonic |
| media | apple tv, roku, nvidia shield, fire tv, chromecast, kodi boxes |
| amp/avr | yamaha, denon, pioneer, marantz, onkyo, sony |

ir brute mode: spray power/mute/source commands across all known protocols.

### badusb

80+ payloads preloaded, organized by os and purpose:

| category | payloads |
|---|---|
| windows recon | sysinfo, wifi dump, user enum, netstat, arp table, installed software, running processes, browser history extract |
| windows exfil | clipboard grab, env dump, ssh key steal, rdp cred pull |
| windows persistence | scheduled task, startup folder, registry run key, wmi subscription |
| mac recon | sysinfo, wifi passwords, users, network, installed apps |
| mac persistence | launchd plist, cron, login items |
| linux recon | sysinfo, users, cron, network, sudo check |
| linux persistence | cron job, systemd unit, bashrc append |
| misc | rickroll cmd, reverse shell launcher, lock screen, keyboard test |

payloads use duckyscript (flipper dialect). readable, editable, swappable on device.

### gpio

- uart terminal (direct serial comms via gpio pins)
- i2c scanner
- spi sniffer
- gpio voltage tester (3.3v/5v modes)
- nrf24 sniffer (requires nrf24 module on gpio)
- esp32 flasher (flash marauder via gpio uart)

### ui + ux

- custom boot animation (pixmin)
- reorganized menu (attack tools surfaced, not buried)
- dark theme assets
- battery percentage shown always
- file manager improvements (delete, move, rename)
- sd card health check
- device info: chip id, serial, firmware hash

---

## hardware support

| device | status |
|---|---|
| flipper zero (hw rev 6+) | full support |
| flipper zero (hw rev 10+) | full support |
| esp32 wifi dev board (official) | full wifi/ble |
| esp32 wifi dev board v2 | full wifi/ble |
| nrf24 gpio module | ble sniff + 2.4ghz |
| gpio expansion board | extended gpio |

---

## building from source

requires: git, python 3.10+, arm-gcc, fbt

```bash
git clone --recursive https://github.com/twosteppy/pixmin.dev
cd pixmin.dev
python3 installer/install.py --build-from-source
```

see [docs/BUILDING.md](docs/BUILDING.md) for full build instructions.

---

## updating scripts

scripts live in `scripts/` on the sd card. pull the latest:

```bash
python3 installer/install.py --update-scripts
```

---

## compared to other firmwares

| feature | unleashed | momentum | xtreme | pixmin |
|---|---|---|---|---|
| subghz protocols | 20 | 24 | 22 | 27 |
| preloaded subghz captures | 50 | 30 | 40 | 200+ |
| badusb payloads | 30 | 20 | 25 | 80+ |
| ir database entries | 200 | 150 | 180 | 400+ |
| wifi attack categories | 3 | 4 | 3 | 6 |
| ble attack types | 4 | 4 | 2 | 6 |
| rfid protocols | 8 | 10 | 9 | 12 |
| subghz bruteforce app | partial | yes | yes | full |
| nfc magic (gen4) | yes | yes | yes | yes |
| custom boot animation | yes | yes | yes | yes |

---

## license

mit. fork it, mod it, do whatever.

---

## contributing

prs welcome. script submissions especially. if you have captures, payloads, or ir codes not in the database, open a pr.

pixmin.dev
