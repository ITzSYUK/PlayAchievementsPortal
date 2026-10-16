#!/usr/bin/env python3
"""Interactive RA login2 helper matching pinned rcheevos v12.5.0 rc_api_user.c.

Run by the owner. No credentials on the command line, no response/error bodies
on stdout, no redirect handling, verified HTTPS only. No third-party packages.
"""
import argparse
import getpass
import http.client
import json
import os
from pathlib import Path
import ssl
import sys
import warnings

MARKER = b"RAClientToken/1\n"
RESPONSE_LIMIT = 16384


def valid_field(value, limit):
    return isinstance(value, str) and 0 < len(value) <= limit and all(33 <= ord(c) <= 126 for c in value)


def encode_auth(username, token):
    if not valid_field(username, 64) or not valid_field(token, 128):
        raise ValueError("Unsupported credential format")
    return MARKER + username.encode("ascii") + b"\n" + token.encode("ascii") + b"\n"


def login(username, password):
    from urllib.parse import urlencode
    if not valid_field(username, 64) or not password:
        raise ValueError("Invalid input")
    # HTTPSConnection verifies hostname and certificates using the system CA
    # store. It never follows redirects or forwards a POST to another host.
    connection = http.client.HTTPSConnection("retroachievements.org", timeout=20, context=ssl.create_default_context())
    try:
        connection.request("POST", "/dorequest.php", urlencode({"r": "login2", "u": username, "p": password}),
                           {"Content-Type": "application/x-www-form-urlencoded", "Accept-Encoding": "identity",
                            "User-Agent": "RAClientPSP-Setup/0.16 rcheevos/12.5.0"})
        response = connection.getresponse()
        if response.status != 200:
            raise ValueError("Login HTTP failure")
        body = response.read(RESPONSE_LIMIT + 1)
        if len(body) > RESPONSE_LIMIT:
            raise ValueError("Login response too large")
        result = json.loads(body)
        if not isinstance(result, dict) or result.get("Success") is not True:
            raise ValueError("Login rejected")
        return encode_auth(result.get("User"), result.get("Token"))
    finally:
        connection.close()


def save_auth(path, data):
    # Refuse overwrite/symlinks. For re-login use --logout first.
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
    except BaseException:
        path.unlink(missing_ok=True)
        raise


def remove_auth(path):
    if path.name != "auth.txt" or path.is_symlink():
        raise ValueError("Unexpected token path")
    with path.open("rb") as stream:
        data = stream.read(257)
    normalized = data.replace(b"\r\n", b"\n")
    fields = normalized.splitlines()
    if len(data) > 256 or len(fields) != 3 or fields[0] != MARKER.rstrip(b"\n"):
        raise ValueError("Unrecognized token file")
    encode_auth(fields[1].decode("ascii"), fields[2].decode("ascii"))
    path.unlink()


def main():
    parser = argparse.ArgumentParser(description="RA session token for PSP; password is never stored. Do not share auth.txt.")
    parser.add_argument("--output", type=Path, required=True, help="Full path to RAClientLite/auth.txt on your Memory Stick")
    parser.add_argument("--logout", action="store_true", help="Delete this client's stored token file")
    args = parser.parse_args()
    try:
        if args.output.name != "auth.txt" or not args.output.parent.is_dir():
            raise ValueError("Expected an existing RAClientLite directory and auth.txt filename")
        if args.logout:
            remove_auth(args.output)
            print("Stored token removed. Close the running PSP game to end its in-memory login.")
            return 0
        if args.output.exists():
            raise ValueError("Token file exists; use --logout before re-login")
        if not sys.stdin.isatty():
            raise ValueError("Interactive terminal required")
        username = input("RetroAchievements username: ").strip()
        with warnings.catch_warnings():
            warnings.simplefilter("error", getpass.GetPassWarning)
            password = getpass.getpass("RetroAchievements password (hidden): ")
        try:
            data = login(username, password)
        finally:
            password = None
        save_auth(args.output, data)
        print("Session token saved. Do not share auth.txt; only send client-bootstrap.log and client-user.log.")
        return 0
    except (Exception, KeyboardInterrupt):
        # Network/server exception text may contain sensitive request/response
        # details. Deliberately print no exception text or traceback.
        print("Setup failed. Check credentials, connectivity and destination; an existing token file is not overwritten.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
