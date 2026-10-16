"""Synthetic credentials and mocked HTTPS only. Never accesses an account."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import ssl
import tempfile
import unittest
from unittest.mock import patch
from urllib.parse import parse_qs

source = Path(__file__).resolve().parents[2] / "scripts" / "configure_psp_login.py"
spec = importlib.util.spec_from_file_location("setup_login", source)
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


class FakeResponse:
    status = 200
    body = json.dumps({"Success": True, "User": "fixture", "Token": "synthetic-test-only"}).encode()

    def read(self, limit):
        return self.body[:limit]


class FakeConnection:
    def __init__(self, host, *, timeout, context):
        assert host == "retroachievements.org" and timeout == 20
        assert context.check_hostname and context.verify_mode == ssl.CERT_REQUIRED
        self.closed = False

    def request(self, method, path, post, headers):
        assert method == "POST" and path == "/dorequest.php"
        assert parse_qs(post) == {"r": ["login2"], "u": ["fixture"], "p": ["synthetic & password"]}
        assert headers["Accept-Encoding"] == "identity"

    def getresponse(self):
        return FakeResponse()

    def close(self):
        self.closed = True


class LoginTest(unittest.TestCase):
    def test_verified_https_and_no_output(self):
        output = io.StringIO()
        with patch.object(setup.http.client, "HTTPSConnection", FakeConnection), contextlib.redirect_stdout(output):
            data = setup.login("fixture", "synthetic & password")
        self.assertEqual(data, b"RAClientToken/1\nfixture\nsynthetic-test-only\n")
        self.assertEqual(output.getvalue(), "")

    def test_bad_server_data_and_redirects(self):
        for status, body in [(302, b""), (403, b"secret-error"), (200, b"x" * 16385), (200, b"not JSON"),
                             (200, b'{"Success":false,"Error":"secret-error"}'),
                             (200, b'{"Success":true,"User":"fixture","Token":"bad\\nvalue"}')]:
            with self.subTest(status=status, size=len(body)), patch.object(FakeResponse, "status", status), \
                    patch.object(FakeResponse, "body", body), patch.object(setup.http.client, "HTTPSConnection", FakeConnection):
                with self.assertRaises((ValueError, UnicodeError)):
                    setup.login("fixture", "synthetic & password")

    def test_file_ownership_and_logout(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "auth.txt"
            data = setup.encode_auth("fixture", "synthetic-test-only")
            setup.save_auth(path, data)
            with self.assertRaises(FileExistsError):
                setup.save_auth(path, data)
            self.assertEqual(path.read_bytes(), data)
            setup.remove_auth(path)
            self.assertFalse(path.exists())
            path.write_bytes(b"unrelated file")
            with self.assertRaises(ValueError):
                setup.remove_auth(path)
            self.assertEqual(path.read_bytes(), b"unrelated file")

    def test_cli_does_not_print_exception_or_credentials(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = io.StringIO()
            with patch.object(setup.sys, "argv", ["setup", "--output", str(Path(temporary) / "auth.txt")]), \
                    patch.object(setup.sys.stdin, "isatty", return_value=True), patch("builtins.input", return_value="fixture"), \
                    patch.object(setup.getpass, "getpass", return_value="synthetic & password"), \
                    patch.object(setup, "login", side_effect=RuntimeError("synthetic & password secret-token")), \
                    contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
                self.assertEqual(setup.main(), 1)
            self.assertNotIn("secret-token", output.getvalue())
            self.assertNotIn("synthetic & password", output.getvalue())


if __name__ == "__main__":
    unittest.main()
