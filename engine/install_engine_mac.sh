#!/bin/bash
# Sets up the song engine (ACE-Step 1.5) that MU Music Maker uses. Run by the plugin in a Terminal window.
#   install_engine_mac.sh <folder to install into>
DIR="$1"
ENGINE_REF="ca1e85fe9430179831e6bc6be790c332190a3866"
export PATH="$HOME/.local/bin:$HOME/.cargo/bin:/opt/homebrew/bin:/usr/local/bin:$PATH"

say()  { printf '\n\033[1;33m%s\033[0m\n' "$1"; }
fail() { printf '\n\033[1;31m%s\033[0m\n\n' "$1"; read -n 1 -s -r -p "Press any key to close this window."; echo; exit 1; }

say "MU Music Maker: setting up the song engine"
echo "The first time, this downloads about 10 GB, so it can take a while."
echo "Leave this window open until it says All done."

[ -n "$DIR" ] || fail "No folder was given to install into."
mkdir -p "$DIR" || fail "Could not create $DIR"

FREE_GB=$(df -g "$DIR" 2>/dev/null | awk 'NR==2 {print $4}')
if [ -n "$FREE_GB" ] && [ "$FREE_GB" -lt 20 ] 2>/dev/null; then
  fail "There is only ${FREE_GB} GB free on this disk. The engine needs about 20 GB. Free some space and run this again."
fi

say "Step 1 of 4: the Python manager (uv)"
if command -v uv >/dev/null 2>&1; then
  echo "Already installed."
else
  curl -LsSf https://astral.sh/uv/install.sh | sh || fail "Could not install uv. Check the internet connection and run this again."
  export PATH="$HOME/.local/bin:$PATH"
  command -v uv >/dev/null 2>&1 || fail "uv was installed but could not be found. Close this window and run the setup again."
fi

say "Step 2 of 4: downloading the engine"
TMP="$DIR/.download"
rm -rf "$TMP"
mkdir -p "$TMP" || fail "Could not write to $DIR"
curl -L --fail --progress-bar -o "$TMP/engine.tar.gz" "https://github.com/ace-step/ACE-Step-1.5/archive/$ENGINE_REF.tar.gz" \
  || fail "The engine could not be downloaded. Check the internet connection and run this again."
tar -xzf "$TMP/engine.tar.gz" -C "$TMP" || fail "The download was damaged. Run this again."
SRC="$(find "$TMP" -mindepth 1 -maxdepth 1 -type d | head -1)"
[ -f "$SRC/pyproject.toml" ] || fail "The download does not look like the engine. Run this again."
# copied over the top, so models that are already there are kept
cp -R "$SRC/." "$DIR/" || fail "Could not copy the engine into $DIR"
rm -rf "$TMP"
cd "$DIR" || fail "Could not open $DIR"

say "Step 3 of 4: installing what the engine needs"
uv sync || fail "The engine's parts could not be installed. Run this again; it carries on from where it stopped."

say "Step 4 of 4: downloading the models (the big part)"
uv run --no-sync acestep-download || fail "The models could not be downloaded. Run this again; it carries on from where it stopped."

date > "$DIR/.mu-installed"
say "All done."
echo "Go back to MU Music Maker and press Start engine. You can close this window."
echo
read -n 1 -s -r -p "Press any key to close this window."
echo
