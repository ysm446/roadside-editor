# Progress

作成日時: 2026-07-22 22:27
更新日時: 2026-07-22 22:27

## 現在の状態

コードは未着手。ドキュメントのみの初期段階。次の作業は Phase 0(terrain-editor の取り込み方針の決定とビルド骨格の作成)。

## 完了済み

- 2026-07-22: リポジトリ初期化、初回コミット
- 2026-07-22: UV空間侵食の技術メモ [docs/uv-space-erosion.md](../uv-space-erosion.md) を整備
- 2026-07-22: AGENTS.md / CLAUDE.md を作成
- 2026-07-22: goals / plan / progress の初期内容を作成

## 未完了

- [docs/plan/plan.md](plan.md) の Phase 0 以降すべて

## 注意点

- AGENTS.md / CLAUDE.md の検証手順は現状 `npm run build` / Python 前提の記述になっている。terrain-editor ベース(C++ / CMake)で確定したら両ファイルを更新すること(Phase 0 のタスク)。
- terrain-editor のローカルクローンは `C:\GitHub\terrain-editor`。アーキテクチャは同リポジトリの `CLAUDE.md` に詳しい。
- 侵食ポテンシャルは必ず $\phi = P_z + h \cdot N_z$ を使う(変位 $h$ 単独で侵食をかけると道路の傾きを無視した流れになる)。実装レビュー時の最重要チェックポイント。
