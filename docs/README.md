# docs/ — GitHub Pages 發佈目錄

此目錄僅為 **GitHub Pages 發佈用途**，不是專案文件。
專案文件（知識庫）在 repo 根目錄的 `assets/`、`chapters/`、`program_info/`、`resource_info/`、`rebuild_info/`。

## 內容

- `character-stat-comparison/fd2_growth_tables.html` — 角色屬性數值比較（成長數值表）。
  由 `tools/growth_table/`（`build_page.py`）產生，請勿手改；改動請動模板 `tools/growth_table/page_template.html` 後重跑建置。
- `.nojekyll` — 停用 Jekyll，讓自足的 HTML 原樣送出。

## 發佈方式

Repo `Settings → Pages` → 「Deploy from a branch」，來源選 `main` 分支的 `/docs`。
發佈後網址為 `https://<帳號>.github.io/<repo>/character-stat-comparison/fd2_growth_tables.html`。
