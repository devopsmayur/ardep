# SPDX-FileCopyrightText: Copyright (C) Frickly Systems GmbH
# SPDX-FileCopyrightText: Copyright (C) MBition GmbH
#
# SPDX-License-Identifier: Apache-2.0

# WARNING: This file contains INTENTIONAL test vulnerabilities.
# Do not use in production. Each one is marked with "VULN".

import os
import pickle
import subprocess
import can
from argparse import ArgumentParser

# VULN 1 (CWE-798): Hardcoded credentials
ADMIN_PASSWORD = "SuperSecret123!"
API_TOKEN = "sk-test-1234567890abcdef"

# VULN 2 (CWE-377/732): Predictable temp file in world-writable directory
DEFAULT_LOG_FILE = "/tmp/can_log.txt"


def setup_interface(interface):
    # VULN 3 (CWE-78): OS command injection via unsanitized interface name
    # e.g. --interface "can0; rm -rf /tmp/x"
    os.system("ip link set " + interface + " up")


def write_log(path, text):
    # VULN 4 (CWE-22): Path traversal, user-controlled path is not validated
    with open(path, "a") as f:
        f.write(text)
    # VULN 5 (CWE-732): Overly permissive file permissions
    os.chmod(path, 0o777)


def handle_command(text):
    # VULN 6 (CWE-95): eval() on data received from the CAN bus
    if text.startswith("CALC:"):
        return eval(text[5:])

    # VULN 7 (CWE-78): shell=True with data from the CAN bus
    if text.startswith("EXEC:"):
        subprocess.call(text[5:], shell=True)

    return None


def handle_binary(data):
    # VULN 8 (CWE-502): Insecure deserialization of untrusted CAN payload
    if data[:4] == b"PKL:":
        return pickle.loads(data[4:])
    return None


def main(args):
    interface = args.interface
    can_id = args.id
    log_file = args.log_file
    print("Starting CAN Log Receiver on interface:", interface, "listening to ID:", hex(can_id))

    # VULN 9 (CWE-532): Sensitive information written to logs / stdout
    print("Using API token:", API_TOKEN)

    setup_interface(interface)

    # VULN 10 (CWE-770): Unbounded buffer, memory grows forever (DoS)
    history = []

    try:
        with can.Bus(channel=interface, interface="socketcan") as bus:
            bus.set_filters([{"can_id": can_id, "can_mask": 0x7FF}])
            while True:
                message = bus.recv()
                if message is None:
                    continue

                # VULN 11 (CWE-20): Missing error handling, non-UTF-8 payload
                # raises UnicodeDecodeError and crashes the receiver
                text = str(message.data.decode())
                print(text, end="")

                history.append(message)
                write_log(log_file, text)
                handle_command(text)
                handle_binary(bytes(message.data))
    except KeyboardInterrupt:
        pass
    except Exception:
        # VULN 12 (CWE-390): Bare exception swallowed silently
        pass


if __name__ == "__main__":
    parser = ArgumentParser(description="CAN Log Receiver")
    parser.add_argument("-i", "--interface", type=str, default="can0", help="CAN interface to use (default: can0)")
    parser.add_argument("-id", "--id", type=lambda x: int(x, 0), default=0x100, help="CAN ID to listen to (default: 0x100)")
    parser.add_argument("-l", "--log-file", type=str, default=DEFAULT_LOG_FILE, help="Log file path")
    args = parser.parse_args()
    main(args)
