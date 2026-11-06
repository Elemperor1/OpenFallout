import copy
import unittest
import tempfile
from pathlib import Path

from harness_release import PLATFORM_JOBS, ReleaseError, create_tag, find_release, finish_release, plan_phase, release_plan, validate_tag, verify_assets


SHA = "a" * 40
TAG = "openfallout-0.1.0-test"
RUN = {"id": 42, "head_branch": "master", "head_sha": SHA, "run_number": 1, "event": "push",
       "status": "completed", "conclusion": "success", "html_url": "https://example.invalid/run"}
RELEASE = {"id": 7, "tag_name": TAG, "draft": True, "prerelease": True,
           "html_url": "https://example.invalid/release", "assets": [
               {"name": name, "size": 12, "state": "uploaded"} for name in (
                   "OpenFallout.exe", "OpenFallout-arm64.dmg", "OpenFallout-x86_64.dmg")]}


class FakeGitHub:
    def __init__(self):
        self.writes = []
        self.comparison = "identical"
        self.marker = {}
        self.builds = [copy.deepcopy(RUN)]
        self.ref = None
        self.packaging = dict(RUN, head_branch=TAG)
        self.release = copy.deepcopy(RELEASE)
        self.release_pages = None
        self.master_sha = SHA
        self.jobs = [{"name": name, "conclusion": "success"} for name in PLATFORM_JOBS]

    def request(self, path, method="GET", data=None, missing_ok=False):
        if method != "GET":
            self.writes.append((path, method, data))
            return dict(self.release, **data) if path.startswith("releases/") else {}
        if path.startswith("commits/"):
            ref = path.split("/", 1)[1]
            return {"sha": ref if len(ref) == 40 else self.master_sha}
        if path.startswith("compare/"):
            return {"status": self.comparison}
        if path.startswith("contents/"):
            return self.marker
        if path.startswith("actions/workflows/push.yml/"):
            return {"workflow_runs": self.builds}
        if path.startswith("git/ref/"):
            return self.ref
        if path.startswith("actions/workflows/release.yml/"):
            return {"workflow_runs": [self.packaging]}
        if path.startswith("actions/runs/"):
            return {"jobs": self.jobs}
        if path.startswith("releases?"):
            if self.release_pages is not None:
                return self.release_pages[int(path.rsplit("=", 1)[1]) - 1]
            return [self.release]
        raise AssertionError(path)


class ReleaseTests(unittest.TestCase):
    def test_preview_is_read_only(self):
        api = FakeGitHub()
        self.assertTrue(release_plan(api, "master", TAG)["ready"])
        self.assertEqual(api.writes, [])

    def test_bad_tags(self):
        for tag in ("v1.0", "openfallout-../bad", "openfallout-a..b", "openfallout-a.lock", "openfallout-a."):
            with self.subTest(tag=tag), self.assertRaises(ReleaseError):
                validate_tag(tag)

    def test_unmerged_source_or_missing_setup_blocks_writes(self):
        for field, value in (("comparison", "ahead"), ("marker", None)):
            api = FakeGitHub()
            setattr(api, field, value)
            with self.assertRaises(ReleaseError):
                create_tag(api, release_plan(api, "feature", TAG))
            self.assertEqual(api.writes, [])

    def test_latest_failed_or_pending_build_blocks_release(self):
        for status, conclusion in (("completed", "failure"), ("in_progress", None)):
            api = FakeGitHub()
            api.builds.append(dict(RUN, run_number=2, status=status, conclusion=conclusion))
            with self.assertRaises(ReleaseError):
                create_tag(api, release_plan(api, "master", TAG))
            self.assertEqual(api.writes, [])

    def test_docs_only_success_with_skipped_platform_jobs_blocks_release(self):
        api = FakeGitHub()
        api.jobs[0]["conclusion"] = "skipped"
        with self.assertRaises(ReleaseError):
            create_tag(api, release_plan(api, "master", TAG))
        self.assertEqual(api.writes, [])

    def test_full_manual_master_build_can_qualify_a_docs_only_commit(self):
        api = FakeGitHub()
        api.builds = [dict(RUN, event="workflow_dispatch")]
        self.assertTrue(release_plan(api, "master", TAG)["ready"])

    def test_create_tag_once_and_never_overwrite(self):
        api = FakeGitHub()
        create_tag(api, release_plan(api, "master", TAG))
        self.assertEqual(api.writes, [("git/refs", "POST", {"ref": f"refs/tags/{TAG}", "sha": SHA})])
        api.writes.clear()
        api.ref = {"object": {"type": "commit", "sha": SHA}}
        create_tag(api, release_plan(api, "master", TAG))
        self.assertEqual(api.writes, [])
        api.ref["object"]["sha"] = "b" * 40
        with self.assertRaises(ReleaseError):
            create_tag(api, release_plan(api, "master", TAG))
        self.assertEqual(api.writes, [])
        api.ref = {"object": {"type": "tag", "sha": SHA}}
        with self.assertRaises(ReleaseError):
            create_tag(api, release_plan(api, "master", TAG))
        self.assertEqual(api.writes, [])

    def test_missing_empty_or_wrong_architecture_assets_block_publish(self):
        for index in range(3):
            release = copy.deepcopy(RELEASE)
            release["assets"][index]["size"] = 0
            with self.assertRaises(ReleaseError):
                verify_assets(release)
        release = copy.deepcopy(RELEASE)
        release["assets"][1]["name"] = "OpenFallout-unknown.dmg"
        with self.assertRaises(ReleaseError):
            verify_assets(release)

    def test_failed_packaging_blocks_publishing(self):
        api = FakeGitHub()
        api.packaging["conclusion"] = "failure"
        with self.assertRaises(ReleaseError):
            finish_release(api, release_plan(api, "master", TAG), publish=True)
        self.assertEqual(api.writes, [])

    def test_verify_keeps_draft_and_publish_only_promotes_prerelease(self):
        api = FakeGitHub()
        plan = release_plan(api, "master", TAG)
        finish_release(api, plan)
        self.assertEqual(api.writes, [])
        finish_release(api, plan, publish=True)
        self.assertEqual(api.writes, [("releases/7", "PATCH", {"draft": False, "prerelease": True})])

    def test_draft_lookup_paginates_and_missing_release_fails(self):
        api = FakeGitHub()
        api.release_pages = [[dict(RELEASE, tag_name=f"other-{i}") for i in range(100)], [RELEASE]]
        self.assertEqual(find_release(api, TAG)["id"], 7)
        api.release_pages = [[]]
        with self.assertRaises(ReleaseError):
            find_release(api, TAG)

    def test_master_advancing_does_not_change_source_between_phases(self):
        api = FakeGitHub()
        with tempfile.TemporaryDirectory() as directory:
            state_file = Path(directory) / "source.json"
            preview = plan_phase(api, "preview", "master", TAG, state_file)
            api.master_sha = "b" * 40
            api.ref = {"object": {"type": "commit", "sha": SHA}}
            for phase in ("tag", "wait", "verify", "publish"):
                plan = plan_phase(api, phase, "master", TAG, state_file)
                self.assertEqual(plan["sha"], preview["sha"])
                self.assertTrue(plan["ready"])
            self.assertEqual(api.writes, [])

    def test_later_phases_reject_missing_or_mismatched_state(self):
        with tempfile.TemporaryDirectory() as directory:
            state_file = Path(directory) / "source.json"
            api = FakeGitHub()
            with self.assertRaises(ReleaseError):
                plan_phase(api, "tag", "master", TAG, state_file)
            plan_phase(api, "preview", "master", TAG, state_file)
            with self.assertRaises(ReleaseError):
                plan_phase(api, "publish", "master", "openfallout-other", state_file)


if __name__ == "__main__":
    unittest.main()
