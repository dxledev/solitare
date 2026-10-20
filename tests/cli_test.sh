#!/usr/bin/env bash
set -euo pipefail

binary="${1:?Pass the solitare executable}"
test_directory="$(mktemp -d /tmp/solitare-cli.XXXXXX)"
config_directory="${test_directory}/config"
trap 'rm -rf -- "$test_directory"' EXIT

fail() {
    printf 'cli-test: %s\n' "$*" >&2
    exit 1
}

theme() {
    "$binary" --config-dir "$config_directory" theme "$@"
}

theme show > "${test_directory}/default.json"
[[ ! -e "$config_directory" ]] || fail 'show created configuration files'
theme preset lofi --dry-run > "${test_directory}/dry-run.json"
[[ ! -e "$config_directory" ]] || fail 'dry run created configuration files'
grep -Fq '"name": "lofi"' "${test_directory}/dry-run.json" || fail 'dry run did not report lofi'

theme preset lofi >/dev/null
theme set 'accent=#80402010' 'red_suit=#7f667788' >/dev/null
grep -Fq '"accent": "#80402010"' "${config_directory}/theme.json" || fail 'alpha-first RGB channels changed'
grep -Fq '"red_suit": "#7f667788"' "${config_directory}/theme.json" || fail 'alpha-first suit color changed'

before="$(sha256sum "${config_directory}/theme.json")"
if theme set 'accent=#bad' >/dev/null 2>&1; then fail 'invalid color succeeded'; fi
[[ "$before" == "$(sha256sum "${config_directory}/theme.json")" ]] || fail 'invalid set changed the theme'

cat > "${test_directory}/palette.json" <<'EOF'
{"colors":{"background":"#80402010","foreground":"#e8e1d8","primary":"#c7a77a","red":"#e27676"}}
EOF
theme import "${test_directory}/palette.json" --dry-run > "${test_directory}/import-preview.json"
[[ "$before" == "$(sha256sum "${config_directory}/theme.json")" ]] || fail 'import dry run changed the theme'
theme import "${test_directory}/palette.json" >/dev/null
grep -Fq '"background": "#80402010"' "${config_directory}/theme.json" || fail 'import lost alpha-first color'

cat > "${test_directory}/Colors.qml" <<'EOF'
pragma Singleton
import QtQuick
Singleton {
    property color background: "#181a1f"
    property color foreground: "#eceff2"
    property color primary: Qt.rgba(0.5, 0.25, 0.75, 0.4)
    property color red: "#e27676"
}
EOF
theme follow file "${test_directory}/Colors.qml" >/dev/null
theme sync --dry-run > "${test_directory}/sync-preview.json"
grep -Fq '"accent": "#668040bf"' "${test_directory}/sync-preview.json" || fail 'sync preview lost Qt.rgba alpha or channels'
grep -Fq '"source"' "${config_directory}/theme.json" || fail 'sync dry run changed source binding'
theme sync >/dev/null
grep -Fq '"accent": "#668040bf"' "${config_directory}/theme.json" || fail 'sync did not apply source palette'

before="$(sha256sum "${config_directory}/theme.json")"
if theme follow file "${config_directory}/theme.json" >/dev/null 2>&1; then fail 'theme accepted its own output as a source'; fi
[[ "$before" == "$(sha256sum "${config_directory}/theme.json")" ]] || fail 'rejected source changed the theme'

theme set 'accent=#112233' >/dev/null &
first_pid=$!
theme set 'card_back=#445566' >/dev/null &
second_pid=$!
wait "$first_pid"
wait "$second_pid"
grep -Fq '"accent": "#112233"' "${config_directory}/theme.json" || fail 'concurrent set lost accent update'
grep -Fq '"card_back": "#445566"' "${config_directory}/theme.json" || fail 'concurrent set lost card update'

theme --help >/dev/null
"$binary" --version >/dev/null
QT_QPA_PLATFORM=unknown-platform "$binary" --config-dir "$config_directory" theme show >/dev/null
if "$binary" --unknown-option >/dev/null 2>&1; then fail 'unknown GUI option was accepted'; fi
printf 'CLI checks passed.\n'
