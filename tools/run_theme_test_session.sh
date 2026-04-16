#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_PATH="$ROOT_DIR/ESTACION-PRO"
HOME_OVERRIDE=""
EXTRA_ARGS=()

usage() {
    cat <<'EOF'
Usage: run_theme_test_session.sh [options] [-- <extra app args>]

Options:
  --bin PATH         Path to ESTACION-PRO binary (default: ./ESTACION-PRO)
  --home PATH        Home path for isolated test profile (passed as --home PATH)
  -h, --help         Show this help

This script:
1) Runs ESTACION-PRO with --debug.
2) Waits for you to test themes manually and close the app.
3) Copies app log to build/theme-test-logs/session_YYYYmmdd_HHMMSS.log.
4) Generates summary file next to the log via summarize_theme_log.sh.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --bin)
            [[ $# -ge 2 ]] || { echo "Missing value for --bin" >&2; exit 1; }
            BIN_PATH="$2"
            shift 2
            ;;
        --home)
            [[ $# -ge 2 ]] || { echo "Missing value for --home" >&2; exit 1; }
            HOME_OVERRIDE="$2"
            shift 2
            ;;
        --)
            shift
            EXTRA_ARGS+=("$@")
            break
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            EXTRA_ARGS+=("$1")
            shift
            ;;
    esac
done

if [[ ! -x "$BIN_PATH" ]]; then
    echo "Binary not executable: $BIN_PATH" >&2
    echo "Build first, or pass --bin with a valid path." >&2
    exit 1
fi

SESSION_TAG="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="$ROOT_DIR/build/theme-test-logs"
mkdir -p "$OUT_DIR"

if [[ -n "$HOME_OVERRIDE" ]]; then
    APPDATA_DIR="$HOME_OVERRIDE/ES-PRO"
    APP_ARGS=(--home "$HOME_OVERRIDE" --debug "${EXTRA_ARGS[@]}")
else
    APPDATA_DIR="$HOME/ES-PRO"
    APP_ARGS=(--debug "${EXTRA_ARGS[@]}")
fi

LOG_PATH="$APPDATA_DIR/logs/es_log.txt"
SESSION_LOG="$OUT_DIR/session_${SESSION_TAG}.log"
SUMMARY_PATH="$OUT_DIR/session_${SESSION_TAG}.summary.txt"

echo "Starting theme test session..."
echo "Binary: $BIN_PATH"
echo "App data dir: $APPDATA_DIR"
echo "When done testing themes, close ESTACION-PRO to finish capture."

"$BIN_PATH" "${APP_ARGS[@]}"

if [[ ! -f "$LOG_PATH" ]]; then
    echo "Log file not found at $LOG_PATH" >&2
    echo "The app may have failed before opening logs." >&2
    exit 1
fi

cp "$LOG_PATH" "$SESSION_LOG"
"$ROOT_DIR/tools/summarize_theme_log.sh" "$SESSION_LOG" > "$SUMMARY_PATH"

echo ""
echo "Session log:     $SESSION_LOG"
echo "Session summary: $SUMMARY_PATH"
echo ""
echo "Share the .summary.txt (or full .log) and I will parse the next fixes."
