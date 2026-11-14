"""CI/macos/release_signing.py run as CI runs it, with stand-ins for security, xcrun, codesign, hdiutil and ditto that
record their arguments. No keychain, certificate or Apple service is touched."""

import base64
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "CI/macos/release_signing.py"

P12_BYTES = b"P12-CERTIFICATE-AND-KEY-SECRET-BYTES"
P12_PASSWORD = "p12-password-secret-9431"
NOTARY_KEY = "-----BEGIN PRIVATE KEY-----\nNOTARY-KEY-SECRET-77\n-----END PRIVATE KEY-----\n"
NOTARY_KEY_ID = "KEYIDSECRET1"
NOTARY_ISSUER = "issuer-secret-0f3c-4e5d"
TEAM = "TEAMSECRET9"
IDENTITY_HASH = "0123456789ABCDEF0123456789ABCDEF01234567"
SECRETS = {
    "MACOS_DEVELOPER_ID_P12_BASE64": base64.b64encode(P12_BYTES).decode(),
    "MACOS_DEVELOPER_ID_P12_PASSWORD": P12_PASSWORD,
    "MACOS_NOTARY_API_KEY": NOTARY_KEY,
    "MACOS_NOTARY_API_KEY_ID": NOTARY_KEY_ID,
    "MACOS_NOTARY_API_ISSUER_ID": NOTARY_ISSUER,
    "MACOS_TEAM_ID": TEAM,
}
SECRET_VALUES = [P12_PASSWORD, NOTARY_KEY_ID, NOTARY_ISSUER, "NOTARY-KEY-SECRET-77", SECRETS["MACOS_DEVELOPER_ID_P12_BASE64"],
                 P12_BYTES.decode()]

# One stand-in for every tool: it appends its argv to $STUB_LOG and answers the calls the script reads.
STUB = textwrap.dedent("""\
    #!{python}
    import json, os, sys
    from pathlib import Path
    name, args = os.path.basename(sys.argv[0]), sys.argv[1:]
    with open(os.environ["STUB_LOG"], "a") as log:
        log.write(json.dumps([name] + args) + "\\n")
    def flag(option):
        return args[args.index(option) + 1] if option in args else None
    if name == "security" and args[:1] == ["find-identity"]:
        print(os.environ.get("STUB_IDENTITIES", ""))
    elif name == "security" and args[:2] == ["list-keychains", "-d"] and "-s" not in args:
        print('    "/Users/runner/Library/Keychains/login.keychain-db"')
    elif name == "security" and args[:1] == ["import"]:
        # the certificate file must exist while it is imported
        sys.exit(0 if Path(args[1]).is_file() else 3)
    elif name == "xcrun" and args[:2] == ["notarytool", "submit"]:
        key = flag("--key")
        if not key or not Path(key).is_file():
            sys.exit(4)
        status = os.environ.get("STUB_NOTARY_STATUS", "Accepted")
        print(json.dumps({{"id": "submission-1", "status": status, "message": "Processing complete"}}))
    elif name == "xcrun" and args[:2] == ["notarytool", "log"]:
        print(json.dumps({{"issues": [{{"path": "OpenFallout.app/Contents/MacOS/openfallout",
                                        "message": "The signature does not include a secure timestamp."}}]}}))
    elif name == "hdiutil" and args[:1] == ["create"]:
        Path(args[-1]).write_bytes(b"new image")
    elif name == "ditto" and "-c" not in args:
        (Path(args[-1]) / "OpenFallout.app").mkdir(parents=True, exist_ok=True)
    elif name == "ditto":
        Path(args[-1]).write_bytes(b"zip")
""")
TOOLS = ("security", "xcrun", "codesign", "hdiutil", "ditto")
GOOD_IDENTITIES = (f'  1) {IDENTITY_HASH} "Developer ID Application: Example Studio ({TEAM})"\n'
                   f"     1 valid identities found")


class ReleaseSigningTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="release signing ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        for tool in TOOLS:
            stub = self.bin / tool
            stub.write_text(STUB.format(python=sys.executable))
            stub.chmod(0o755)
        self.runner_temp = self.root / "runner temp"
        self.runner_temp.mkdir()
        self.log = self.root / "calls.log"
        self.github_env = self.root / "github_env"
        self.github_output = self.root / "github_output"

    def run_script(self, *args, secrets=None, event="push", ref="refs/tags/openfallout-1.0", ref_type="tag", **extra):
        env = {key: value for key, value in os.environ.items() if not key.startswith(("MACOS_", "OPENFALLOUT_", "GITHUB_"))}
        env.update({
            "PATH": f"{self.bin}{os.pathsep}{env.get('PATH', '')}",
            "STUB_LOG": str(self.log),
            "STUB_IDENTITIES": GOOD_IDENTITIES,
            "RUNNER_TEMP": str(self.runner_temp),
            "GITHUB_ENV": str(self.github_env),
            "GITHUB_OUTPUT": str(self.github_output),
            "GITHUB_EVENT_NAME": event,
            "GITHUB_REF": ref,
            "GITHUB_REF_TYPE": ref_type,
            "OPENFALLOUT_DEFAULT_BRANCH": "master",
        })
        env.update(SECRETS if secrets is None else secrets)
        env.update(extra)
        result = subprocess.run([sys.executable, str(SCRIPT), *args], env=env, capture_output=True, text=True)
        return result.returncode, result.stdout + result.stderr

    def calls(self):
        if not self.log.exists():
            return []
        return [json.loads(line) for line in self.log.read_text().splitlines()]

    def outputs(self, path):
        return dict(line.split("=", 1) for line in path.read_text().splitlines()) if path.exists() else {}

    def assert_no_secrets(self, output):
        for value in SECRET_VALUES:
            self.assertNotIn(value, output)

    # ------------------------------------------------------------------------------------------------- the gate

    def test_pull_request_builds_ad_hoc_and_skips_notarization(self):
        status, output = self.run_script("setup", event="pull_request", ref="refs/pull/7/merge", ref_type="branch")
        self.assertEqual(status, 0, output)
        self.assertIn("ad hoc build, notarization skipped", output)
        self.assertIn("pull request", output)
        self.assertEqual(self.outputs(self.github_output), {"mode": "adhoc"})
        self.assertEqual(self.calls(), [])
        self.assertFalse(self.github_env.exists() and self.github_env.read_text().strip())

    def test_run_without_secrets_on_a_tag_builds_ad_hoc(self):
        status, output = self.run_script("setup", secrets={})
        self.assertEqual(status, 0, output)
        self.assertIn("ad hoc build, notarization skipped", output)
        self.assertIn("no signing secrets", output)
        self.assertEqual(self.outputs(self.github_output), {"mode": "adhoc"})
        self.assertEqual(self.calls(), [])

    def test_other_branch_with_secrets_builds_ad_hoc(self):
        status, output = self.run_script("setup", ref="refs/heads/feature", ref_type="branch")
        self.assertEqual(status, 0, output)
        self.assertIn("ad hoc build, notarization skipped", output)
        self.assertIn("refs/heads/feature", output)
        self.assertEqual(self.calls(), [])

    def test_partial_secrets_on_a_release_ref_fail_and_name_what_is_missing(self):
        secrets = dict(SECRETS)
        del secrets["MACOS_NOTARY_API_KEY"]
        status, output = self.run_script("setup", secrets=secrets)
        self.assertEqual(status, 1, output)
        self.assertIn("MACOS_NOTARY_API_KEY", output)
        self.assert_no_secrets(output)
        self.assertEqual(self.calls(), [])

    # -------------------------------------------------------------------------------------------- the keychain

    def test_default_branch_with_secrets_imports_the_identity_into_a_temporary_keychain(self):
        status, output = self.run_script("setup", ref="refs/heads/master", ref_type="branch")
        self.assertEqual(status, 0, output)
        self.assertIn("release build", output)
        calls = self.calls()
        commands = [call[:2] for call in calls]
        self.assertEqual(commands, [
            ["security", "create-keychain"],
            ["security", "set-keychain-settings"],
            ["security", "unlock-keychain"],
            ["security", "import"],
            ["security", "set-key-partition-list"],
            ["security", "list-keychains"],
            ["security", "list-keychains"],
            ["security", "find-identity"],
        ])
        keychain = calls[0][-1]
        self.assertTrue(keychain.startswith(str(self.runner_temp)), keychain)
        self.assertIn(keychain, calls[6], "the temporary keychain joins the search list so codesign finds it")
        self.assertIn("/Users/runner/Library/Keychains/login.keychain-db", calls[6])
        self.assertEqual(self.outputs(self.github_output), {"mode": "release"})
        env = self.outputs(self.github_env)
        self.assertEqual(env["OPENFALLOUT_CODESIGN_IDENTITY"], IDENTITY_HASH)
        self.assertEqual(env["OPENFALLOUT_CODESIGN_TEAM_ID"], TEAM)
        # the certificate file is gone once it is imported
        self.assertEqual([path for path in self.runner_temp.rglob("*") if path.suffix == ".p12"], [])
        self.assert_no_secrets(output)
        password = calls[0][calls[0].index("-p") + 1]
        self.assertNotIn(password, output)
        self.assertNotIn(password, self.github_env.read_text())

    def test_password_with_quotes_is_not_printed(self):
        password = "it's a \"quoted\" secret $(x) 9431"
        secrets = dict(SECRETS, MACOS_DEVELOPER_ID_P12_PASSWORD=password)
        status, output = self.run_script("setup", secrets=secrets)
        self.assertEqual(status, 0, output)
        self.assertNotIn("quoted", output)
        self.assertNotIn("9431", output)
        import_call = next(call for call in self.calls() if call[:2] == ["security", "import"])
        self.assertEqual(import_call[import_call.index("-P") + 1], password)

    def test_identity_of_another_team_or_kind_is_refused(self):
        identities = f'  1) {IDENTITY_HASH} "Apple Development: Someone (OTHERTEAM1)"\n     1 valid identities found'
        status, output = self.run_script("setup", STUB_IDENTITIES=identities)
        self.assertEqual(status, 1, output)
        self.assertIn("no Developer ID Application identity", output)
        self.assertEqual(self.calls()[-1][:2], ["security", "delete-keychain"])
        self.assert_no_secrets(output)

    def test_any_error_in_setup_removes_the_keychain(self):
        # a certificate that is not base64 fails in Python (binascii.Error), after the keychain was created
        status, output = self.run_script("setup", MACOS_DEVELOPER_ID_P12_BASE64="abc")
        self.assertNotEqual(status, 0, output)
        self.assertEqual(self.calls()[0][:2], ["security", "create-keychain"])
        self.assertEqual(self.calls()[-1][:2], ["security", "delete-keychain"])
        self.assertEqual(list(self.runner_temp.iterdir()), [])
        self.assertNotIn("mode=release", self.github_output.read_text() if self.github_output.exists() else "")

    def test_the_workflow_removes_the_keychain_whatever_the_signing_step_did(self):
        # setup() may fail before it sets its output; cleanup() does nothing without a keychain of this run
        workflow = (SCRIPT.parents[2] / ".github/workflows/macos.yml").read_text()
        step = workflow.split("- name: Remove the signing keychain", 1)[1].split("- name:", 1)[0]
        self.assertIn("if: ${{ always() }}", step)
        self.assertIn("release_signing.py cleanup", step)

    def test_cleanup_deletes_the_keychain(self):
        self.run_script("setup")
        keychain = self.calls()[0][-1]
        self.log.unlink()
        status, output = self.run_script("cleanup")
        self.assertEqual(status, 0, output)
        self.assertEqual(self.calls(), [["security", "delete-keychain", keychain]])

    def test_cleanup_without_a_keychain_does_nothing(self):
        status, output = self.run_script("cleanup", secrets={})
        self.assertEqual(status, 0, output)
        self.assertEqual(self.calls(), [])

    # ---------------------------------------------------------------------------------------------- notarization

    def notarize(self, **extra):
        dmg = self.root / "build/OpenFallout-1.0-macOS-arm64.dmg"
        dmg.parent.mkdir()
        dmg.write_bytes(b"cpack image")
        env = {"OPENFALLOUT_CODESIGN_IDENTITY": IDENTITY_HASH, **extra}
        status, output = self.run_script("notarize", str(dmg), **env)
        return dmg, status, output

    def test_notarize_submits_waits_staples_the_app_and_the_image(self):
        dmg, status, output = self.notarize()
        self.assertEqual(status, 0, output)
        calls = self.calls()
        steps = [" ".join(call[:3]) if call[0] == "xcrun" else " ".join(call[:2]) for call in calls]
        self.assertEqual(steps, [
            "codesign --force",                # sign the CPack image
            "xcrun notarytool submit",         # notarize it: every app inside gets its ticket
            "hdiutil attach",
            "ditto " + calls[3][1],            # copy the volume out
            "hdiutil detach",
            "xcrun stapler staple",            # staple the app inside
            "hdiutil create",                  # the image again, with the stapled app
            "codesign --force",
            "xcrun notarytool submit",
            "xcrun stapler staple",            # staple the image
            "xcrun stapler validate",
            "xcrun stapler validate",
        ])
        submit = calls[1]
        self.assertIn("--wait", submit)
        self.assertEqual(submit[3], str(dmg))
        self.assertTrue(calls[5][-1].endswith("/OpenFallout.app"), calls[5])
        self.assertEqual(calls[6][-1], str(dmg) + ".new.dmg")
        self.assertIn("UDZO", calls[6])
        self.assertEqual(calls[6][calls[6].index("-volname") + 1], "OpenFallout-1.0-macOS-arm64")
        self.assertEqual(calls[9][-1], str(dmg))
        self.assertEqual(dmg.read_bytes(), b"new image")
        for sign in (calls[0], calls[7]):
            self.assertIn("--timestamp", sign)
            self.assertEqual(sign[sign.index("--sign") + 1], IDENTITY_HASH)
        self.assertEqual([path for path in self.runner_temp.rglob("*.p8")], [], "the notary key is removed")
        self.assert_no_secrets(output)
        self.assertIn("notarized and stapled", output)

    def test_rejected_submission_prints_the_notary_log_and_fails(self):
        dmg, status, output = self.notarize(STUB_NOTARY_STATUS="Invalid")
        self.assertEqual(status, 1, output)
        self.assertIn("Invalid", output)
        self.assertIn("The signature does not include a secure timestamp.", output)
        log_call = self.calls()[-1]
        self.assertEqual(log_call[:4], ["xcrun", "notarytool", "log", "submission-1"])
        self.assertNotIn("stapler", [call[1] for call in self.calls() if call[0] == "xcrun"])
        self.assertEqual(dmg.read_bytes(), b"cpack image", "a rejected image is left as CPack made it")
        self.assertEqual([path for path in self.runner_temp.rglob("*.p8")], [])
        self.assert_no_secrets(output)

    def test_notary_key_that_cannot_be_written_leaves_no_temporary_folder(self):
        # an undecodable byte in the key makes writing the key file fail
        dmg, status, output = self.notarize(MACOS_NOTARY_API_KEY="key \udcff")
        self.assertNotEqual(status, 0, output)
        self.assertEqual(list(self.runner_temp.glob("openfallout-notary-*")), [])

    def test_notarize_without_a_signing_identity_is_refused(self):
        dmg = self.root / "x.dmg"
        dmg.write_bytes(b"")
        status, output = self.run_script("notarize", str(dmg))
        self.assertEqual(status, 1, output)
        self.assertIn("OPENFALLOUT_CODESIGN_IDENTITY", output)
        self.assertEqual(self.calls(), [])


if __name__ == "__main__":
    unittest.main()
