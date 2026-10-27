# NewRenderingSystem 設計まとめ

DirectX12 前提のレンダリングシステムの設計。仮実装は `C:\cabotya\Projects\team\Engine\Rendering\NewRenderingSystem` にある。
名前空間は `NRS`。

---

## 1. 基本方針

- **パスは足しやすく**。パスを追加する側は影やマテリアルの内部事情を知らなくてよい。
- **依存は上位 → 下位の一方向**。下位は上位を知らない。下から上へ伝えたいときは戻り値で返す（必要なら下位側に抽象を置く依存性逆転）。
  - 目安: 「その部品を単体で別プロジェクトに持っていけるか」。
- **GPU の実体はすべてハンドル**で扱い、`IRenderDevice` 経由でしか触らない。
- **パイプライン（Forward など）は起動時に決めて途中で変えない**。
- **グラフィックス API は当面 D3D12 のみ**。他 API を想像で抽象化しない。境界（ハンドルと自前の型）だけ切っておき、2つ目の API を触るときに共通化を考える。
  - パイプラインやパスのコードに `ID3D12*` / `D3D12_*` / `DXGI_*` が出てきたら漏れている。

---

## 2. 全体構成（依存の向き）

```
RenderEngine（最上位。具象を組み立てて注入する）
  ├→ MeshRendererDataStore ←(登録・更新)── IMeshRenderer
  ├→ CameraDataStore       ←(登録・更新)── ICamera ── ICameraSettings
  ├→ MaterialRegistry ── Material
  └→ IRenderPipeline（例: ForwardPipeline）
        ├→ CullingStage
        ├→ DrawListBuilder
        └→ IRenderPass 群（Shadow / DepthPrepass / Forward ...）
              └→ ICommandList / IRenderDevice（RHI）
                    └→ D3D12RenderDevice（D3D12 を知るのはここだけ）
```

- `RenderEngine` だけが全体と具象型を知る。
- `IMeshRenderer` / `ICamera` はストアに書くだけで、パイプラインを知らない。
- `IRenderPipeline` は `RenderEngine` を知らない。渡されたスナップショットを読むだけ。
- データストア、`Material` は `IRenderDevice` を知らない（ハンドルを持つだけ）。

---

## 3. 1フレームの流れ

1. `RenderEngine::RenderFrame`
   1. `IRenderDevice::BeginFrame()`（フェンス待ち、アロケータのリセット、遅延破棄）
   2. 各ストアから `TakeSnapshot()` して `FrameSnapshot` を作る（読み取り専用コピー。描画スレッドとの競合を避ける）
   3. `IRenderPipeline::Render(frame)`
   4. `EndFrame()` → `Present()`
2. `IRenderPipeline::Render`
   1. スナップショットから **RenderView の一覧**を作る（カメラ由来 + 影由来）
   2. ビューごとに `CullingStage` で可視オブジェクトを絞る
   3. パスごとに、そのビューを受け付けるか（`AcceptsView`）を判定
   4. `DrawListBuilder` がパスタグで Material を引いて描画リストを作る（PSO が無ければ除外）
   5. パスが描画リストを `ICommandList` に記録
   6. `Submit`

---

## 4. 各要素

### RenderEngine
- 起動時に `IRenderDevice`（D3D12 実装）と `IRenderPipeline` を受け取って注入する。
- カメラ作成時に `pipeline->CreateCameraSettings()` を呼び、そのカメラに持たせる。
- ロード処理がリソースを作れるよう `IRenderDevice` を公開する。

### MeshRendererDataStore / IMeshRenderer
- 描画に必要な最小限だけを持つ。ゲーム側オブジェクトへの参照は持たない。
- 中身: 頂点/インデックスバッファのハンドル、`MaterialId`、ワールド行列、前フレームのワールド行列、AABB、レイヤーマスク、影を落とすか。
- ID は世代番号付き（破棄済み ID の使用を検出）。
- 将来: SoA 化、ダーティ管理。

### CameraDataStore / ICamera / ICameraSettings
- メッシュと同じく、ICamera が登録・更新し、エンジンが取得する。
- `CameraCommon`（全パイプライン共通）: ビュー/射影行列、前フレームの ViewProj、位置、ビューポート、描画先、カリングマスク、優先度、クリア色。
- `ICameraSettings`（パイプライン固有。例: `ForwardCameraSettings` に MSAA 数、プリパス有無）
  - パイプラインが固定なので、カメラは**今のパイプライン用を1つだけ**持つ。
  - パイプラインが生成するので型の不一致は起きない（受け側はキャストのみ）。
  - スナップショット時に `Clone()` でコピー。

### RenderView
- 「どこから、どこへ、何を描くか」。カメラ由来も影由来も同じ形にし、同じ経路（カリング → 描画リスト → パス）で処理する。
- 影の特殊性は、ビューの設定（深度のみ、バイアス、描画先が影アトラス）に閉じ込める。

### IRenderPipeline
- 「何を、どの順で描くか」を決める。
- 持つもの: パイプライン全体の設定（`IRenderingSettings`。影の解像度、MSAA など）、パス構成、`CullingStage`、`DrawListBuilder`。
- 設定の分け方:
  - シーン全体で1つ → パイプライン設定
  - カメラごとに変えたい → `ICameraSettings`
  - パス固有（影バイアスなど）→ パス自身
- パスの追加は `AddPass()` に登録するだけ。登録順 = 実行順。

### IRenderPass
- `GetTag()`（PassTag）、`AcceptsView(view)`、`Execute(PassContext)`。
- `PassContext`: コマンドリスト、ビュー、スナップショット、描画リスト、MaterialRegistry、ビュー定数。

### ShadowSystem（未実装）
- ライトとカメラを入力に、影ビュー（カスケード、ポイントライト6面など）・影アトラス・シェーダー用の影データを出力する。
- パイプラインからは「影由来の RenderView」として普通に流れるだけ。ライティング側は影アトラスと影データのバッファだけを読む契約にする。

### Material / MaterialRegistry
- **Material はパイプラインごとに存在する**（今のパイプライン用だけ作る）。サーフェス記述とテンプレートの合成方式は採用しない（必要になったら検討）。
- 持つもの = **データ + パスごとの PSO ハンドル**
  - `PipelineHandle passes[PassTag::Count]`: そのパスのエントリがあれば描画、なければスキップ。影を落とさない素材は Shadow を持たなければよい。半透明は DepthPrepass を持たない。
  - `ConstantBufferData`（定数パラメータ）
  - `TextureHandle textures[8]`
- シェーダーは PSO に含まれるので、`ShaderHandle` は Material が持たない（PSO 作成時の材料）。
- `IRenderDevice` を知らない。GPU への書き込みはパイプライン側。
- `MeshRendererDataStore` は `MaterialId` だけを持ち、`MaterialRegistry` で本体を引く。
- 注意: 頂点変形（スキニングなど）は全パスのシェーダーで共通の関数を `#include` して共有する（影とメインのズレ、プリパスとの深度不一致を防ぐ）。

### PassTag
- `Shadow`, `DepthPrepass`, `Forward`（増やすときはここに追加）。
- 将来候補: Velocity、GBuffer、ピッキング/ID、リフレクションキャプチャなど。

### ShaderData（ConstantBufferData / StructuredBufferData）
- シェーダーに渡すデータの入れ物。Material、カメラ、オブジェクトなどで使い回す。
- 2通りの持ち方:
  1. **CPU の値**（`Set<T>()` / `SetArray<T>()`）→ 変更時（ダーティ）にパイプライン側がアップロード
  2. **GPU バッファのハンドル**（`SetGpuBuffer()`）→ GPU が書いた結果や、ReadBack 対象
- 自身は GPU に書き込まない。
- HLSL 側とのレイアウト一致（16 バイト境界のパッキング）に注意。

---

## 5. RHI（IRenderDevice / ICommandList）

### ハンドル
- `Handle<Tag>{ index, generation }`。`BufferHandle`, `TextureHandle`, `ShaderHandle`, `PipelineHandle`, `SamplerHandle`。

### IRenderDevice
- リソース作成: `CreateBuffer`, `CreateTexture`, `CreateShader`, `CreatePipeline`(PSO + 共通ルートシグネチャ), `CreateSampler`
- `Destroy`: **遅延破棄**（GPU が使い終わったフレームで解放）
- `UpdateBuffer`: Upload メモリへの書き込み
- **ReadBack は非同期（チケット方式）**: `RequestReadback()` → 1〜3 フレーム後に `TryGetReadback()` で取得
- コマンド: `BeginCommandList`, `Submit`（当面はフレームに1本）
- フレーム制御: `BeginFrame`, `EndFrame`, `Present`, `GetBackBuffer`, `Resize`

### バッファの種類
- 型を分けず、`BufferDesc` の **用途フラグ**（Constant / Structured / ReadWrite / Vertex / Index / Indirect）と **メモリ種別**（GpuOnly / Upload / Readback）で表す。シェーダーへの見せ方はバインディングで決める。
- 最初に使うのは ConstantBuffer、Vertex/Index、Texture2D + Sampler、StructuredBuffer。UAV・ByteAddress・Indirect は GPU カリングや GPU ドリブン描画を始めるときに。

### バインディング
- パスは `SetBindings(Bindings)` で「どのスロットに何を見せるか」だけを書く。
- `SetGraphicsRootDescriptorTable` などの D3D12 固有操作は**外に出さない**（ディスクリプタヒープ管理と一体で実装の内側に隠す）。
- ルートシグネチャは当面**全パイプライン共通で1つ**:
  - `b0`: View（カメラ行列など）
  - `b1`: Material（マテリアル定数）
  - `b2`: Object（ワールド行列など）
  - `t0..t7`: テクスチャ / StructuredBuffer
  - `s0..s3`: サンプラー
- `ConstantBinding` は CPU の値（実装側がアップロードリングにコピー）か、GPU バッファのハンドルのどちらかを渡す。
- 将来バインドレスやパスごとのルートシグネチャが欲しくなったら、D3D12 用語ではなく「バインドグループ」などの抽象を追加する。

### リソースバリア
- 当面は**パス側が `Transition()` を明示的に呼ぶ**。現在の状態はデバイス側が保持。
- パスが増えたらフレームグラフで自動化を検討。

### D3D12 実装が内部に持つ予定のもの
- ID3D12Device、コマンドキュー、スワップチェーン
- フレーム・イン・フライト（2〜3）分のコマンドアロケータとフェンス値
- ハンドル → ID3D12Resource のテーブル（世代番号で寿命チェック）
- CBV/SRV/UAV 用のシェーダー可視ディスクリプタヒープ（フレームごとのリング）、RTV/DSV ヒープ
- 定数バッファ用のアップロードリング
- 共通ルートシグネチャ
- 遅延破棄キュー
- ReadBack 用ヒープとチケット管理

---

## 6. ファイル構成

```
NewRenderingSystem/
  Core/       MathTypes.h（仮。DirectXMath 等に差し替え）, ShaderData.h
  RHI/        RenderHandle.h, RHITypes.h, ICommandList.h, IRenderDevice.h
  RHI/D3D12/  D3D12RenderDevice.h（宣言のみ）
  Material/   Material.h, MaterialRegistry.h
  Scene/      MeshRendererDataStore.h/.cpp, CameraDataStore.h/.cpp
  Pipeline/   RenderView.h, RenderStages.h/.cpp, IRenderPass.h, IRenderPipeline.h
  Pipeline/Forward/ ForwardPipeline.h/.cpp, ForwardPasses.h/.cpp
  Engine/     RenderEngine.h/.cpp
```

---

## 7. 未実装・TODO

- D3D12 実装の中身（`CreateD3D12RenderDevice`）。まずは三角形を出すところまで。
- 視錐台カリング（今はレイヤーと影フラグのみ）
- ShadowSystem（影ビューと影アトラスの生成）
- カメラごとの深度バッファの確保
- 描画リストのソートキーに深度を入れる（不透明は前→後、半透明は後→前）
- Material 定数のアップロード方式（当面はアップロードリング。数が増えたら全 Material を1本の StructuredBuffer にまとめる）
- 数学型の差し替え

## 8. 将来検討

- フレームグラフ（バリアの自動化、リソースのエイリアシング）
- コマンド記録のマルチスレッド化（スレッドごとに ICommandList）
- GPU カリング / ExecuteIndirect
- バインドレス
- 2つ目のグラフィックス API（そのときに RHI の共通化を見直す）
