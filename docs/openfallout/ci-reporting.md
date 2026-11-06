# Codecov coverage flags and Test Analytics

The Ubuntu Actions job runs the components, OpenFallout, and editor test suites.
Each suite produces its own coverage report and JUnit test results. The existing
`CODECOV_TOKEN` Actions secret authenticates both coverage and Test Analytics
uploads. Windows continues to run its existing tests; these uploads describe
the Ubuntu runs.

| Flag | Executable | Coverage report | Test results |
| --- | --- | --- | --- |
| `components` | `components-tests` | `coverage/components.xml` | `test-results/components.junit.xml` |
| `openfallout` | `openfallout-tests` | `coverage/openfallout.xml` | `test-results/openfallout.junit.xml` |
| `editor` | `openfallout-cs-tests` | `coverage/editor.xml` | `test-results/editor.junit.xml` |

The flags measure which production code each test suite exercises. They are
not source-directory filters: shared production code can appear under several
flags. Test sources, third-party dependencies, and generated build files are
excluded from coverage.

`CI/test_reports.py` clears `.gcda` execution counters before and after each
suite, keeping compiled objects and `.gcno` metadata reusable. This separates
the three reports even when the build was restored from a cache. gcovr JSON
traces are merged into `coverage.xml`, which is uploaded without a flag and
retained as an artifact. This combined upload preserves complete branch data:
two suites may exercise opposite sides of a branch, while each separate XML
report only describes partial coverage. The combined upload does not affect
the individual flags.

GoogleTest XML is normalized to JUnit, preserving test names, durations,
failure details, and skipped/disabled tests. Source paths inside the checkout
become repository-relative paths. Raw GoogleTest reports are retained alongside
the JUnit files in the `ubuntu-test-results` artifact. Coverage reports and JSON
traces are retained in `ubuntu-coverage`.

A failed test still fails the job. The remaining test suites run, and available
coverage and test results are uploaded even after a test failure. Cancelled runs
stop reporting. Fork and Dependabot PRs keep their artifacts without using the
repository upload secret.

`codecov.yml` lists the three flags, disables carryforward, and waits for all
four coverage uploads before producing the coverage notification and PR comment.
There are no new per-flag coverage thresholds. Test Analytics needs successive
uploads to establish timing, failure-rate, and flakiness history.

## Source components

The Components view groups production source files independently of the test
suite flags. The component IDs in `codecov.yml` are stable so coverage history
can continue across display-name changes.

| Component | Source paths |
| --- | --- |
| OpenFallout engine | `apps/openfallout/` |
| Shared libraries | `components/` |
| Construction Set editor | `apps/opencs/` |
| Launcher and installation wizard | `apps/launcher/`, `apps/wizard/` |
| Data and navigation tools | BSA, Bullet, ESM, ESS, INI, navigation, and NIF tools under `apps/` |

These five groups cover the production files in the coverage report without
overlapping one another. They are included in PR comments and do not add new
coverage thresholds. To see this configuration in the default-branch view,
merge the PR and allow its `master` coverage run to finish. Codecov may offer
**Enable Component Analytics** to backfill historical component trends.

With an instrumented GCC build in `build/` and gcovr 8.3 on `PATH`, the same
reporting commands can be run locally:

```sh
python3 CI/test_reports.py run components
python3 CI/test_reports.py run openfallout
python3 CI/test_reports.py run editor
python3 CI/test_reports.py merge
```

References: [Codecov flags](https://docs.codecov.com/docs/flags),
[Test Analytics](https://docs.codecov.com/docs/test-analytics), and
[gcovr trace merging](https://gcovr.com/en/8.3/guide/merging.html).

## Dependabot

`.github/dependabot.yml` checks GitHub Actions, `docs/requirements.txt`, and the
Dockerfiles under `docker/` and `docs/` every Monday at 09:00 America/New_York.
GitHub Actions updates are grouped into one PR, including major versions.
Documentation Python updates are also grouped so related Sphinx dependencies
are validated together. Sphinx 9 and Docutils 0.22 and later are held back until
the OpenMW theme supports them. Docker minor and patch updates remain grouped.
Each ecosystem permits up to five open version-update PRs. Dependabot starts
using this configuration after it reaches the default branch.

CMake FetchContent pins and packages installed by shell commands, such as the
inline gcovr version, still need manual updates. The current Docker base images
use floating tags, so they may not produce version-update PRs until pinned.
