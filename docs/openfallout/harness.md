# Harness delivery pipelines

OpenFallout uses the existing **OpenBethesda** project in Harness, organization
`default`, account `Vti98EjbSdmr0xdtCsVP_w`:

<https://app.harness.io/ng/account/Vti98EjbSdmr0xdtCsVP_w/all/orgs/default/projects/OpenBethesda>

The pipelines are stored inline in Harness. The `.harness/` files are versioned
copies for review and recovery; merging them does **not** automatically update
the inline definitions. Apply reviewed changes in Harness too.

## Build and test

`openfallout_ci` supplements the Windows/macOS/Linux GitHub Actions matrix with
an Ubuntu 24.04 container build on Harness Cloud. The host VM remains on Harness's
GA Ubuntu 22.04 image; Docker provides the same Ubuntu generation used by GitHub
Actions. `CI/harness_ci.sh` installs the repository's Debian dependencies and Qt
translation tools, builds in Debug with eight compiler jobs, and runs all three
GoogleTest suites with JUnit ingestion.

The build driver is fetched from the immutable GitHub commit pinned in the
pipeline YAML, so source branches predating this setup can build too. When
changing `CI/harness_ci.sh`, push the reviewed driver first, then update that pin
in the versioned and inline pipeline definitions.

The `.ccache` compiler cache is separate from GitHub Actions caches. Master builds
may save it; PR builds only restore it. Build output and test results are not
cached. `preflight_only=true` checks checkout and the VM without compiling.
An initial native cache miss can appear as an ignored cache-restore failure.

## Source and supply-chain security

`openfallout_security` runs native Semgrep SAST, Gitleaks with redacted output,
and SSCA's cdxgen CycloneDX generation. All three completed successfully on the
initial master baseline, and the 38-component SBOM and its scorecard were uploaded to Harness.
Static C++ inventory is an initial dependency view; it is not a complete inventory
of every linked platform library or shipped installer.

Scans initially report findings without a new severity gate. The initial STO
baseline contained 27 active deduplicated findings: nine high, nine medium, and
nine low. A successful scan means ingestion worked; it does not mean zero findings.
Review findings in the pipeline execution's **Security** tab before selecting a
blocking threshold.

Both CI and Security have enabled GitHub webhook triggers for master pushes and
non-draft, same-repository pull requests targeting master. PR updates abort the
previous execution. Fork PRs are excluded. The registered GitHub webhook was
verified active with a successful response. Trigger definitions live under
`.harness/triggers/`.

## Desktop releases

`openfallout_release` is manual. It orchestrates the existing GitHub Actions
`Release` workflow, which builds the Windows installer and both macOS disk images.
The workflow accepts `openfallout-*` tags as well as its existing `openmw-**` tags.

Run it with these variables:

| Variable | Values | Default |
| --- | --- | --- |
| `mode` | `preview`, `draft`, `publish` | `preview` |
| `source_ref` | Tested commit on master; use an immutable SHA | `master` |
| `tag` | Unique `openfallout-` tag, e.g. `openfallout-0.1.0-rc1` | Required |

`preview` only reads GitHub and prints readiness, source SHA, CI link, and blockers.
It saves that SHA in an ephemeral workspace file, so later phases keep using the
same source even if master advances while packaging runs.
The hosted preview was tested successfully. It correctly blocks release creation
until this setup is merged into master. No tag or release is created by preview.

`draft` requires the source to be on master, this setup to exist at that commit,
and its latest master **Build and test** push run to have passed. It creates a
lightweight tag, waits for release packaging through `gh run watch`, and verifies
nonempty uploaded Windows `.exe`, macOS `arm64.dmg`, and macOS `x86_64.dmg` assets.
GitHub Actions leaves the release as a draft prerelease. Harness retries the wait
step while GitHub registers the new tag's workflow run.

`publish` performs the same checks, then promotes the verified draft to a public
**prerelease**. Running either mode against an existing matching tag resumes the
process; it never moves an existing tag. A tag for another commit is rejected.
Failed packaging must be repaired and rerun in GitHub Actions before promotion.
Draft creation and publishing were unit-tested, but no real version was published
during setup. Use the same immutable `source_ref` SHA when resuming a release.

The release pipeline checks out an immutable commit containing
`CI/harness_release.py`. Its application source is selected separately, preventing
a runtime source ref from replacing the privileged release script. When changing
the helper, push and review it first, then update the pipeline's pinned commit.
Credentials use the existing Harness GitHub OAuth secret by reference only;
no token value is stored in Git. CI/Security do not persist checkout credentials.

## Local checks

```sh
python3 -m unittest discover -s CI -p 'test_*.py'
actionlint .github/workflows/push.yml .github/workflows/release.yml
```

Release tests exercise read-only preview, invalid tags, failed or unmerged source
rejection, tag immutability, draft lookup, asset requirements, and promotion.
The lightweight GitHub Actions Changes job also runs the release helper tests.
