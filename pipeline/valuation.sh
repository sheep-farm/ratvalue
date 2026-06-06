#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 --ticker TICKER --token TOKEN"
    echo "       $0 --ticker WEGE3 --token abc123"
    exit 1
}

TICKER=""
TOKEN="${BRAPI_TOKEN:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ticker) TICKER="${2^^}"; shift 2 ;;
        --token)  TOKEN="$2";      shift 2 ;;
        *) usage ;;
    esac
done

[[ -z "$TICKER" ]] && { echo "Error: --ticker required"; usage; }
[[ -z "$TOKEN"  ]] && { echo "Error: --token required (or set BRAPI_TOKEN)"; usage; }

DIR="$(cd "$(dirname "$0")" && pwd)"

echo "══════════════════════════════════════════════"
echo "  Damodaran Valuation — $TICKER"
echo "══════════════════════════════════════════════"

echo ""
echo "── 1/4  Fetching data ($TICKER)..."
BRAPI_TOKEN="$TOKEN" "$DIR/brapi_fetch.py" "$TICKER"

echo ""
echo "── 2/4  Deriving parameters..."
"$DIR/prepare_inputs.py" "$TICKER"

echo ""
echo "── 3/4  Running DCF..."
"$DIR/demo.py" "$TICKER"

echo ""
echo "── 4/4  Generating PDF report..."
"$DIR/report.py" "$TICKER"
