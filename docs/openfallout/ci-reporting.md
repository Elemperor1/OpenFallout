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
traces are merged into `coverage.xml` for a combined artifact; Codecov merges
the three flagged uploads into the overall coverage report.

GoogleTest XML is normalized to JUnit, preserving test names, durations,
failure details, and skipped/disabled tests. Source paths inside the checkout
become repository-relative paths. Raw GoogleTest reports are retained alongside
the JUnit files in the `ubuntu-test-results` artifact. Coverage reports and JSON
traces are retained in `ubuntu-coverage`.

A failed test still fails the job. The remaining test suites run, and available
coverage and test results are uploaded even after a test failure. Cancelled runs
stop reporting. Fork and Dependabot PRs keep their artifacts without using the
repository upload secret.

`codecov.yml` lists the three flags, disables carryforward, and waits for three
coverage uploads before producing the coverage notification and PR comment.
There are no new per-flag coverage thresholds. Test Analytics needs successive
uploads to establish timing, failure-rate, and flakiness history.

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
Minor and patch updates are grouped per ecosystem; major updates get separate
PRs. Each ecosystem permits up to five open version-update PRs. Dependabot
starts using this configuration after it reaches the default branch.

CMake FetchContent pins and packages installed by shell commands, such as the
inline gcovr version, still need manual updates. The current Docker base images
use floating tags, so they may not produce version-update PRs until pinned.
