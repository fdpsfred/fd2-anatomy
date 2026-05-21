這是繁體中文版 DOS 遊戲「炎龍騎士團二代」 的逆向工程專案。
目標:

- 完全理解 Ghidra 對 FD2.LE 產生的 decompiled source，最終完美複刻遊戲程式碼並且可以被編譯成在 DOS 環境下正確執行的執行檔。

要遵守的工作步驟:

- 每個新session的一開始要讀取這個檔案以了解逆向工程知識庫結構和基礎知識: @index.md
- 在開始規劃或執行每個 plan 之前，都要先確認下述的可用工具可以使用，否則立刻停下來等我檢查
- 工作過程中產生的所有 deferred / backlog 項目在整個工作結束以前都要被深入研究和解決，真的遇到無法處理的狀況才問我，若我確認無法當下解決，才寫進 open_issues.md
- Scripts 規範:
  -- 新增的 scripts 必須放在 tools/{工作名稱} subfolder 下面
  -- Scripts 的 pipeline intermediate / output 一律寫到 workspace/{工作名稱} 下面
- 新增/修改知識庫的寫作規範:
  -- 在工作過程中得到的確定結論都要整合進知識庫內並且確定內容沒有重複
  -- 新撰寫的知識庫內容描述必須是「最後的結論」，不能是描述分析過程的流水帳
  -- 新撰寫的知識庫內容如果要引用其他文件或資料，不能引用到 legacy/ 或是 workspace/ 下面的東西
  -- 任何文件和scrtips的更新都要反映到相對應的 _index.md 內
  -- 修改完文件以後，要再次 review 剛剛修改的內容，確定沒有出現「在某個時間解出、以前原本是甚麼、phase」這類流水帳內容，若有則立刻修正

可用的工具:

- Ghidra 已經啟動並且打開 FD2.LE 的 code browser，所有的 decompiled source 都已經被解析過並且根據語意重新命名，可以透過 Ghidra mcp 存取
- DOSBox-X 執行檔路徑已經在 path 環境變數內
- DOSBox-X 要使用 silent mode 執行 (-silent command-line option)，以達成全自動化開發
- Watcom C/C++ 9.5a 的執行檔路徑: C:\Users\fdpsf\Documents\WATCOM_9.5a\BIN，要在 DOSBox-X 裡面執行

Ghidra 操作規範:

- 如果有對 FD2.LE 進行修改，例如新建 function、重新 decompile，在修改結束後都要檢查有沒有產生 error bookmark，若有就要全部修復
- Ghidra 搜尋 error bookmark 的指令是 list_bookmarks(category="Bad Instruction")
- Error bookmark 在問題修正後不會自己消失，要手動移除
- 如果有對 FD2.LE 修改過，在工作完成後要儲存變更
