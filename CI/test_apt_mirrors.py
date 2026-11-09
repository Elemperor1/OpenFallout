#!/usr/bin/env python3
"""Tests of CI/apt_mirrors.sh, which lets the Harness build take another Ubuntu mirror when the default one is out of reach.

The script is run by bash against a copy of /etc/apt in a temporary folder and a stand-in `apt-get` that fails for the
hosts the test names, so no network or root is needed.
"""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).with_name("apt_mirrors.sh")

DEB822 = """Types: deb
URIs: http://archive.ubuntu.com/ubuntu/
Suites: noble noble-updates noble-backports
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://security.ubuntu.com/ubuntu/
Suites: noble-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
"""

# Fails (as apt-get update does for a host it cannot reach) when the first source of the file, the one for the
# noble suites, names one of the hosts in $DEAD, and records every call in $CALLS. The security pocket stays on its own
# host in the file, so only the first source tells which mirror is being tried.
FAKE_APT = """#!/usr/bin/env bash
echo "$*" >>"$CALLS"
first=$(grep -m1 '^URIs:' "$APT_ETC/sources.list.d/ubuntu.sources")
for dead in $DEAD; do
  if [[ "$first" == *"//$dead/"* ]]; then
    echo "Err: could not reach $dead" >&2
    exit 100
  fi
done
exit 0
"""


class AptMirrorsTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.etc = self.root / "etc-apt"
        (self.etc / "sources.list.d").mkdir(parents=True)
        self.sources = self.etc / "sources.list.d" / "ubuntu.sources"
        self.sources.write_text(DEB822)
        self.calls = self.root / "calls"
        fake = self.root / "apt-get"
        fake.write_text(FAKE_APT)
        fake.chmod(0o755)
        self.fake = fake

    def run_update(self, dead):
        env = dict(os.environ, APT_ETC=str(self.etc), APT_GET=str(self.fake), CALLS=str(self.calls), DEAD=dead)
        return subprocess.run(
            ["bash", "-c", f"set -euo pipefail; source {SCRIPT}; apt_update_with_fallback"],
            env=env,
            capture_output=True,
            text=True,
        )

    def number_of_updates(self):
        return len(self.calls.read_text().splitlines()) if self.calls.exists() else 0

    def test_default_mirror_that_answers_is_left_alone(self):
        result = self.run_update(dead="")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.number_of_updates(), 1)
        self.assertEqual(self.sources.read_text(), DEB822)

    def test_retries_are_set_for_every_apt_call_of_the_run(self):
        self.run_update(dead="")
        settings = (self.etc / "apt.conf.d" / "80-ci-retries").read_text()
        self.assertIn('Acquire::Retries "5";', settings)
        self.assertIn('Acquire::http::Timeout "30";', settings)

    def test_unreachable_default_mirror_falls_back_to_the_security_host(self):
        result = self.run_update(dead="archive.ubuntu.com")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.number_of_updates(), 2)
        text = self.sources.read_text()
        self.assertNotIn("archive.ubuntu.com", text)
        # Both pockets of the file are served by that host now, and nothing else of the file changed
        self.assertEqual(text.count("URIs: http://security.ubuntu.com/ubuntu/\n"), 2)
        self.assertIn("Suites: noble noble-updates noble-backports", text)
        self.assertIn("Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg", text)

    def test_next_mirror_is_made_from_the_original_sources(self):
        # The security host is out of reach as well: the third try uses the cloud mirror, and the file shows no trace
        # of the second
        result = self.run_update(dead="archive.ubuntu.com security.ubuntu.com")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.number_of_updates(), 3)
        text = self.sources.read_text()
        self.assertIn("http://us-central1.gce.archive.ubuntu.com/ubuntu", text)
        self.assertNotIn("http://archive.ubuntu.com", text)
        self.assertEqual((self.etc / "sources.list.d" / "ubuntu.sources.ci-orig").read_text(), DEB822)

    def test_a_country_mirror_is_replaced_too(self):
        self.sources.write_text(DEB822.replace("http://archive.ubuntu.com", "http://us.archive.ubuntu.com"))
        result = self.run_update(dead="us.archive.ubuntu.com")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("us.archive.ubuntu.com", self.sources.read_text())

    def test_every_mirror_out_of_reach_fails_the_step(self):
        result = self.run_update(
            dead="archive.ubuntu.com security.ubuntu.com us-central1.gce.archive.ubuntu.com azure.archive.ubuntu.com"
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("failed on every mirror", result.stderr)
        # the default and the three of the list
        self.assertEqual(self.number_of_updates(), 4)

    def test_old_style_sources_list_is_rewritten_too(self):
        (self.etc / "sources.list").write_text("deb http://archive.ubuntu.com/ubuntu noble main\n")
        self.run_update(dead="archive.ubuntu.com")
        self.assertEqual(
            (self.etc / "sources.list").read_text(), "deb http://security.ubuntu.com/ubuntu/ noble main\n"
        )


if __name__ == "__main__":
    unittest.main()
