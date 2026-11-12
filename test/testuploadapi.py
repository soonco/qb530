"""Run against a built nox binary, using an isolated profile and localhost only."""
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="upload-api-") as tmp:
    profile = Path(tmp)
    config = profile / "qBittorrent" / "config" / "qBittorrent.ini"
    config.parent.mkdir(parents=True)
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    config.write_text(
        "[LegalNotice]\nAccepted=true\n"
        "[WebUI]\nEnabled=true\nAddress=127.0.0.1\nLocalHostAuth=false\n"
        f"Port={port}\n"
        "[BitTorrent]\nSession\\DHTEnabled=false\nSession\\LSDEnabled=false\n"
        "Session\\PeXEnabled=false\nSession\\Interface=lo0\n"
        "Session\\InterfaceAddress=127.0.0.1\n"
        "[Preferences]\nConnection\\UPnP=false\n")
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    with (profile / "output.log").open("w+") as log:
        process = subprocess.Popen(
            [str(binary), f"--profile={profile}", "--confirm-legal-notice"],
            stdout=log, stderr=log, env=env)
        base = f"http://127.0.0.1:{port}"
        def request(path, data=None):
            body = None if data is None else urllib.parse.urlencode(data).encode()
            req = urllib.request.Request(base + path, body, headers={"Referer": base})
            return urllib.request.urlopen(req, timeout=3).read()
        def preferences():
            return json.loads(request("/api/v2/app/preferences"))
        def update(values, expected):
            request("/api/v2/app/setPreferences", {"json": json.dumps(values)})
            p = preferences()
            assert (p["fake_upload_ratio"], p["fake_upload_ratio_randomization"]) == expected, p
        try:
            for _ in range(100):
                try:
                    p = preferences()
                    break
                except Exception:
                    if process.poll() is not None:
                        log.seek(0)
                        raise RuntimeError(log.read())
                    time.sleep(0.1)
            else:
                log.seek(0)
                raise RuntimeError("WebUI did not start\n" + log.read())
            assert (p["fake_upload_ratio"], p["fake_upload_ratio_randomization"]) == (1, True)
            update({"fake_upload_ratio": 99}, (5, True))
            update({"fake_upload_ratio_randomization": False}, (5, False))
            update({"fake_upload_ratio": -3}, (1, False))
            update({"fake_upload_ratio": 1, "fake_upload_ratio_randomization": True}, (1, True))
            update({"fake_upload_ratio": 3}, (3, True))
            print("API defaults, clamp, independent partial updates and (1,true): PASS")
        finally:
            process.send_signal(signal.SIGTERM)
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        saved = config.read_text()
        assert "Session\\FakeUploadRatio=3" in saved
        assert "Session\\FakeUploadRatioRandomization=true" in saved
        print("Configuration persistence: PASS")
