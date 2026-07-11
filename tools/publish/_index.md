# tools/publish/

把 dev repo 的乾淨 subset 發佈到 public repo（`github.com/fdpsfred/fd2-anatomy`）的工具。
public repo 是一個「零 dev history、全新單一 commit」的 subset 鏡像。

| Script | 用途 |
|---|---|
| `publish_public.py` | 從 dev repo HEAD `git archive` 出追蹤檔 → 移除 dev-only 的 `.claude/` + `CLAUDE.md` → 掃個資（Windows 使用者名 `fdpsf` 主機路徑 / 真名；公開帳號 handle `fdpsfred` 例外放行）→ 建**全新單一 commit**（不帶 dev history）。**預設 dry-run**（列 manifest + 掃描結果、不 push）；加 `--push` 才 force-push 到 public repo。staging 在 `workspace/publish/`，public remote 可用 `FD2_PUBLIC_REMOTE` 覆寫。 |

發佈流程：dev repo 改動 commit 後，先 `python tools/publish/publish_public.py` dry-run 檢視 manifest 與 PII 掃描，確認乾淨再加 `--push`。腳本發佈的是 HEAD（已 commit 的狀態），不是 working tree，所以要先 commit dev 變更。每次發佈都是「一個全新 commit force-push 覆蓋」，對外永遠零 dev history。
