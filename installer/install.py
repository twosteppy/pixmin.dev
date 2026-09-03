#!/usr/bin/env python3
import os
import platform
import shutil
import sys
import tempfile
import urllib.request
import zipfile

REPO = "twosteppy/pixmin.dev"
ZIP_URL = f"https://github.com/{REPO}/releases/latest/download/pixmin-release.zip"


def find_sd():
    system = platform.system()

    if system == "Darwin":
        for entry in os.listdir("/Volumes"):
            path = f"/Volumes/{entry}"
            if os.path.isdir(os.path.join(path, "apps")) or os.path.isdir(os.path.join(path, "subghz")):
                return path

    elif system == "Linux":
        for root in ("/media", "/mnt", "/run/media"):
            if not os.path.exists(root):
                continue
            for a in os.listdir(root):
                for b in (os.path.join(root, a), os.path.join(root, a, "Flipper SD")):
                    if os.path.isdir(os.path.join(b, "apps")) or os.path.isdir(os.path.join(b, "subghz")):
                        return b

    elif system == "Windows":
        import ctypes, string
        mask = ctypes.windll.kernel32.GetLogicalDrives()
        for ch in string.ascii_uppercase:
            if mask & 1:
                drive = f"{ch}:\\"
                if os.path.isdir(os.path.join(drive, "apps")) or os.path.isdir(os.path.join(drive, "subghz")):
                    return drive
            mask >>= 1

    return None


def download(url, dest):
    print(f"downloading pixmin release...")
    req = urllib.request.Request(url, headers={"User-Agent": "pixmin-installer"})
    with urllib.request.urlopen(req, timeout=60) as r, open(dest, "wb") as f:
        total = int(r.headers.get("Content-Length", 0))
        done = 0
        while True:
            chunk = r.read(65536)
            if not chunk:
                break
            f.write(chunk)
            done += len(chunk)
            if total:
                pct = done * 100 // total
                bar = "#" * (pct // 2) + "-" * (50 - pct // 2)
                sys.stdout.write(f"\r  [{bar}] {pct}%")
                sys.stdout.flush()
    print()


def install_to(sd, zip_path):
    print(f"installing to {sd} ...")
    with zipfile.ZipFile(zip_path) as z:
        for member in z.infolist():
            dest = os.path.join(sd, member.filename)
            if member.is_dir():
                os.makedirs(dest, exist_ok=True)
            else:
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                with z.open(member) as src, open(dest, "wb") as out:
                    shutil.copyfileobj(src, out)
    print("done.")


def main():
    print("\npixmin installer\n")

    sd = find_sd()
    if not sd:
        print("flipper sd card not found.")
        print("make sure your flipper is plugged in and the sd card is mounted.")
        print("you can also extract pixmin-release.zip directly to your sd card.")
        sys.exit(1)

    print(f"found flipper sd at: {sd}")

    with tempfile.TemporaryDirectory() as tmp:
        zip_path = os.path.join(tmp, "pixmin-release.zip")
        download(ZIP_URL, zip_path)
        install_to(sd, zip_path)

    print("\nall done! apps are in Main Menu -> Apps on your flipper.")


if __name__ == "__main__":
    main()
