#!/usr/bin/env bash
set -e

REPO_NAME="talos-v1-checkpoints"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CHECKPOINTS_DIR="$PROJECT_DIR/checkpoints"

if [ -z "$GITHUB_TOKEN" ]; then
    echo "[BACKUP] ERROR: GITHUB_TOKEN not set!"
    exit 1
fi

echo "[BACKUP] Daemon started — backing up $CHECKPOINTS_DIR"

REPO_EXISTS=$(curl -s -o /dev/null -w "%{http_code}" \
    -H "Authorization: token $GITHUB_TOKEN" \
    "https://api.github.com/repos/GigaLearnTalos/$REPO_NAME" 2>/dev/null || echo "000")

if [ "$REPO_EXISTS" != "200" ]; then
    echo "[BACKUP] Creating remote repo $REPO_NAME..."
    curl -s -X POST \
        -H "Authorization: token $GITHUB_TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"$REPO_NAME\",\"private\":true}" \
        "https://api.github.com/user/repos" > /dev/null
fi

mkdir -p "$CHECKPOINTS_DIR"
cd "$CHECKPOINTS_DIR"

if [ ! -d ".git" ]; then
    git init
    git remote add origin "https://GigaLearnTalos:${GITHUB_TOKEN}@github.com/GigaLearnTalos/$REPO_NAME.git"
    git checkout -b main
    git config user.email "talos@talonbot.ai"
    git config user.name "Talos Backup"
fi

TOTAL_STEPS=400000000000

while true; do
    LATEST_SUBDIR=$(find . -maxdepth 1 -type d -name '[0-9]*' -printf '%f\n' 2>/dev/null | sort -n | tail -1)

    if [ -n "$LATEST_SUBDIR" ]; then
        PCT=$(echo "scale=6; $LATEST_SUBDIR * 100 / $TOTAL_STEPS" | bc 2>/dev/null || echo "?")
        echo "[BACKUP] Step: $LATEST_SUBDIR / $TOTAL_STEPS (${PCT}%)"
    fi

    git add -A
    if git diff --cached --quiet 2>/dev/null; then
        echo "[BACKUP] No changes to commit."
    else
        git commit -m "ckpt $(date -u +%Y-%m-%dT%H:%M:%SZ) step $LATEST_SUBDIR"
        git push origin main --force 2>&1 | tail -2
        echo "[BACKUP] Pushed at $(date -u +%H:%M:%S UTC)"
    fi

    sleep 3600
done
