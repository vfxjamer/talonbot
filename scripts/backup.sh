#!/usr/bin/env bash
set -e

DATASET="peroplayer/talon-v1-checkpoints"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CHECKPOINTS_DIR="$PROJECT_DIR/checkpoints"

echo "[BACKUP] Daemon started — backing up to Kaggle dataset $DATASET"

mkdir -p "$CHECKPOINTS_DIR"

# Ensure dataset-metadata.json exists
cat > "$CHECKPOINTS_DIR/dataset-metadata.json" << EOF
{
  "title": "Talos V1 Checkpoints",
  "id": "$DATASET",
  "licenses": [{"name": "CC0-1.0"}]
}
EOF

while true; do
    LATEST_SUBDIR=$(find "$CHECKPOINTS_DIR" -maxdepth 1 -type d -name '[0-9]*' -printf '%f\n' 2>/dev/null | sort -n | tail -1)

    if [ -n "$LATEST_SUBDIR" ]; then
        PCT=$(echo "scale=6; $LATEST_SUBDIR * 100 / 400000000000" | bc 2>/dev/null || echo "?")
        echo "[BACKUP] Step: $LATEST_SUBDIR / 400000000000 (${PCT}%)"
    fi

    OUT=$(kaggle datasets version -p "$CHECKPOINTS_DIR" -m "checkpoint" --dir-mode zip 2>&1)
    if [ $? -eq 0 ]; then
        echo "[BACKUP] Pushed at $(date -u +%H:%M:%S UTC)"
    elif echo "$OUT" | grep -qi "404\|Not Found"; then
        kaggle datasets create -p "$CHECKPOINTS_DIR" --public 2>/dev/null || true
        echo "[BACKUP] Created dataset $DATASET"
    else
        echo "[BACKUP] $OUT"
    fi

    sleep 3600
done
