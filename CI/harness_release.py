#!/usr/bin/env python3
"""Drive the desktop release workflow from a trusted Harness checkout."""

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from urllib.error import HTTPError
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen

REPOSITORY = "Elemperor1/OpenFallout"
MARKER = ".harness/openfallout-release.yaml"
PLATFORM_JOBS = {
    "Ubuntu",
    "Windows (2025) / windows-2025",
    "MacOS (macos-26) / macos-26",
    "MacOS (macos-26-intel) / macos-26-intel",
}


class ReleaseError(RuntimeError):
    pass


class GitHub:
    def __init__(self):
        self.token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
        if not self.token:
            raise ReleaseError("GH_TOKEN or GITHUB_TOKEN is required")

    def request(self, path, method="GET", data=None, missing_ok=False):
        request = Request(
            f"https://api.github.com/repos/{REPOSITORY}/{path}",
            data=json.dumps(data).encode() if data is not None else None,
            headers={
                "Authorization": f"Bearer {self.token}",
                "Accept": "application/vnd.github+json",
                "X-GitHub-Api-Version": "2022-11-28",
                "Content-Type": "application/json",
            },
            method=method,
        )
        try:
            with urlopen(request, timeout=30) as response:
                return json.load(response)
        except HTTPError as error:
            if missing_ok and error.code == 404:
                return None
            raise ReleaseError(f"GitHub {method} {path.split('?')[0]} returned HTTP {error.code}") from None


def validate_tag(tag):
    if not re.fullmatch(r"openfallout-[A-Za-z0-9][A-Za-z0-9._-]{0,100}", tag):
        raise ReleaseError("Tag must start with openfallout- and contain only letters, numbers, dots, underscores or hyphens")
    if ".." in tag or tag.endswith((".", ".lock")):
        raise ReleaseError("Tag is not a valid Git reference")


def release_plan(api, source_ref, tag):
    validate_tag(tag)
    commit = api.request(f"commits/{quote(source_ref, safe='')}")
    sha = commit["sha"]
    comparison = api.request(f"compare/master...{sha}")
    problems = []
    if comparison["status"] not in ("identical", "behind"):
        problems.append("Source commit must be on master")
    marker = api.request(f"contents/{MARKER}?ref={sha}", missing_ok=True)
    if marker is None:
        problems.append("Merge the Harness release setup into master first")
    query = urlencode({"head_sha": sha, "per_page": 100})
    runs = api.request(f"actions/workflows/push.yml/runs?{query}")["workflow_runs"]
    latest = max((run for run in runs if run["head_branch"] == "master"
                  and run["event"] in ("push", "workflow_dispatch")),
                 key=lambda run: (run["run_number"], run.get("run_attempt", 1)), default=None)
    if not latest or latest["status"] != "completed" or latest["conclusion"] != "success":
        problems.append("The latest master Build and test run for this commit must pass")
    else:
        jobs = api.request(f"actions/runs/{latest['id']}/jobs?filter=latest&per_page=100")["jobs"]
        passed = {job["name"] for job in jobs if job["conclusion"] == "success"}
        if not PLATFORM_JOBS.issubset(passed):
            problems.append("All four platform jobs must pass; run the full Build and test workflow on this master commit")
    ref = api.request(f"git/ref/tags/{quote(tag, safe='')}", missing_ok=True)
    if ref and (ref["object"]["type"] != "commit" or ref["object"]["sha"] != sha):
        problems.append("Existing tag does not point directly to the selected commit; tags are never overwritten")
    return {"repository": REPOSITORY, "source_ref": source_ref, "sha": sha,
            "tag": tag, "ready": not problems, "problems": problems,
            "build_url": latest["html_url"] if latest else None, "tag_exists": bool(ref)}


def require_ready(plan):
    if not plan["ready"]:
        raise ReleaseError("; ".join(plan["problems"]))


def plan_phase(api, phase, source_ref, tag, state_file):
    if phase == "preview":
        plan = release_plan(api, source_ref, tag)
        state_file.write_text(json.dumps({"sha": plan["sha"], "source_ref": source_ref, "tag": tag}))
        return plan
    try:
        state = json.loads(state_file.read_text())
    except (OSError, ValueError):
        raise ReleaseError("Run the preview phase first to resolve and save the source commit") from None
    if (state.get("source_ref") != source_ref or state.get("tag") != tag
            or not re.fullmatch(r"[0-9a-f]{40}", state.get("sha", ""))):
        raise ReleaseError("Saved release source is invalid or belongs to different inputs")
    return release_plan(api, state["sha"], tag)


def create_tag(api, plan):
    require_ready(plan)
    if not plan["tag_exists"]:
        api.request("git/refs", "POST", {"ref": f"refs/tags/{plan['tag']}", "sha": plan["sha"]})
    print(f"Release tag ready: {plan['tag']} at {plan['sha']}")


def packaging_run(api, plan):
    query = urlencode({"head_sha": plan["sha"], "branch": plan["tag"], "event": "push", "per_page": 100})
    runs = api.request(f"actions/workflows/release.yml/runs?{query}")["workflow_runs"]
    matching = [run for run in runs if run["head_branch"] == plan["tag"] and run["head_sha"] == plan["sha"]]
    if not matching:
        raise ReleaseError("Release workflow has not registered yet; retry this Harness step")
    return max(matching, key=lambda run: (run["run_number"], run.get("run_attempt", 1)))


def verify_assets(release):
    assets = [asset["name"] for asset in release["assets"] if asset.get("size", 0) > 0 and asset.get("state") == "uploaded"]
    if not any(name.lower().endswith(".exe") for name in assets):
        raise ReleaseError("Release is missing its Windows installer")
    for arch in ("arm64", "x86_64"):
        if not any(name.endswith(".dmg") and arch in name for name in assets):
            raise ReleaseError(f"Release is missing its macOS {arch} disk image")
    # Each disk image carries the line CI/check_mac_package.py --notes wrote: the macOS version and chip it was checked on.
    lines = (release.get("body") or "").splitlines()
    for name in (name for name in assets if name.endswith(".dmg")):
        mine = [line for line in lines if line.startswith(f"- {name}:") and " on macOS " in line]
        if not mine:
            raise ReleaseError(f"Release notes lack the macOS version and chip that {name} was tested on")
        if "arm64" not in name:
            continue
        # The notarized Apple Silicon image must have passed the check in release mode, and the game must have
        # started from it (a hosted runner skips the smoke step; the run on a Mac with a GPU before tagging adds it).
        passed = [line for line in mine if f"- {name}: packaged-app check PASS (" in line]
        if not any("(release mode;" in line for line in passed):
            raise ReleaseError(f"Release notes lack a packaged-app check PASS in release mode for {name}")
        if not any("; smoke PASS)" in line for line in passed):
            raise ReleaseError(f"Release notes lack a smoke PASS (the game started on a Mac with a GPU) for {name}")


def find_release(api, tag):
    # The tag endpoint only promises published releases. Authenticated listings
    # include drafts, including older drafts beyond the first page.
    page = 1
    while True:
        releases = api.request(f"releases?per_page=100&page={page}")
        matches = [release for release in releases if release["tag_name"] == tag]
        if matches:
            if len(matches) != 1:
                raise ReleaseError("More than one release matches the tag")
            return matches[0]
        if len(releases) < 100:
            raise ReleaseError("Packaging completed without a matching release")
        page += 1


def finish_release(api, plan, publish=False):
    require_ready(plan)
    run = packaging_run(api, plan)
    if run["status"] != "completed" or run["conclusion"] != "success":
        raise ReleaseError("Release packaging must finish successfully before verifying or publishing assets")
    release = find_release(api, plan["tag"])
    if release["tag_name"] != plan["tag"] or not release["prerelease"]:
        raise ReleaseError("Expected the selected tag's prerelease")
    verify_assets(release)
    if publish and release["draft"]:
        release = api.request(f"releases/{release['id']}", "PATCH", {"draft": False, "prerelease": True})
    print(json.dumps({"url": release["html_url"], "draft": release["draft"], "prerelease": release["prerelease"]}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("preview", "tag", "wait", "verify", "publish"))
    parser.add_argument("--source-ref", default=os.environ.get("RELEASE_SOURCE", "master"))
    parser.add_argument("--tag", default=os.environ.get("RELEASE_TAG"), required=not os.environ.get("RELEASE_TAG"))
    parser.add_argument("--state-file", type=Path, default=Path(".harness-release-state.json"))
    args = parser.parse_args()
    api = GitHub()
    plan = plan_phase(api, args.phase, args.source_ref, args.tag, args.state_file)
    if args.phase == "preview":
        print(json.dumps(plan, indent=2))
    elif args.phase == "tag":
        create_tag(api, plan)
    elif args.phase == "wait":
        require_ready(plan)
        run = packaging_run(api, plan)
        print(f"Watching {run['html_url']}", flush=True)
        subprocess.run(["gh", "run", "watch", str(run["id"]), "--repo", REPOSITORY,
                        "--exit-status", "--interval", "30"], check=True)
    else:
        finish_release(api, plan, publish=args.phase == "publish")


if __name__ == "__main__":
    try:
        main()
    except (ReleaseError, subprocess.CalledProcessError) as error:
        print(f"Release stopped: {error}", file=sys.stderr)
        sys.exit(1)
