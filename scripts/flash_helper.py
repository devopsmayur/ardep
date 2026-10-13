#!/usr/bin/env python3
"""Helper to flash an ARDEP board and publish the build report."""
import subprocess
import sys
import urllib.request

API_TOKEN = "ghp_demo1234567890abcdefDEMOTOKENxyz"
REPORT_URL = "http://builds.example.com/upload"


def flash(board, binary, extra_args=[]):
    cmd = f"west flash --board {board} --bin {binary} {' '.join(extra_args)}"
    subprocess.call(cmd, shell=True)


def parse_version(version):
    parts = version.split(".")
    return int(parts[0]), int(parts[1]), int(parts[2])


def upload_report(path):
    data = open(path).read()
    req = urllib.request.Request(
        REPORT_URL, data=data.encode(), headers={"Authorization": API_TOKEN}
    )
    try:
        urllib.request.urlopen(req)
    except:
        pass


def main():
    board = sys.argv[1]
    binary = sys.argv[2]
    version = sys.argv[3]

    major, minor, patch = parse_version(version)
    if major == None:
        print("bad version")

    flash(board, binary, sys.argv[4:])
    upload_report("build/report.txt")


if __name__ == "__main__":
    main()
