#!/usr/bin/env bash
# codex-watch.sh — 監看 codex 與規劃者之間的通道。協定見 wf/CODEX-PROTOCOL.md
#
# 每行 stdout 就是一則通知。三個訊號源：
#   1) .codex-inbox/ 的新檔（.ask / .blocked / .done / .progress）
#   2) 各 m6-*-wt 分支上新出現的 wf/inbox/*-complete.md
#   3) 我的 codex 全部消失（異常結束的唯一訊號）
#
# 用法：SESSION=<session-id> scripts/codex-watch.sh

set -u
REPO="${REPO:-/home/lorkhan/repo/game_dev/aetheria}"
INBOX="$REPO/.codex-inbox"
SESSION="${SESSION:?需要 SESSION=<session-id>}"
POLL="${POLL:-30}"

mkdir -p "$INBOX"
cd "$REPO" || exit 1

# 我的 codex：讀 cmdline 的 -C 參數比對 session id。
# ⚠ 不要用名稱（會打到別的 agent 的同名實例）、不要用行程樹（nohup 會 reparent）、
#   也不要用 cwd（-C 只改 codex 內部的工作目錄，行程 cwd 仍是啟動處）。
my_codex_count() {
  local n=0 p
  for p in $(pgrep -x codex 2>/dev/null); do
    if tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null | grep -q -- "$SESSION"; then
      n=$((n + 1))
    fi
  done
  printf '%s' "$n"
}

declare -A seen_inbox
declare -A seen_report

# 開機時把既有的收件匣檔與分支回報都標記為已看過，避免重啟時重播歷史。
# ⚠ 分支那半原本漏了這步，於是每次重啟都把舊回報再喊一遍。
for f in "$INBOX"/*; do
  [ -e "$f" ] || continue
  seen_inbox["$(basename "$f")"]=1
done
for br in $(git for-each-ref --format='%(refname:short)' 'refs/heads/m6-*-wt' 2>/dev/null); do
  for rpt in $(git ls-tree -r --name-only "$br" -- wf/inbox 2>/dev/null | grep -- '-complete\.md$'); do
    seen_report["$br:$rpt"]=1
  done
done

echo "codex-watch 啟動：session=${SESSION:0:8}… 目前我的 codex $(my_codex_count) 個"

while true; do
  # ── 1) 收件匣 ────────────────────────────────────────────────
  for f in "$INBOX"/*; do
    [ -e "$f" ] || continue
    b="$(basename "$f")"
    [ -n "${seen_inbox[$b]:-}" ] && continue
    seen_inbox["$b"]=1
    first="$(head -n 1 "$f" 2>/dev/null | cut -c1-200)"
    case "$b" in
      *.blocked)  echo "[BLOCKED] $b — $first" ;;
      *.ask)      echo "[ASK] $b — $first" ;;
      *.done)     echo "[DONE] $b — $first" ;;
      *.progress) echo "[進度] $b — $first" ;;
      *.reply)    : ;;   # 我自己寫的，不回報
      *)          echo "[收件匣] $b — $first" ;;
    esac
  done

  # ── 2) 分支上新出現的回報檔 ──────────────────────────────────
  for br in $(git for-each-ref --format='%(refname:short)' 'refs/heads/m6-*-wt' 2>/dev/null); do
    for rpt in $(git ls-tree -r --name-only "$br" -- wf/inbox 2>/dev/null | grep -- '-complete\.md$'); do
      key="$br:$rpt"
      [ -n "${seen_report[$key]:-}" ] && continue
      # main 上已經有的（併過的）不算新
      if git cat-file -e "main:$rpt" 2>/dev/null; then
        seen_report["$key"]=1
        continue
      fi
      seen_report["$key"]=1
      echo "[回報] $br 產出 $(basename "$rpt") — $(git log --oneline -1 "$br")"
    done
  done

  # ── 3) 我的 codex 全部消失 ───────────────────────────────────
  if [ "$(my_codex_count)" = "0" ]; then
    echo "[結束] 我的 codex 已全部離開；未回報的回合需要人工確認"
    exit 0
  fi

  sleep "$POLL"
done
