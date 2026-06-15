#!/usr/bin/env bash
#
# Post a firmware release announcement to a Discord channel webhook.
# Shared by the release workflow and the manual "announce" workflow.
#
# Required env:
#   WEBHOOK  Discord channel webhook URL
#   TAG      release tag, e.g. v1.2.0 or v1.2.0-beta.1
# Optional env:
#   REPO     owner/name for the release link (default: DriftKingTW/Schnell-BLE-Keypad)
#   NOTE     overrides the announcement text; otherwise the annotated tag
#            message is used, falling back to a generic line.
#
set -euo pipefail

: "${WEBHOOK:?WEBHOOK not set}"
: "${TAG:?TAG not set}"
REPO="${REPO:-DriftKingTW/Schnell-BLE-Keypad}"

case "$TAG" in
  *beta*|*rc*) CHANNEL="Beta"; SLUG="beta"; COLOR=16753920 ;;
  *)           CHANNEL="Stable"; SLUG="stable"; COLOR=5763719 ;;
esac

RELEASE_URL="https://github.com/$REPO/releases/tag/$TAG"
# ?channel= preselects the matching build in the installer dropdown.
INSTALLER_URL="https://blog.driftking.tw/Schnell-Keypad-Configuration-Tool/?channel=$SLUG"

if [ -n "${NOTE:-}" ]; then
  DESC="$NOTE"
else
  # Use the annotated tag message (git tag -a <tag> -m "...") as a short,
  # user-facing summary; lightweight tags have no message -> generic line.
  DESC="A new **$CHANNEL** firmware build is available."
  # actions/checkout leaves the tag as a lightweight ref pointing at the commit,
  # so fetch the real (annotated) tag object before reading it.
  git fetch --force origin "refs/tags/$TAG:refs/tags/$TAG" 2>/dev/null || true
  if [ "$(git cat-file -t "$TAG" 2>/dev/null)" = "tag" ]; then
    MSG="$(git tag -l "$TAG" --format='%(contents:subject)%0a%0a%(contents:body)' \
      | sed '/-----BEGIN PGP SIGNATURE-----/,$d')"
    MSG="$(printf '%s' "$MSG" | sed -e 's/[[:space:]]*$//')"
    if [ -n "$(printf '%s' "$MSG" | tr -d '[:space:]')" ]; then
      DESC="$MSG"
    fi
  fi
fi
echo "Discord announcement description:"; printf '%s\n' "$DESC"

payload=$(jq -n \
  --arg title "⌨️ Schnell Keypad firmware $TAG ($CHANNEL)" \
  --arg desc "$DESC" \
  --arg rel "$RELEASE_URL" \
  --arg inst "$INSTALLER_URL" \
  --argjson color "$COLOR" \
  '{
    username: "Schnell Firmware",
    embeds: [{
      title: $title,
      description: $desc,
      color: $color,
      fields: [
        { name: "📥 Install in browser", value: "[Open the configuration tool](\($inst))" },
        { name: "📝 Release notes", value: "[GitHub release page](\($rel))" }
      ]
    }]
  }')
curl -sf -H "Content-Type: application/json" -d "$payload" "$WEBHOOK" \
  || echo "::warning::Discord announcement failed."
