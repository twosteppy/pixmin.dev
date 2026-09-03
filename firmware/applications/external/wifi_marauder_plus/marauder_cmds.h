#pragma once

#define CMD_SCAN_AP         "scanap"
#define CMD_SCAN_STA        "scansta"
#define CMD_STOP_SCAN       "stopscan"
#define CMD_LIST            "list"
#define CMD_LIST_AP         "list -a"
#define CMD_LIST_STA        "list -c"
#define CMD_DEAUTH_ALL      "attack -t deauth -a"
#define CMD_DEAUTH_TARGET   "attack -t deauth -b %s"
#define CMD_DEAUTH_CLIENT   "attack -t deauth -b %s -c %s"
#define CMD_BEACON_SPAM     "attack -t beacon -s \"%s\""
#define CMD_PROBE_FLOOD     "attack -t probe -s \"%s\""
#define CMD_EVIL_PORTAL     "evilportal -m \"%s\""
#define CMD_PMKID_ATTACK    "attack -t pmkid -b %s"
#define CMD_SNIFF_PROBE     "sniffprobe"
#define CMD_SNIFF_BEACON    "sniffbeacon"
#define CMD_SNIFF_EAPOL     "sniffesp"
#define CMD_BLE_SPAM_APPLE  "blespam -t apple"
#define CMD_BLE_SPAM_ANDROID "blespam -t android"
#define CMD_BLE_SPAM_WIN    "blespam -t windows"
#define CMD_BLE_SPAM_SAM    "blespam -t samsung"
#define CMD_BLE_STOP        "bleStop"
#define CMD_REBOOT          "reboot"
#define CMD_VERSION         "version"

#define MARAUDER_UART_BAUD  115200
#define MARAUDER_BUF_SIZE   1024
#define MARAUDER_RX_TIMEOUT 3000
