#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
import zipfile
from pathlib import Path


PIXMIN_VERSION = "1.0.0"
GITHUB_REPO = "twosteppy/pixmin.dev"
RELEASE_API = f"https://api.github.com/repos/{GITHUB_REPO}/releases/latest"
OFFICIAL_FW_REPO = "https://github.com/flipperdevices/flipperzero-firmware.git"
OFFICIAL_FW_TAG = "1.1.2"

FLIPPER_VENDOR_ID = 0x0483
FLIPPER_PRODUCT_ID = 0x5740

REQUIRED_PYTHON = (3, 10)
REQUIRED_ARM_GCC = "arm-none-eabi-gcc"
REQUIRED_FBT_DEPS = ["scons", "git"]

SCRIPT_DIRS = [
    "scripts/subghz",
    "scripts/nfc",
    "scripts/rfid",
    "scripts/infrared",
    "scripts/badusb",
]

SD_MOUNT_PATHS = {
    "Linux": ["/media", "/mnt", "/run/media"],
    "Darwin": ["/Volumes"],
    "Windows": [],
}


def banner():
    print("""
 ██████╗ ██╗██╗  ██╗███╗   ███╗██╗███╗   ██╗
 ██╔══██╗██║╚██╗██╔╝████╗ ████║██║████╗  ██║
 ██████╔╝██║ ╚███╔╝ ██╔████╔██║██║██╔██╗ ██║
 ██╔═══╝ ██║ ██╔██╗ ██║╚██╔╝██║██║██║╚██╗██║
 ██║     ██║██╔╝ ██╗██║ ╚═╝ ██║██║██║ ╚████║
 ╚═╝     ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝
""")
    print(f"  pixmin firmware installer v{PIXMIN_VERSION}")
    print(f"  https://pixmin.dev\n")


def fail(msg):
    print(f"[!] {msg}", file=sys.stderr)
    sys.exit(1)


def info(msg):
    print(f"[*] {msg}")


def ok(msg):
    print(f"[+] {msg}")


def warn(msg):
    print(f"[-] {msg}")


def check_python_version():
    if sys.version_info < REQUIRED_PYTHON:
        fail(f"python {REQUIRED_PYTHON[0]}.{REQUIRED_PYTHON[1]}+ required, got {sys.version}")
    ok(f"python {sys.version.split()[0]}")


def get_os():
    system = platform.system()
    if system not in ("Linux", "Darwin", "Windows"):
        fail(f"unsupported os: {system}")
    return system


def check_tool(name):
    path = shutil.which(name)
    if path:
        ok(f"found {name}: {path}")
        return True
    warn(f"not found: {name}")
    return False


def check_build_deps():
    info("checking build dependencies...")
    ok_count = 0
    for tool in REQUIRED_FBT_DEPS:
        if check_tool(tool):
            ok_count += 1
    arm = check_tool(REQUIRED_ARM_GCC)
    if not arm:
        warn("arm-none-eabi-gcc not found. install arm embedded toolchain:")
        warn("  ubuntu/debian: sudo apt install gcc-arm-none-eabi")
        warn("  mac:           brew install --cask gcc-arm-embedded")
        warn("  windows:       https://developer.arm.com/downloads/-/gnu-rm")
    return arm and ok_count == len(REQUIRED_FBT_DEPS)


def fetch_json(url):
    req = urllib.request.Request(url, headers={"User-Agent": "pixmin-installer"})
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            return json.loads(resp.read().decode())
    except Exception as e:
        fail(f"failed to fetch {url}: {e}")


def get_latest_release():
    info("fetching latest release info...")
    data = fetch_json(RELEASE_API)
    tag = data.get("tag_name", "unknown")
    assets = data.get("assets", [])
    ok(f"latest release: {tag}")
    return tag, assets


def download_file(url, dest, label=None):
    label = label or os.path.basename(dest)
    info(f"downloading {label}...")

    def progress(count, block_size, total_size):
        if total_size > 0:
            pct = min(100, count * block_size * 100 // total_size)
            bar = "#" * (pct // 2) + " " * (50 - pct // 2)
            sys.stdout.write(f"\r  [{bar}] {pct}%")
            sys.stdout.flush()

    req = urllib.request.Request(url, headers={"User-Agent": "pixmin-installer"})
    try:
        with urllib.request.urlopen(req, timeout=60) as resp, open(dest, "wb") as out:
            total = int(resp.headers.get("Content-Length", 0))
            downloaded = 0
            block = 8192
            while True:
                data = resp.read(block)
                if not data:
                    break
                out.write(data)
                downloaded += len(data)
                progress(downloaded // block, block, total)
    except Exception as e:
        fail(f"download failed: {e}")
    print()
    ok(f"downloaded {label}")


def verify_sha256(path, expected_hash):
    info(f"verifying sha256 for {os.path.basename(path)}...")
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    actual = h.hexdigest()
    if actual != expected_hash:
        fail(f"hash mismatch!\n  expected: {expected_hash}\n  got:      {actual}")
    ok("sha256 verified")


def find_flipper_dfu():
    system = get_os()
    info("searching for flipper zero in dfu mode...")

    if system in ("Linux", "Darwin"):
        try:
            result = subprocess.run(
                ["lsusb"],
                capture_output=True,
                text=True,
            )
            vid = format(FLIPPER_VENDOR_ID, "04x")
            pid = format(FLIPPER_PRODUCT_ID, "04x")
            for line in result.stdout.splitlines():
                if vid in line.lower() and pid in line.lower():
                    ok("flipper zero detected (dfu mode)")
                    return True
        except FileNotFoundError:
            pass

    if system == "Linux":
        try:
            import usb.core
            dev = usb.core.find(idVendor=FLIPPER_VENDOR_ID, idProduct=FLIPPER_PRODUCT_ID)
            if dev:
                ok("flipper zero detected via pyusb")
                return True
        except ImportError:
            pass

    warn("flipper zero not detected. make sure it's connected in dfu mode:")
    warn("  hold left button + plug usb, or")
    warn("  settings > power > reboot in dfu")
    return False


def find_qflipper():
    candidates = ["qFlipper", "qflipper", "qFlipper-x86_64.AppImage"]
    for c in candidates:
        path = shutil.which(c)
        if path:
            ok(f"qFlipper found: {path}")
            return path
    system = get_os()
    if system == "Darwin":
        apppath = "/Applications/qFlipper.app/Contents/MacOS/qFlipper"
        if os.path.exists(apppath):
            ok(f"qFlipper found: {apppath}")
            return apppath
    if system == "Windows":
        candidates_win = [
            os.path.expandvars(r"%LOCALAPPDATA%\Programs\qFlipper\qFlipper.exe"),
            r"C:\Program Files\qFlipper\qFlipper.exe",
        ]
        for c in candidates_win:
            if os.path.exists(c):
                ok(f"qFlipper found: {c}")
                return c
    return None


def flash_with_qflipper(qflipper_path, dfu_path):
    info(f"flashing with qFlipper: {dfu_path}")
    result = subprocess.run(
        [qflipper_path, "-d", dfu_path],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        warn(f"qFlipper output:\n{result.stderr}")
        fail("qFlipper flash failed")
    ok("flash complete")


def flash_with_dfu_util(dfu_path):
    dfu_util = shutil.which("dfu-util")
    if not dfu_util:
        fail("dfu-util not found. install it or use qFlipper.")
    info("flashing via dfu-util...")
    result = subprocess.run(
        [
            dfu_util,
            "-d", f"{FLIPPER_VENDOR_ID:04x}:{FLIPPER_PRODUCT_ID:04x}",
            "-a", "0",
            "-s", "0x08000000",
            "-D", dfu_path,
        ],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        warn(result.stderr)
        fail("dfu-util flash failed")
    ok("flash complete via dfu-util")


def find_flipper_sd(system):
    info("searching for flipper sd card...")
    mount_roots = SD_MOUNT_PATHS.get(system, [])

    if system == "Windows":
        import string
        import ctypes
        drives = []
        bitmask = ctypes.windll.kernel32.GetLogicalDrives()
        for letter in string.ascii_uppercase:
            if bitmask & 1:
                drives.append(f"{letter}:\\")
            bitmask >>= 1
        for drive in drives:
            marker = os.path.join(drive, "flipper_sd")
            if os.path.exists(marker):
                ok(f"flipper sd found at {drive}")
                return drive
            if os.path.exists(os.path.join(drive, "subghz")) and os.path.exists(os.path.join(drive, "infrared")):
                ok(f"flipper sd found at {drive}")
                return drive
        return None

    for root in mount_roots:
        if not os.path.exists(root):
            continue
        try:
            entries = os.listdir(root)
        except PermissionError:
            continue
        for entry in entries:
            candidate = os.path.join(root, entry)
            if os.path.isdir(candidate):
                if os.path.exists(os.path.join(candidate, "subghz")) or os.path.exists(os.path.join(candidate, "infrared")):
                    ok(f"flipper sd found at {candidate}")
                    return candidate
                for sub in os.listdir(candidate):
                    deeper = os.path.join(candidate, sub)
                    if os.path.isdir(deeper):
                        if os.path.exists(os.path.join(deeper, "subghz")):
                            ok(f"flipper sd found at {deeper}")
                            return deeper
    return None


def copy_scripts(sd_path, repo_root):
    info("copying scripts to sd card...")
    scripts_src = os.path.join(repo_root, "scripts")
    if not os.path.exists(scripts_src):
        warn("scripts directory not found in repo")
        return

    sd_map = {
        "subghz": "subghz",
        "nfc": "nfc",
        "rfid": "rfid",
        "infrared": "infrared",
        "badusb": "badusb",
    }

    for src_name, dst_name in sd_map.items():
        src_dir = os.path.join(scripts_src, src_name)
        dst_dir = os.path.join(sd_path, dst_name, "pixmin")
        if not os.path.exists(src_dir):
            continue
        os.makedirs(dst_dir, exist_ok=True)
        count = 0
        for root, dirs, files in os.walk(src_dir):
            rel = os.path.relpath(root, src_dir)
            out_root = os.path.join(dst_dir, rel)
            os.makedirs(out_root, exist_ok=True)
            for f in files:
                shutil.copy2(os.path.join(root, f), os.path.join(out_root, f))
                count += 1
        ok(f"  {src_name}: {count} files")

    ok("scripts installed to sd card")


def clone_official_firmware(dest):
    info(f"cloning official flipper firmware (tag {OFFICIAL_FW_TAG})...")
    if os.path.exists(dest):
        warn(f"directory {dest} already exists, skipping clone")
        return
    result = subprocess.run(
        [
            "git", "clone",
            "--depth", "1",
            "--branch", OFFICIAL_FW_TAG,
            "--recurse-submodules",
            "--jobs", "4",
            OFFICIAL_FW_REPO,
            dest,
        ],
        capture_output=False,
    )
    if result.returncode != 0:
        fail("failed to clone official firmware")
    ok("official firmware cloned")


def apply_pixmin_overlay(fw_dir, repo_root):
    info("applying pixmin overlay...")
    overlay_src = os.path.join(repo_root, "firmware", "applications", "external")
    overlay_dst = os.path.join(fw_dir, "applications", "external")

    if not os.path.exists(overlay_src):
        warn("no external applications overlay found")
        return

    os.makedirs(overlay_dst, exist_ok=True)
    for app_dir in os.listdir(overlay_src):
        src = os.path.join(overlay_src, app_dir)
        dst = os.path.join(overlay_dst, app_dir)
        if os.path.isdir(src):
            shutil.copytree(src, dst, dirs_exist_ok=True)
            ok(f"  app: {app_dir}")

    assets_src = os.path.join(repo_root, "firmware", "assets")
    if os.path.exists(assets_src):
        assets_dst = os.path.join(fw_dir, "assets", "pixmin")
        shutil.copytree(assets_src, assets_dst, dirs_exist_ok=True)
        ok("  assets overlay applied")


def run_fbt(fw_dir, target="updater_package"):
    info(f"building firmware (target: {target})...")
    fbt_script = os.path.join(fw_dir, "fbt")
    if not os.path.exists(fbt_script) and platform.system() == "Windows":
        fbt_script = os.path.join(fw_dir, "fbt.cmd")
    if not os.path.exists(fbt_script):
        fail(f"fbt not found in {fw_dir}")

    env = os.environ.copy()
    env["COMPACT"] = "1"
    env["DEBUG"] = "0"
    env["VERBOSE"] = "0"

    result = subprocess.run(
        [fbt_script, target],
        cwd=fw_dir,
        env=env,
    )
    if result.returncode != 0:
        fail("fbt build failed")
    ok("build complete")

    dist_dir = os.path.join(fw_dir, "dist")
    dfu_files = list(Path(dist_dir).rglob("*.dfu"))
    if dfu_files:
        ok(f"firmware: {dfu_files[0]}")
        return str(dfu_files[0])
    return None


def mode_install(args):
    system = get_os()
    repo_root = str(Path(__file__).parent.parent)

    tag, assets = get_latest_release()

    dfu_asset = None
    hash_asset = None
    for a in assets:
        name = a["name"].lower()
        if name.endswith(".dfu") and "pixmin" in name:
            dfu_asset = a
        if name.endswith(".sha256") or name.endswith(".hash"):
            hash_asset = a

    if not dfu_asset:
        fail("no .dfu file found in latest release. try --build-from-source")

    with tempfile.TemporaryDirectory() as tmpdir:
        dfu_path = os.path.join(tmpdir, dfu_asset["name"])
        download_file(dfu_asset["browser_download_url"], dfu_path, dfu_asset["name"])

        if hash_asset:
            hash_path = os.path.join(tmpdir, hash_asset["name"])
            download_file(hash_asset["browser_download_url"], hash_path, hash_asset["name"])
            with open(hash_path) as f:
                expected = f.read().strip().split()[0]
            verify_sha256(dfu_path, expected)

        if not args.skip_flash:
            qflipper = find_qflipper()
            if not find_flipper_dfu():
                warn("flipper not found. skipping flash. copy the dfu manually:")
                warn(f"  {dfu_path}")
            elif qflipper:
                flash_with_qflipper(qflipper, dfu_path)
            else:
                flash_with_dfu_util(dfu_path)

        if not args.skip_scripts:
            sd = find_flipper_sd(system)
            if sd:
                copy_scripts(sd, repo_root)
            else:
                warn("sd card not found. mount flipper as usb drive and re-run with --update-scripts")


def mode_build(args):
    repo_root = str(Path(__file__).parent.parent)
    fw_dir = os.path.join(repo_root, "flipper-zero-firmware")

    check_build_deps()
    clone_official_firmware(fw_dir)
    apply_pixmin_overlay(fw_dir, repo_root)
    dfu_path = run_fbt(fw_dir)

    if dfu_path and not args.skip_flash:
        qflipper = find_qflipper()
        if find_flipper_dfu():
            if qflipper:
                flash_with_qflipper(qflipper, dfu_path)
            else:
                flash_with_dfu_util(dfu_path)


def mode_scripts(args):
    system = get_os()
    repo_root = str(Path(__file__).parent.parent)
    sd = find_flipper_sd(system)
    if not sd:
        if args.sd_path:
            sd = args.sd_path
        else:
            fail("sd card not found. specify path with --sd-path /path/to/sd")
    copy_scripts(sd, repo_root)


def main():
    banner()
    check_python_version()

    parser = argparse.ArgumentParser(
        prog="install.py",
        description="pixmin firmware installer for flipper zero",
    )
    sub = parser.add_subparsers(dest="command")

    p_install = sub.add_parser("install", help="download and install latest release (default)")
    p_install.add_argument("--skip-flash", action="store_true", help="download only, don't flash")
    p_install.add_argument("--skip-scripts", action="store_true", help="don't copy scripts to sd card")

    p_build = sub.add_parser("build", help="build from source")
    p_build.add_argument("--skip-flash", action="store_true", help="build only, don't flash")

    p_scripts = sub.add_parser("update-scripts", help="copy scripts to sd card")
    p_scripts.add_argument("--sd-path", help="path to flipper sd card mount")

    args = parser.parse_args()

    if args.command is None or args.command == "install":
        if not hasattr(args, "skip_flash"):
            args.skip_flash = False
            args.skip_scripts = False
        mode_install(args)
    elif args.command == "build":
        mode_build(args)
    elif args.command == "update-scripts":
        mode_scripts(args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
