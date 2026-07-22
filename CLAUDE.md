# CLAUDE.md

このファイルは、Claude Code が Roadside-Editor で作業するときのガイドです。プロジェクトルールの原本は [AGENTS.md](AGENTS.md) にあり、本ファイルはそれを前提とした要約と補足です。両者が食い違う場合は AGENTS.md を優先し、本ファイルを追従させる。

## プロジェクトの目的

道路・路肩のようなリボン形状に対して、ワールド空間ハイトフィールドではなく **UV空間上で hydraulic erosion を計算する** エディタを作る。センターラインからスイープした道路メッシュと路肩リボンをUV展開し、UV空間の2Dグリッド上で侵食を回し、結果を displacement / mask テクスチャとして書き出すのがゴール。

技術コンセプトの詳細は [docs/uv-space-erosion.md](docs/uv-space-erosion.md) にまとまっている。作業前に一読すること。要点:

- リボン形状はヤコビアンが閉じた形で書け、メトリックが対角（$F = 0$）になるため、1Dの曲率配列だけで全テクセルのスケール補正が復元できる。
- 侵食を駆動するポテンシャルは法線方向変位 $h$ ではなく **ワールドZ**。ソルバに渡す標高は $\phi = P_z + h \cdot N_z$（バンク・縦断勾配に沿って水が流れるための最重要ポイント）。
- 法面が垂直に近い（$N_z \to 0$）ケースは対象外。
- CFL条件は最小セルサイズに支配されるため、ヘアピン内側の $1 - \kappa v$ 縮小には $s_u$ の下限クランプで対処する。
- UVレイアウトはワールド比例（例: 1テクセル = 4cm 固定）を推奨。道路〜路肩〜法面裾までを一枚の連続UVアトラスにする。

## terrain-editor の再利用（重要）

侵食のUIとノードシステムは、既存プロジェクト **terrain-editor** を土台として利用する。Phase 0 で terrain-editor のソース一式 (`src/`, `shaders/`, `assets/`, CMake 構成) を本リポジトリへコピーする **fork 方式** を採用済み。以後の変更は本リポジトリ内で完結し、terrain-editor 本体には手を入れない。

- GitHub: https://github.com/ysm446/terrain-editor
- ローカルクローン: `D:\GitHub\terrain-editor`

### ビルド

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug
./build/Debug/roadside_editor.exe
```

`roadside_editor.exe` は実行中ロックされるため、リビルド前に終了する。バージョンは `CMakeLists.txt` の `project(RoadsideEditor VERSION ...)` が基準。

terrain-editor は C++20 / CMake / vcpkg、Win32 + Direct3D 12、Dear ImGui + imgui-node-editor によるノードベース地形エディタで、以下を持つ:

- `src/node_graph.{h,cpp}` — ノードグラフのデータモデルと評価パイプライン（ハイトフィールド系とマスク系の2系統、ノード単位のキャッシュ、非同期評価）
- `Multi-Scale Erosion` ノード — Stream Power + Thermal + Deposition の侵食ソルバ（Schott et al. SIGGRAPH 2024 ベース、マルチグリッドピラミッド対応）
- `Mask Fluvial` / `Mask Noise` / `Mask Blend` などのマスクノード群
- D3D12 GPU compute バックエンド（CPU フォールバック付き）
- 3D / 2D プレビュー、`.terrainproj` プロジェクト保存

新機能を設計・実装するときは、まず terrain-editor 側の該当実装（特に `CLAUDE.md`、`src/node_graph.*`、`docs/nodes/`）を確認し、その構造・命名・慣習に合わせる。本プロジェクト固有の追加は、UVメトリック補正（$s_u, s_v$）、ワールドZポテンシャル、境界フラックスなど uv-space-erosion.md 由来の部分になる。

## 作業開始時の確認

作業前に必ず以下を読む:

1. [docs/plan/goals.md](docs/plan/goals.md) — 目的・完成形・重視する価値
2. [docs/plan/plan.md](docs/plan/plan.md) — 実装方針・優先順位・今後の予定
3. [docs/plan/progress.md](docs/plan/progress.md) — 進捗・完了/未完了・注意点

依頼が計画のどこに関係するかを把握してから着手し、方針と矛盾しそうな場合は実装前にユーザーへ確認する。

## 基本方針

- プロジェクト固有の説明・判断基準・運用ルールは日本語で書く。コード、コマンド、API 名、ファイルパス、識別子は既存表記を優先し、無理に翻訳しない。
- 既存の実装方針を確認してから変更する。
- ユーザーの未コミット変更を勝手に戻さない。
- 変更は必要な範囲に留め、無関係な整形やリファクタリングを混ぜない。

## ドキュメント管理

- `docs/**/*.md` を新規作成・更新するときは、本文の先頭付近に `作成日時: YYYY-MM-DD HH:MM` / `更新日時: YYYY-MM-DD HH:MM` を書く。更新時は更新日時を現在の作業日時にする。
- [docs/changelog.md](docs/changelog.md) は日本語で書き、ユーザー向けの明確な変更を記録する。未確定の変更は先頭付近の「未リリース」セクションに置く。
- `docs/design/` は設計資料・仕様メモ・調査資料の置き場。
- アプリのバージョンは `CMakeLists.txt` の `project(RoadsideEditor VERSION x.y.z ...)` を基準にする。

## ファイル編集の注意

- ソースファイルは **UTF-8 no BOM** で保存する。
- PowerShell を使う場合、変数の `$` はエスケープしない。AGENTS.md に UTF-8 no BOM での読み取り/atomic 書き込みの PowerShell レシピがある。
- ファイル検索は `rg` / `rg --files` を優先する。

## 検証

- コード変更後は、可能な限り `cmake --build build --config Debug` でビルドを通す。
- 検証できなかった場合は、その理由を作業報告に書く。

## ポインタ

- [AGENTS.md](AGENTS.md) — プロジェクトルールの原本（日本語）
- [docs/uv-space-erosion.md](docs/uv-space-erosion.md) — UV空間侵食の技術コンセプト（本プロジェクトの核）
- [docs/plan/](docs/plan/) — goals / plan / progress
- [docs/changelog.md](docs/changelog.md) — 変更履歴
- `D:\GitHub\terrain-editor\CLAUDE.md` — 土台プロジェクトのアーキテクチャ解説
- `D:\GitHub\terrain-editor\docs\nodes\README.md` — 既存ノードのドキュメント索引
