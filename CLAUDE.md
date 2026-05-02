這是繁體中文版 DOS 遊戲「炎龍騎士團二代」 的逆向工程專案。
目標:

- 完全理解 Ghidra 對 FD2.LE 產生的 decompiled source，最終完美複刻遊戲程式碼並且可以被編譯成在 DOS 環境下執行的執行檔。

要遵守的工作步驟:

- 每個新session的一開始要讀取這個檔案以了解逆向工程知識庫結構和基礎知識: @index.md
- 工作完成後，如果新得到的資訊或是新寫出的scripts是現有的知識庫所沒有的而且適合加進知識庫，就停下來先讓我 review ，我許可後才整合進去
- 新增/修改知識庫的寫作規範:
  -- scripts要放到tools/下面
  -- 新撰寫的文件描述必須是「最後的結論」，不能是描述分析過程的流水帳
  -- 新撰寫的文件如果要引用其他文件或資料，不能引用到 legacy/ 或是 workspace/ 下面的東西

可用的工具:

- Ghidra 已經啟動並且打開 FD2.LE 的 code browser，所有的 decompiled source 都已經被解析過並且根據語意重新命名，可以透過 Ghidra mcp 存取
- DOSBox-X 和 Open Watcom v2 執行檔路徑已經在 path 環境變數內
- DosBox-X 要使用 silent mode 執行 (-silent command-line option)，以達成全自動化開發

Ghidra 操作規範:

- 如果有對 FD2.LE 進行修改，例如新建 function、重新 decompile，在修改結束後都要檢查有沒有產生 error bookmark，若有就要全部修復
- Ghidra 搜尋 error bookmark 的指令是 list_bookmarks(category="Bad Instruction")
- Error bookmark 在問題修正後不會自己消失，要手動移除
- 如果有對 FD2.LE 修改過，在工作完成後要儲存變更
