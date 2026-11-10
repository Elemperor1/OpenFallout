# Sourced by CI/harness_ci.sh before its first apt-get call; it defines `apt_update_with_fallback`.
#
# The Harness build VMs lost their route to archive.ubuntu.com port 80 while security.ubuntu.com kept working, so every
# `apt-get update` in the Ubuntu 24.04 container failed and the build never started. security.ubuntu.com serves the whole
# archive (the noble, noble-updates and noble-backports suites and the pools), so a run that cannot reach the default
# mirror takes the next one of the list instead of failing, and every apt call of the run retries a failed download.
#
# APT_ETC and APT_GET are there for the tests of this file (CI/test_apt_mirrors.py).

APT_ETC="${APT_ETC:-/etc/apt}"
APT_GET="${APT_GET:-apt-get}"
APT_FALLBACK_MIRRORS=(
  "http://security.ubuntu.com/ubuntu"
  "http://us-central1.gce.archive.ubuntu.com/ubuntu"
  "http://azure.archive.ubuntu.com/ubuntu"
)

# A download that fails is tried again, and a host that does not answer is given up on after 30 seconds, for every apt
# call of the run (CI/install_debian_deps.sh has its own loops around apt-get update and add-apt-repository).
apt_retry_settings() {
  mkdir -p "$APT_ETC/apt.conf.d"
  printf '%s\n' \
    'Acquire::Retries "5";' \
    'Acquire::http::Timeout "30";' \
    'Acquire::https::Timeout "30";' >"$APT_ETC/apt.conf.d/80-ci-retries"
}

# Point every source that names an archive.ubuntu.com host (the default mirror, or a country's) at the mirror given. The
# sources are always made from the copy taken before the first change (`.ci-orig`, which apt does not read), so a
# second mirror is never a rewrite of the first.
apt_use_mirror() {
  local mirror="$1" file escaped
  # The mirror goes into the replacement of sed, where the delimiter, & and \ mean something
  escaped=$(printf '%s' "$mirror" | sed -e 's/[\\&#]/\\&/g')
  for file in "$APT_ETC/sources.list" "$APT_ETC"/sources.list.d/*.sources "$APT_ETC"/sources.list.d/*.list; do
    [ -f "$file" ] || continue
    [ -f "$file.ci-orig" ] || cp "$file" "$file.ci-orig"
    sed -E "s#https?://([a-z0-9-]+\.)*archive\.ubuntu\.com/ubuntu/?#${escaped}/#g" "$file.ci-orig" >"$file"
  done
}

# `apt-get update` ends with status 0 when only some of the indexes could not be downloaded (it prints "Failed to fetch"
# and goes on), which would leave the unreachable archive in use: with Update::Error-Mode "any" every failed index is an
# error, so the next mirror is tried.
apt_update_with_fallback() {
  local mirror
  apt_retry_settings
  if "$APT_GET" -o APT::Update::Error-Mode=any update; then
    return 0
  fi
  for mirror in "${APT_FALLBACK_MIRRORS[@]}"; do
    echo "apt-get update failed; trying the mirror $mirror" >&2
    apt_use_mirror "$mirror"
    if "$APT_GET" -o APT::Update::Error-Mode=any update; then
      return 0
    fi
  done
  echo "apt-get update failed on every mirror" >&2
  return 1
}
