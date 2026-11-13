# Releasing the macOS build: Developer ID signing and notarization

A tag (`openfallout-*`, `release.yml`) or a push to the default branch (`push.yml`) builds the Apple Silicon disk image
signed with the project's Developer ID, notarized by Apple and stapled, so that a player can open it without a
Gatekeeper warning, also offline (spec #166, stories 1, 2, 19 to 22 and 35). The Intel image keeps the ad hoc signature.

Nothing of this runs without the secrets below. Pull requests (from forks or not), other branches and any run without
the secrets build the ad hoc package as before; the log says `ad hoc build, notarization skipped: <why>` and the job
stays green. A run on a tag or the default branch that has only some of the secrets fails and names the missing ones.

## What CI does (`.github/workflows/macos.yml`, `CI/macos/release_signing.py`)

1. **Release signing** (`release_signing.py setup`): decides release or ad hoc. For a release it creates a keychain
   under the runner's temporary folder with a random password, imports the certificate into it, adds it to the search
   list and hands the Developer ID Application identity of `MACOS_TEAM_ID` to the packaging step as
   `OPENFALLOUT_CODESIGN_IDENTITY` and `OPENFALLOUT_CODESIGN_TEAM_ID` (read by `cmake/SignMacApplications.cmake`, which
   signs inside out with the hardened runtime, a secure timestamp and `files/mac/*.entitlements`).
2. **Package disk image**: CPack signs and builds the image as usual.
3. **Notarize and staple** (`release_signing.py notarize <dmg>`): signs the image, submits it with
   `xcrun notarytool submit --wait` (this notarizes every app inside it), copies the volume out, staples each app,
   builds the image again from that copy, signs and submits it again, staples it, and runs `stapler validate` on the
   image and the apps. A rejected submission prints the notary log (`notarytool log`) and fails the job.
4. **Remove the signing keychain** (`release_signing.py cleanup`, also when an earlier step failed).
5. **Check the packaged app** in release mode: `CI/check_mac_package.py --mode release` requires a Developer ID
   Application signature of the one team, the hardened runtime and a secure timestamp on every Mach-O file and bundle,
   no `get-task-allow`, Gatekeeper's acceptance (`spctl --assess --type open --context context:primary-signature` for
   the image, `spctl --assess --type execute` for the app) and a stapled ticket on both. Ad hoc builds are checked in
   local mode.
6. **Release notes**: the check appends one line per disk image to the draft release (`--notes`): the result, the mode,
   the smoke result, the OpenGL renderer the smoke run reported, the image's SHA-256 and the macOS version and chip it
   ran on, e.g. `- OpenFallout-0.52.0-macOS-arm64.dmg: packaged-app check PASS (release mode; smoke SKIP; renderer not
   reported; sha256 3f2a...) on macOS 26.1 (25B78), Apple M2 (Virtual), arm64. For ...`. `CI/harness_release.py`
   refuses to publish a release whose disk images lack such a line with both a macOS version and a chip. For the Apple
   Silicon image it counts only lines whose `sha256` equals the digest GitHub reports for the uploaded asset (the
   `digest` field of the release asset, `sha256:<hex>`; a release asset without one blocks publishing), and among those
   it needs a `packaged-app check PASS (release mode; ...)` line and a `PASS` line with `smoke PASS` and a GPU renderer
   (not `Apple Software Renderer`, not `not reported`): the game must have started from that very image on a Mac with a
   GPU (both may be the same line). A smoke run of a locally built package does not count, even with the same file
   name. When the smoke step was skipped on the runner (no OpenGL context, or only Apple's software renderer: the hosted
   arm64 runner has no GPU, and on 12 November 2026 the engine ended there with a bus error about 9.5 s after the cell
   started, while a package built from the same code on a Mac with a GPU passed; the cause on the runner is not known),
   download the Apple Silicon disk image from the draft release, run
   `CI/check_mac_package.py <dmg> --mode release --smoke run --notes notes.md` on a Mac with a display, and add its line
   to the release notes before publishing.

Secret values never reach the log: the scripts echo every command with them replaced by `***` (GitHub masks them as
well), the certificate and API key exist as files only while a command reads them, and nothing under the runner's
temporary folder is uploaded. The secrets are passed only to the two steps that need them, and are blanked for
`pull_request` events.

## The secrets the owner adds

Keep them in a GitHub **environment**, not as repository secrets. A repository secret is handed to any workflow run
that someone with push access can start, including a tag on a commit nobody reviewed, and that run executes its own
copy of the signing scripts. An environment's rules are applied by GitHub before the job starts and cannot be changed
from the pushed commit.

1. Repository settings, Environments, New environment, name `macos-release` (the name the workflows use for the arm64
   build of a tag or of the default branch; every other macOS job has no environment).
2. Deployment branches and tags: Selected branches and tags; add the default branch and the tag pattern
   `openfallout-*`.
3. Optional, and the strongest guard: Required reviewers. Each signing job then waits for an approval, also on every
   push to the default branch.
4. Add the six secrets below as **environment secrets** of `macos-release`.
5. Repository settings, Rules, Rulesets: a tag ruleset for `openfallout-*` that restricts who may create, update and
   delete those tags.

The workflows still read repository secrets of the same names when the environment has none, so that works too, without
the protection above.

| Secret | What it holds |
| --- | --- |
| `MACOS_DEVELOPER_ID_P12_BASE64` | the Developer ID Application certificate with its private key, exported as .p12, base64 encoded |
| `MACOS_DEVELOPER_ID_P12_PASSWORD` | the password the .p12 was exported with |
| `MACOS_TEAM_ID` | the Apple Developer Team ID (10 characters, shown in the certificate name and on developer.apple.com, Membership) |
| `MACOS_NOTARY_API_KEY` | the App Store Connect API key file (`AuthKey_<id>.p8`), its full text |
| `MACOS_NOTARY_API_KEY_ID` | that key's Key ID |
| `MACOS_NOTARY_API_ISSUER_ID` | the Issuer ID shown above the key list |

How to make them (needs a paid Apple Developer Program membership; the Account Holder creates the Developer ID
certificate):

1. **Certificate.** In Xcode, Settings, Accounts, Manage Certificates, add a *Developer ID Application* certificate (or
   create one on developer.apple.com, Certificates, from a certificate signing request made in Keychain Access). In
   Keychain Access, under My Certificates, select `Developer ID Application: <name> (<team id>)` together with its
   private key, Export, format .p12, with a strong password. Then `base64 -i DeveloperID.p12 | pbcopy` and paste it as
   `MACOS_DEVELOPER_ID_P12_BASE64`; the password goes into `MACOS_DEVELOPER_ID_P12_PASSWORD`. Delete the .p12 file
   afterwards.
2. **Team ID** into `MACOS_TEAM_ID`.
3. **Notary credential.** On App Store Connect, Users and Access, Integrations, App Store Connect API, Team Keys,
   generate a key with the *Developer* role. Download the .p8 (it can be downloaded once), paste its whole text into
   `MACOS_NOTARY_API_KEY`, its Key ID into `MACOS_NOTARY_API_KEY_ID` and the Issuer ID into
   `MACOS_NOTARY_API_ISSUER_ID`.

To check the credentials locally before the first tag: `xcrun notarytool history --key AuthKey_<id>.p8 --key-id <id>
--issuer <issuer>` should list (possibly no) submissions instead of an authentication error.

## Checking a release package locally

    CI/check_mac_package.py OpenFallout-<version>-macOS-arm64.dmg --mode release --smoke run

`--team-id <id>` (or `OPENFALLOUT_CODESIGN_TEAM_ID`) pins the team; without it, the app's own Team ID is the one every
file must carry. See [playing.md](playing.md) for the steps of the check and [mac-luajit.md](mac-luajit.md) for the
JIT entitlement.

## Homebrew cask: not part of this work

A Homebrew cask (spec #166, stories 17 and 32) is deferred. It waits for the first notarized tagged release, because a
cask must point at a published, notarized disk image and its checksum, and it will live in a tap that the project owns
(not in Homebrew's main cask repository). Nothing in this repository builds or updates a cask yet.

## What has run for real, and what has not

On 13 November 2026 the release path ran once on a Mac with a GPU (Apple M3 Pro, macOS 27.2.1), with a real Developer
ID Application certificate and a real App Store Connect team key, on the apps of the Apple Silicon disk image that CI
built for 67089ed566:

- `release_signing.py setup` imported the certificate into a temporary keychain and found the identity of the team;
  `cleanup` removed the keychain and left the keychain search list as it was.
- `cmake/SignMacApplications.cmake` signed both apps with that identity (run as a script on the apps copied out of the
  image, not from inside CPack) and its own verification passed.
- `release_signing.py notarize` had both submissions accepted by Apple, stapled the two apps and the image, and
  `stapler validate` passed for all three.
- `CI/check_mac_package.py --mode release --smoke run` passed every step: Developer ID of the one team, hardened
  runtime and secure timestamp on 76 Mach-O files and 6 nested bundles, Gatekeeper's `Notarized Developer ID` for the
  image and the app, stapled tickets, and the game started from the image with the LuaJIT compiler on.
- `CI/harness_release.py` accepted the line that check wrote for the image's SHA-256. Earlier the same day it was run
  against a real draft release with the ad hoc images: GitHub reports a `sha256:` digest for the assets of a draft,
  equal to the local hash, and the gate refused that release for lacking a release-mode line.

Not run yet: the workflows themselves with the secrets (the `macos-release` environment, CPack calling the signing
script in the job, the notes line appended to the draft release, publishing through the gate). The first tag or push
to the default branch with the secrets is the first run of those. The unit tests with stand-in tools stay
(`CI/test_release_signing.py`, `CI/test_check_mac_package.py`). Two macOS jobs append their notes lines to the same
draft release; if they finish at the same moment, one line can be lost, and `harness_release.py` then refuses to publish
until it is added by hand.
