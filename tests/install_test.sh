#!/usr/bin/env bash
set -euo pipefail

source_directory="${1:?Pass the Solitare source directory}"
build_directory="${2:?Pass the Solitare build directory}"
source_directory="$(cd -- "$source_directory" && pwd)"
build_directory="$(cd -- "$build_directory" && pwd)"
installer="$source_directory/scripts/install-desktop.sh"
binary="$build_directory/solitare"
test_directory="$(mktemp -d "${TMPDIR:-/tmp}/solitare-install.XXXXXX")"
trap 'rm -rf -- "$test_directory"' EXIT

fail() {
    printf 'install-test: %s\n' "$*" >&2
    exit 1
}

[[ -f "$installer" ]] || fail "Missing installer: $installer"
[[ -x "$binary" ]] || fail "Build first: $binary"

export HOME="$test_directory/home directory"
export XDG_CONFIG_HOME="$test_directory/xdg config"
export XDG_DATA_HOME="$test_directory/xdg data"
export XDG_CACHE_HOME="$test_directory/xdg cache"
export XDG_STATE_HOME="$test_directory/xdg state"
mkdir -p -- "$HOME" "$XDG_CONFIG_HOME"

prefix="$HOME/app prefix"
config_directory="$XDG_CONFIG_HOME/solitare config"
apps_list="$XDG_CONFIG_HOME/apps.list"
dry_prefix="$test_directory/dry prefix"
dry_config="$test_directory/dry config"
dry_apps="$test_directory/dry apps.list"
palette_directory="$test_directory/palette directory"
mkdir -p -- "$palette_directory"
cat > "$palette_directory/palette.json" <<'EOF'
{"background":"#112233","foreground":"#ddeeff","primary":"#aabbcc","red":"#dd6677"}
EOF

bash "$installer" --dry-run --build-dir "$build_directory" --prefix "$dry_prefix" \
    --config-dir "$dry_config" --apps-list "$dry_apps" >/dev/null
bash "$installer" --dry-run --build-dir "$build_directory" --prefix "$dry_prefix" \
    --config-dir "$dry_config" --apps-list "$dry_apps" --follow "$palette_directory" >/dev/null
[[ ! -e "$dry_prefix" && ! -e "$dry_config" && ! -e "$dry_apps" ]] || fail 'dry run wrote installation or configuration files'

cat > "$apps_list" <<'EOF'
first.desktop|/icons/first.svg|First
second.desktop|/icons/second.svg|Second
EOF
cp -- "$apps_list" "$test_directory/original-apps.list"

bash "$installer" --build-dir "$build_directory" --prefix "$prefix" \
    --config-dir "$config_directory" --apps-list "$apps_list" >/dev/null
[[ ! -e "$config_directory" ]] || fail 'standalone install created theme configuration'
"$prefix/bin/solitare" --version >/dev/null || fail 'installed standalone binary did not run'

desktop="$prefix/share/applications/io.github.solitare.desktop"
bin_desktop="$prefix/bin/applications/io.github.solitare.desktop"
icon="$prefix/share/icons/hicolor/scalable/apps/io.github.solitare.svg"
expected_exec="Exec=\"$prefix/bin/solitare\" --config-dir \"$config_directory\""
for installed_desktop in "$desktop" "$bin_desktop"; do
    [[ -f "$installed_desktop" ]] || fail "Missing desktop file: $installed_desktop"
    grep -Fxq '[Desktop Entry]' "$installed_desktop" || fail "Invalid desktop group: $installed_desktop"
    grep -Fxq 'Type=Application' "$installed_desktop" || fail "Invalid desktop type: $installed_desktop"
    grep -Fxq 'Name=Solitare' "$installed_desktop" || fail "Invalid desktop name: $installed_desktop"
    grep -Fxq 'Icon=io.github.solitare' "$installed_desktop" || fail "Invalid desktop icon key: $installed_desktop"
    grep -Fxq 'Terminal=false' "$installed_desktop" || fail "Invalid terminal setting: $installed_desktop"
    [[ "$(grep -c '^Exec=' "$installed_desktop")" -eq 1 ]] || fail "Expected one Exec entry: $installed_desktop"
    grep -Fxq "$expected_exec" "$installed_desktop" || fail "Exec is not absolute or correctly quoted: $installed_desktop"
done
[[ -f "$icon" ]] || fail 'Desktop icon was not installed'
for document in PROVENANCE.md UPSTREAM-LICENSE.txt; do
    cmp -s "$source_directory/assets/$document" "$prefix/share/doc/Solitare/$document" || fail "Design attribution was not installed: $document"
done
if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "$desktop" || fail 'Desktop entry failed desktop-file-validate'
fi

expected_apps="$test_directory/expected-apps.list"
cat "$test_directory/original-apps.list" > "$expected_apps"
printf 'io.github.solitare.desktop|%s/share/icons/hicolor/scalable/apps/io.github.solitare.svg|Solitare\n' "$prefix" >> "$expected_apps"
cmp -s "$apps_list" "$expected_apps" || fail 'app list order or appended registration is incorrect'

bash "$installer" --build-dir "$build_directory" --prefix "$prefix" \
    --config-dir "$config_directory" --apps-list "$apps_list" >/dev/null
[[ "$(grep -c '^io.github.solitare.desktop|' "$apps_list")" -eq 1 ]] || fail 'repeated install duplicated app registration'
cmp -s "$apps_list" "$expected_apps" || fail 'repeated install changed app list order or contents'

bash "$installer" --build-dir "$build_directory" --prefix "$prefix" \
    --config-dir "$config_directory" --apps-list "$apps_list" --follow "$palette_directory" >/dev/null
[[ -f "$config_directory/theme.json" ]] || fail 'follow install did not save the selected palette'
grep -Fq '"kind": "desktop"' "$config_directory/theme.json" || fail 'follow install did not save desktop source kind'
grep -Fq "\"path\": \"$palette_directory\"" "$config_directory/theme.json" || fail 'follow install did not save the palette path'
grep -Fq '"accent": "#aabbcc"' "$config_directory/theme.json" || fail 'follow install did not apply the selected palette'
[[ "$(grep -c '^io.github.solitare.desktop|' "$apps_list")" -eq 1 ]] || fail 'follow install duplicated app registration'
cmp -s "$apps_list" "$expected_apps" || fail 'follow install changed app list order or contents'

printf 'Installer checks passed.\n'
