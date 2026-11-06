#!/usr/bin/env python3
"""Drive the desktop release workflow from a trusted Harness checkout."""

import argparse
import json
import os
import re
import subprocess
import sys
from urllib.error import HTTPError
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen

REPOSITORY = "Elemperor1/OpenFallout"
MARKER = ".harness/openfallout-release.yaml"


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
    query = urlencode({"head_sha": sha, "event": "push", "per_page": 100})
    runs = api.request(f"actions/workflows/push.yml/runs?{query}")["workflow_runs"]
    latest = max((run for run in runs if run["head_branch"] == "master"),
                 key=lambda run: (run["run_number"], run.get("run_attempt", 1)), default=None)
    if not latest or latest["status"] != "completed" or latest["conclusion"] != "success":
        problems.append("The latest master Build and test run for this commit must pass")
    ref = api.request(f"git/ref/tags/{quote(tag, safe='')}", missing_ok=True)
    if ref and (ref["object"]["type"] != "commit" or ref["object"]["sha"] != sha):
        problems.append("Existing tag does not point directly to the selected commit; tags are never overwritten")
    return {"repository": REPOSITORY, "source_ref": source_ref, "sha": sha,
            "tag": tag, "ready": not problems, "problems": problems,
            "build_url": latest["html_url"] if latest else None, "tag_exists": bool(ref)}


def require_ready(plan):
    if not plan["ready"]:
        raise ReleaseError("; ".join(plan["problems"]))


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
    args = parser.parse_args()
    api = GitHub()
    plan = release_plan(api, args.source_ref, args.tag)
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
