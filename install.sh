#!/bin/bash

set -euo pipefail

# parse arguments
GLOBAL=""
while [[ $# -gt 0 ]]; do
	case "$1" in
		--global | -g) GLOBAL=1 ;;
	esac
	shift
done

# get the build directory
BUILD_DIR="${BUILD_DIR:-}"
if [[ -z "$BUILD_DIR" && -n "${BASH_SOURCE[0]:-}" && -f "${BASH_SOURCE[0]}" ]]; then
	script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
	if git -C "$script_dir" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
		# the script is in a git repo: assume it's the right one
		BUILD_DIR="$script_dir"
	fi
fi
if [[ -z "$BUILD_DIR" ]]; then
	# not runned as a file (e.g. with curl) or not in a git repo: clone it unless it's already here
	BUILD_DIR="${TMPDIR:-/tmp}/oly_build"
	if [[ ! -d "$BUILD_DIR/.git" ]]; then
		git clone https://github.com/antinomie8/oly "$BUILD_DIR"
	fi
fi
cd "$BUILD_DIR"

# build the executable
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release --parallel

# set the installation locations
if [[ -z "$GLOBAL" ]]; then
	data_home=${XDG_DATA_HOME:-$HOME/.local/share}
	INSTALL_DIR="${INSTALL_DIR:-$HOME/.local/bin}"
	SHARE_DIR="${SHARE_DIR:-$data_home}"
	ZSH_SITE_FUNCTIONS="${ZSH_SITE_FUNCTIONS:-$data_home/zsh/completions}"
	sudo=""
else
	INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"
	SHARE_DIR="${SHARE_DIR:-/usr/share}"
	ZSH_SITE_FUNCTIONS="${ZSH_SITE_FUNCTIONS:-/usr/local/share/zsh/site-functions}"
	sudo="sudo"
fi
declare -A files
files["build/Release/oly"]="$INSTALL_DIR"
files["assets/typst/packages/local/oly"]="$SHARE_DIR/typst/packages/local"
files["assets/extras/_oly"]="$ZSH_SITE_FUNCTIONS"
files["assets/app/oly.desktop"]="$SHARE_DIR/applications"
files["assets/app/oly.png"]="$SHARE_DIR/icons/hicolor/48x48/apps"
for file in "${!files[@]}"; do
	dir="${files["$file"]}"
	if [[ ! -d "$dir" ]]; then
		$sudo mkdir -p "$dir"
	fi
	$sudo cp -r "$file" "$dir"
done

# register scheme handler
xdg-mime default oly.desktop x-scheme-handler/oly

# $PATH and $fpath check
if [[ -z "$GLOBAL" && ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
	echo
	echo '⚠️ Installation directory is not in your $PATH.'
	echo 'Add this to your shell config:'
	echo 'export PATH="'"$INSTALL_DIR"':$PATH"'
fi
if [[ -z "$GLOBAL" && "${SHELL:-}" = */zsh ]]; then
	if ! zsh -ic 'print -l $fpath' 2>/dev/null |
		grep -qx "$ZSH_SITE_FUNCTIONS"; then
		echo
		echo '⚠️ Zsh completion directory not in your $fpath.'
		echo 'Add this to your .zshrc:'
		echo 'fpath=("$'"$ZSH_SITE_FUNCTIONS"'" $fpath)'
	fi
fi
