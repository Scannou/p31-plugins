#!/usr/bin/env bash
# Release notes for a tag, as Markdown on stdout. Usage: scripts/release-notes.sh <tag> [summary|commits]
# summary: the tag's section of CHANGELOG.md (fails when there is none).
# commits: every commit since the previous v* tag and a compare link (needs the tags fetched).
# With no second argument, both.
set -euo pipefail
cd "$(dirname "$0")/.."

TAG=${1:?usage: scripts/release-notes.sh <tag> [summary|commits]}
PART=${2:-all}
VERSION=${TAG#v}
REPO=${GITHUB_REPOSITORY:-Scannou/p31-plugins}

summary() {
    # From "## [<version>]" to the next "## [" heading or the link list, without
    # the heading itself and without blank lines at either end. A list item's
    # wrapped lines are joined: GitHub shows every line break in release notes.
    local section
    section=$(awk -v head="## [$VERSION]" '
        index($0, "## [") == 1 { if (on) exit; on = index($0, head) == 1; next }
        /^\[[^]]+\]: / { if (on) exit }
        on' CHANGELOG.md | awk '
        /^  [^ ]/ && held { line = line " " substr($0, 3); next }
        { if (held) print line; line = $0; held = 1 }
        END { if (held) print line }' | sed -e '/./,$!d' | sed -e ':a' -e '/^\n*$/{$d;N;ba' -e '}')
    if [[ -z "$section" ]]; then
        echo "CHANGELOG.md has no section for $VERSION" >&2
        return 1
    fi
    printf '%s\n' "$section"
}

commits() {
    local previous
    previous=$(git describe --tags --abbrev=0 --match 'v*' "$TAG^" 2>/dev/null || true)
    echo "## Commits"
    echo
    git log --reverse --no-merges --format='- %s (%h)' "${previous:+$previous..}$TAG"
    if [[ -n "$previous" ]]; then
        echo
        echo "**Full changelog**: https://github.com/$REPO/compare/$previous...$TAG"
    fi
}

case $PART in
    summary) summary ;;
    commits) commits ;;
    all) summary; echo; commits ;;
    *) echo "unknown part: $PART" >&2; exit 2 ;;
esac
