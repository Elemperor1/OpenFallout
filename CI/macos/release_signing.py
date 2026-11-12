#!/usr/bin/env python3
"""Developer ID signing and notarization of the macOS disk image in CI.

    CI/macos/release_signing.py setup            decide release or ad hoc; for a release, import the identity
    CI/macos/release_signing.py notarize DMG     notarize and staple the disk image and the apps inside it
    CI/macos/release_signing.py cleanup          delete the temporary keychain

A release build needs a tag or the default branch (not a pull request) AND the signing secrets. Anything else is the
ad hoc build: `setup` prints "ad hoc build, notarization skipped: <why>", writes mode=adhoc to $GITHUB_OUTPUT and exits
0. A run that has some of the secrets but not all on a release ref fails, naming the missing ones.

setup (release): the Developer ID certificate (.p12, base64 in MACOS_DEVELOPER_ID_P12_BASE64, password in
MACOS_DEVELOPER_ID_P12_PASSWORD) is imported into a new keychain under $RUNNER_TEMP with a random password; the keychain
joins the user search list so that codesign finds it. The identity of team MACOS_TEAM_ID is handed to the packaging step
as OPENFALLOUT_CODESIGN_IDENTITY (its SHA-1) and OPENFALLOUT_CODESIGN_TEAM_ID through $GITHUB_ENV
(cmake/SignMacApplications.cmake reads them), and mode=release goes to $GITHUB_OUTPUT.

notarize: signs the CPack disk image, submits it with `notarytool submit --wait` (App Store Connect API key:
MACOS_NOTARY_API_KEY, MACOS_NOTARY_API_KEY_ID, MACOS_NOTARY_API_ISSUER_ID), which notarizes every app inside it; copies
the volume out, staples each app, builds the image again from the copy, signs and submits that, and staples it. A
rejected submission prints the notary log and fails. `stapler validate` checks the image and the apps at the end.

Secret values are never printed: the commands are echoed with them replaced by ***. The certificate and key files exist
only while a command reads them.
"""
import base64
import json
import os
from pathlib import Path
import re
import shlex
import secrets
import shutil
import subprocess
import sys
import tempfile

SIGNING_SECRETS = ("MACOS_DEVELOPER_ID_P12_BASE64", "MACOS_DEVELOPER_ID_P12_PASSWORD", "MACOS_TEAM_ID")
NOTARY_SECRETS = ("MACOS_NOTARY_API_KEY", "MACOS_NOTARY_API_KEY_ID", "MACOS_NOTARY_API_ISSUER_ID")
ALL_SECRETS = SIGNING_SECRETS + NOTARY_SECRETS
SKIPPED = "ad hoc build, notarization skipped"
STATE = "openfallout-signing.json"


class Failed(Exception):
    pass


def secret_values():
    return [os.environ[name] for name in ALL_SECRETS if os.environ.get(name)]


def redact(text, hidden):
    for value in sorted(hidden, key=len, reverse=True):
        if value:
            text = text.replace(value, "***")
    return text


def run(command, hidden=(), check=True, quiet=False):
    """Runs a command, echoing it with secret values (the environment's and those in `hidden`) replaced by ***."""
    hidden = [*secret_values(), *hidden]
    command = [str(part) for part in command]
    print("+ " + redact(shlex.join(command), hidden), flush=True)
    result = subprocess.run(command, capture_output=True, text=True)
    if not quiet:
        for stream in (result.stdout, result.stderr):
            if stream.strip():
                print(redact(stream.rstrip(), hidden), flush=True)
    if check and result.returncode != 0:
        raise Failed(f"{command[0]} {command[1] if len(command) > 1 else ''} exited with {result.returncode}")
    return result


def runner_temp():
    return Path(os.environ.get("RUNNER_TEMP") or tempfile.gettempdir())


def append(variable, name, value):
    path = os.environ.get(variable)
    if path:
        with open(path, "a", encoding="utf-8") as file:
            file.write(f"{name}={value}\n")


# ------------------------------------------------------------------------------------------------------------- gate

def release_ref_problem():
    """None when this run may sign a release, otherwise why not."""
    event = os.environ.get("GITHUB_EVENT_NAME", "")
    ref = os.environ.get("GITHUB_REF", "")
    default = os.environ.get("OPENFALLOUT_DEFAULT_BRANCH") or "master"
    if event.startswith("pull_request"):
        return "a pull request build"
    if os.environ.get("GITHUB_REF_TYPE") == "tag" or ref.startswith("refs/tags/"):
        return None
    if ref == f"refs/heads/{default}":
        return None
    return f"{ref or 'this ref'} is neither a tag nor the default branch ({default})"


def decide():
    """'release' or 'adhoc' with the reason; raises when a release ref has only some of the secrets."""
    present = [name for name in ALL_SECRETS if os.environ.get(name)]
    problem = release_ref_problem()
    if problem:
        return "adhoc", problem
    if not present:
        return "adhoc", "no signing secrets in this run (forks and repositories without them build ad hoc)"
    missing = [name for name in ALL_SECRETS if name not in present]
    if missing:
        raise Failed(f"release signing has only some of its secrets; missing: {', '.join(missing)}")
    return "release", None


# --------------------------------------------------------------------------------------------------------- keychain

def state_path():
    return runner_temp() / STATE


def find_identity(keychain, team):
    listing = run(["security", "find-identity", "-v", "-p", "codesigning", keychain]).stdout
    for sha1, name in re.findall(r'^\s*\d+\)\s+([0-9A-F]{40})\s+"([^"]+)"', listing, re.MULTILINE):
        if name.startswith("Developer ID Application:") and name.endswith(f"({team})"):
            return sha1
    raise Failed("the certificate holds no Developer ID Application identity of team MACOS_TEAM_ID")


def setup():
    mode, why = decide()
    if mode == "adhoc":
        print(f"{SKIPPED}: {why}")
        append("GITHUB_OUTPUT", "mode", "adhoc")
        return
    folder = Path(tempfile.mkdtemp(prefix="openfallout-signing-", dir=runner_temp()))
    keychain = folder / "openfallout-signing.keychain-db"
    password = secrets.token_hex(24)
    state_path().write_text(json.dumps({"keychain": str(keychain)}))
    certificate = folder / "developer-id.p12"
    try:
        run(["security", "create-keychain", "-p", password, keychain], hidden=[password])
        run(["security", "set-keychain-settings", "-lut", "21600", keychain])
        run(["security", "unlock-keychain", "-p", password, keychain], hidden=[password])
        certificate.touch(mode=0o600)
        certificate.write_bytes(base64.b64decode(os.environ["MACOS_DEVELOPER_ID_P12_BASE64"]))
        try:
            run(["security", "import", certificate, "-k", keychain, "-f", "pkcs12",
                 "-P", os.environ["MACOS_DEVELOPER_ID_P12_PASSWORD"], "-T", "/usr/bin/codesign"])
        finally:
            certificate.unlink()
        run(["security", "set-key-partition-list", "-S", "apple-tool:,apple:,codesign:", "-s", "-k", password,
             keychain], hidden=[password], quiet=True)
        listed = run(["security", "list-keychains", "-d", "user"]).stdout
        search = [line.strip().strip('"') for line in listed.splitlines() if line.strip()]
        run(["security", "list-keychains", "-d", "user", "-s", keychain, *search])
        identity = find_identity(keychain, os.environ["MACOS_TEAM_ID"])
    except Failed:
        cleanup()
        raise
    append("GITHUB_ENV", "OPENFALLOUT_CODESIGN_IDENTITY", identity)
    append("GITHUB_ENV", "OPENFALLOUT_CODESIGN_TEAM_ID", os.environ["MACOS_TEAM_ID"])
    append("GITHUB_OUTPUT", "mode", "release")
    print("release build: Developer ID identity imported into a temporary keychain; the package will be signed with "
          "it, notarized and stapled")


def cleanup():
    path = state_path()
    if not path.exists():
        return
    keychain = Path(json.loads(path.read_text())["keychain"])
    run(["security", "delete-keychain", keychain], check=False)
    shutil.rmtree(keychain.parent, ignore_errors=True)
    path.unlink()


# ----------------------------------------------------------------------------------------------------- notarization

class Notary:
    def __init__(self, folder):
        self.key = folder / "notary-key.p8"
        self.key.touch(mode=0o600)
        self.key.write_text(os.environ["MACOS_NOTARY_API_KEY"])
        self.credentials = ["--key", self.key, "--key-id", os.environ["MACOS_NOTARY_API_KEY_ID"],
                            "--issuer", os.environ["MACOS_NOTARY_API_ISSUER_ID"]]

    def submit(self, path):
        result = run(["xcrun", "notarytool", "submit", path, *self.credentials, "--wait", "--timeout", "2h",
                      "--output-format", "json"], check=False)
        try:
            outcome = json.loads(result.stdout)
        except ValueError:
            raise Failed(f"notarytool submit of {path.name} failed (exit {result.returncode}) without a result")
        status, submission = outcome.get("status"), outcome.get("id")
        if status != "Accepted":
            print(f"notarization of {path.name} was not accepted: {status}; the notary log follows")
            if submission:
                run(["xcrun", "notarytool", "log", submission, *self.credentials], check=False)
            raise Failed(f"notarization {status or 'failed'} for {path.name}")
        print(f"notarization of {path.name} accepted (submission {submission})")

    def close(self):
        self.key.unlink(missing_ok=True)


def sign_image(dmg, identity):
    run(["codesign", "--force", "--sign", identity, "--timestamp", dmg])


def notarize(dmg):
    identity = os.environ.get("OPENFALLOUT_CODESIGN_IDENTITY")
    if not identity or identity == "-":
        raise Failed("notarize needs the Developer ID identity in OPENFALLOUT_CODESIGN_IDENTITY (run setup first)")
    missing = [name for name in NOTARY_SECRETS if not os.environ.get(name)]
    if missing:
        raise Failed(f"notarize needs {', '.join(missing)}")
    dmg = Path(dmg).absolute()
    folder = Path(tempfile.mkdtemp(prefix="openfallout-notary-", dir=runner_temp()))
    notary = Notary(folder)
    try:
        sign_image(dmg, identity)
        notary.submit(dmg)
        mountpoint, stage = folder / "mount", folder / "stage"
        mountpoint.mkdir()
        run(["hdiutil", "attach", dmg, "-readonly", "-nobrowse", "-noautoopen", "-mountpoint", mountpoint])
        try:
            run(["ditto", mountpoint, stage])
        finally:
            run(["hdiutil", "detach", mountpoint, "-force"], check=False)
        apps = sorted(stage.glob("*.app"))
        if not apps:
            raise Failed(f"{dmg.name} holds no app")
        for app in apps:
            run(["xcrun", "stapler", "staple", app])
        rebuilt = dmg.with_name(dmg.name + ".new.dmg")
        run(["hdiutil", "create", "-volname", dmg.stem, "-srcfolder", stage, "-format", "UDZO", "-ov", rebuilt])
        os.replace(rebuilt, dmg)
        sign_image(dmg, identity)
        notary.submit(dmg)
        run(["xcrun", "stapler", "staple", dmg])
        for item in (dmg, *apps):
            run(["xcrun", "stapler", "validate", item])
    finally:
        notary.close()
        shutil.rmtree(folder, ignore_errors=True)
    print(f"{dmg.name}: notarized and stapled (the image and {', '.join(app.name for app in apps)})")


def main(argv):
    if not argv or argv[0] not in ("setup", "notarize", "cleanup") or (argv[0] == "notarize") != (len(argv) == 2):
        print(__doc__.split("\n\n")[1], file=sys.stderr)
        return 2
    try:
        if argv[0] == "setup":
            setup()
        elif argv[0] == "notarize":
            notarize(argv[1])
        else:
            cleanup()
    except Failed as error:
        print(f"release signing failed: {redact(str(error), secret_values())}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
