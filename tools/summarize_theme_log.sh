#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: summarize_theme_log.sh LOG_PATH" >&2
    exit 1
fi

LOG_PATH="$1"
if [[ ! -f "$LOG_PATH" ]]; then
    echo "Log file not found: $LOG_PATH" >&2
    exit 1
fi

count() {
    local pattern="$1"
    grep -E -c "$pattern" "$LOG_PATH" || true
}

count_level() {
    local level="$1"
    grep -E -c "^[A-Za-z]{3} [ 0-9]{2} [0-9:]{8} ${level}:" "$LOG_PATH" || true
}

echo "Theme Log Summary"
echo "Log file: $LOG_PATH"
echo "Generated: $(date '+%Y-%m-%d %H:%M:%S')"
echo ""

echo "Counts"
echo "  ThemeData parse errors:           $(count 'ThemeData::(loadFile|parse[A-Za-z]+)\(\): .*([Ee]rror|[Mm]issing|[Ii]nvalid)')"
echo "  Unknown element types:            $(count 'ThemeData::parseView\(\): Unknown element type')"
echo "  Unknown property types:           $(count 'ThemeData::parseElement\(\): Unknown property type')"
echo "  Blank property values skipped:    $(count 'ThemeData::parseElement\(\): Property .* has no value defined')"
echo "  Invalid theme configuration:      $(count 'Invalid theme configuration')"
echo "  Files missing/not found:          $(count 'does not exist|doesn.t exist|not found|Couldn.t open|Couldn.t find')"
echo "  Storyboard warnings:              $(count 'ThemeStoryboard|storyboard')"
echo "  Total warnings:                   $(count_level 'Warn')"
echo "  Total errors:                     $(count_level 'Error')"
echo ""

echo "Top ThemeData warnings (up to 40)"
grep -E 'ThemeData::|Invalid theme configuration' "$LOG_PATH" | \
    grep -E 'Warning|Error' | \
    sed -E 's/^[A-Za-z]{3} [ 0-9]{2} [0-9:]{8} [A-Z]+: *//' | \
    sort | uniq -c | sort -nr | head -40 || true

echo ""
echo "Recent relevant lines (last 120)"
tail -n 120 "$LOG_PATH" | grep -E 'ThemeData::|Invalid theme configuration|ThemeStoryboard|Unknown|missing|does not exist|Couldn.t' || true
