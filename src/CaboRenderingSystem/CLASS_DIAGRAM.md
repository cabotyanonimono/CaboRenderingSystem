# NewRenderingSystem クラス設計図

DESIGN.md の構成を、現在のコードに合わせてクラス図にしたもの。名前空間はすべて `NRS`。
図は3枚に分けている（全体 / パイプライン / RHI）。矢印は依存の向き（上位 → 下位）。

---

## 1. 全体（RenderEngine とデータ）

```mermaid
classDiagram
    direction TB

    class RenderEngine {
        -unique_ptr~IRenderDevice~ m_device
        -unique_ptr~IRenderPipeline~ m_pipeline
        -MeshRendererDataStore m_meshes
        -CameraDataStore m_cameras
        -MaterialRegistry m_materials
        -uint64_t m_frameIndex
        +Initialize(device, pipeline)
        +Shutdown()
        +RenderFrame(deltaTime)
        +CreateCamera(CameraCommon) CameraId
        +Meshes() MeshRendererDataStore&
        +Cameras() CameraDataStore&
        +Materials() MaterialRegistry&
        +Device() IRenderDevice&
    }

    class IMeshRenderer {
        <<interface>>
        +SubmitTo(MeshRendererDataStore&)*
    }
    class MeshRendererDataStore {
        -vector~Slot~ m_slots
        -vector~uint32_t~ m_freeList
        +Register(MeshRendererData) MeshRendererId
        +Unregister(MeshRendererId)
        +Update(MeshRendererId, MeshRendererData)
        +Get(MeshRendererId) MeshRendererData*
        +TakeSnapshot() vector~MeshRendererData~
    }
    class MeshRendererData {
        <<struct>>
        +BufferHandle vertexBuffer
        +BufferHandle indexBuffer
        +uint32_t vertexStride
        +uint32_t indexCount
        +MaterialId material
        +Matrix4x4 world
        +Matrix4x4 prevWorld
        +AABB worldBounds
        +uint32_t layerMask
        +bool castShadow
        +bool visible
    }
    class MeshRendererId {
        <<struct>>
        +uint32_t index
        +uint32_t generation
    }

    class ICamera {
        <<interface>>
        +SubmitTo(CameraDataStore&)*
    }
    class CameraDataStore {
        -vector~Slot~ m_slots
        -vector~uint32_t~ m_freeList
        +Register(CameraCommon, unique_ptr~ICameraSettings~) CameraId
        +Unregister(CameraId)
        +Update(CameraId, CameraCommon)
        +GetSettings(CameraId) ICameraSettings*
        +TakeSnapshot() vector~CameraSnapshot~
    }
    class CameraCommon {
        <<struct>>
        +Matrix4x4 view
        +Matrix4x4 projection
        +Matrix4x4 prevViewProjection
        +Float3 position
        +Viewport viewport
        +TextureHandle target
        +uint32_t cullingMask
        +int32_t priority
        +float clearColor[4]
    }
    class CameraSnapshot {
        <<struct>>
        +CameraCommon common
        +unique_ptr~ICameraSettings~ settings
    }
    class ICameraSettings {
        <<interface>>
        +Clone()* unique_ptr~ICameraSettings~
    }

    class MaterialRegistry {
        -vector~unique_ptr~Material~~ m_materials
        +Add(unique_ptr~Material~) MaterialId
        +Get(MaterialId) Material*
        +Count() uint32_t
    }
    class Material {
        -PipelineHandle m_passes[PassTagCount]
        -ConstantBufferData m_constants
        -TextureHandle m_textures[8]
        +SetPass(PassTag, PipelineHandle)
        +RemovePass(PassTag)
        +HasPass(PassTag) bool
        +GetPass(PassTag) PipelineHandle
        +Constants() ConstantBufferData&
        +SetTexture(slot, TextureHandle)
        +GetTexture(slot) TextureHandle
    }
    class MaterialId {
        <<struct>>
        +uint32_t value
    }
    class PassTag {
        <<enumeration>>
        Shadow
        DepthPrepass
        Forward
        Count
    }

    class ShaderData {
        -Kind m_kind
        -vector~uint8_t~ m_bytes
        -uint32_t m_stride
        -BufferHandle m_gpuBuffer
        -bool m_dirty
        +Set~T~(value)
        +SetArray~T~(values, count)
        +SetGpuBuffer(BufferHandle)
        +Data() void*
        +Size() uint32_t
        +IsDirty() bool
        +ClearDirty()
    }
    class ConstantBufferData
    class StructuredBufferData

    RenderEngine *-- MeshRendererDataStore
    RenderEngine *-- CameraDataStore
    RenderEngine *-- MaterialRegistry
    RenderEngine *-- IRenderPipeline
    RenderEngine *-- IRenderDevice

    IMeshRenderer ..> MeshRendererDataStore : 登録・更新
    MeshRendererDataStore o-- MeshRendererData
    MeshRendererDataStore ..> MeshRendererId : 発行
    MeshRendererData --> MaterialId

    ICamera ..> CameraDataStore : 登録・更新
    CameraDataStore o-- CameraCommon
    CameraDataStore o-- ICameraSettings
    CameraDataStore ..> CameraSnapshot : TakeSnapshot
    CameraSnapshot *-- CameraCommon
    CameraSnapshot *-- ICameraSettings

    MaterialRegistry *-- Material
    MaterialRegistry ..> MaterialId : 発行
    Material *-- ConstantBufferData
    Material ..> PassTag

    ShaderData <|-- ConstantBufferData
    ShaderData <|-- StructuredBufferData

    class IRenderPipeline {
        <<interface>>
    }
    class IRenderDevice {
        <<interface>>
    }
```

ポイント
- 具象型を知っているのは `RenderEngine` だけ。
- `IMeshRenderer` / `ICamera` はストアに書き込むだけで、パイプラインのことは知らない。
- `Material` とデータストアはハンドル（`PipelineHandle` / `TextureHandle` / `BufferHandle`）を持つだけで、`IRenderDevice` は知らない。

---

## 2. パイプライン

```mermaid
classDiagram
    direction TB

    class IRenderPipeline {
        <<interface>>
        +Initialize(IRenderDevice&, MaterialRegistry&)*
        +Shutdown()*
        +CreateCameraSettings()* unique_ptr~ICameraSettings~
        +SetSettings(IRenderingSettings)*
        +Render(FrameSnapshot)*
    }
    class IRenderingSettings {
        <<interface>>
    }

    class ForwardPipeline {
        -IRenderDevice* m_device
        -MaterialRegistry* m_materials
        -ForwardRenderingSettings m_settings
        -CullingStage m_culling
        -DrawListBuilder m_drawListBuilder
        -vector~unique_ptr~IRenderPass~~ m_passes
        -vector~RenderView~ m_views
        -VisibleSet m_visible
        -DrawList m_drawList
        +AddPass(unique_ptr~IRenderPass~)
        -BuildViews(FrameSnapshot, vector~RenderView~&)
    }
    class ForwardRenderingSettings {
        +uint32_t shadowMapSize
        +uint32_t msaaSamples
    }
    class ForwardCameraSettings {
        +uint32_t msaaSamples
        +bool enableDepthPrepass
        +Clone() unique_ptr~ICameraSettings~
    }
    class ICameraSettings {
        <<interface>>
    }

    class FrameSnapshot {
        <<struct>>
        +vector~MeshRendererData~ meshes
        +vector~CameraSnapshot~ cameras
        +uint64_t frameIndex
        +float deltaTime
    }
    class RenderView {
        <<struct>>
        +Source source
        +Matrix4x4 view
        +Matrix4x4 projection
        +Matrix4x4 prevViewProjection
        +Float3 position
        +Viewport viewport
        +TextureHandle colorTarget
        +TextureHandle depthTarget
        +uint32_t cullingMask
        +bool depthOnly
        +ICameraSettings* cameraSettings
    }

    class CullingStage {
        +Cull(RenderView, FrameSnapshot, VisibleSet&)
    }
    class VisibleSet {
        <<struct>>
        +vector~uint32_t~ meshIndices
    }
    class DrawListBuilder {
        +Build(PassTag, RenderView, FrameSnapshot, VisibleSet, MaterialRegistry, DrawList&)
    }
    class DrawList {
        <<struct>>
        +PassTag pass
        +vector~DrawItem~ items
    }
    class DrawItem {
        <<struct>>
        +uint32_t meshIndex
        +MaterialId material
        +PipelineHandle pipeline
        +uint64_t sortKey
    }

    class IRenderPass {
        <<interface>>
        +GetName()* const char*
        +GetTag()* PassTag
        +AcceptsView(RenderView)* bool
        +Execute(PassContext)*
    }
    class PassContext {
        <<struct>>
        +ICommandList* cmd
        +RenderView* view
        +FrameSnapshot* frame
        +DrawList* drawList
        +MaterialRegistry* materials
        +ConstantBinding viewConstants
    }
    class ShadowPass
    class DepthPrepass
    class ForwardOpaquePass

    IRenderPipeline <|.. ForwardPipeline
    IRenderingSettings <|-- ForwardRenderingSettings
    ICameraSettings <|-- ForwardCameraSettings

    ForwardPipeline *-- ForwardRenderingSettings
    ForwardPipeline *-- CullingStage
    ForwardPipeline *-- DrawListBuilder
    ForwardPipeline *-- IRenderPass : 登録順 = 実行順
    ForwardPipeline ..> ForwardCameraSettings : 生成
    ForwardPipeline ..> FrameSnapshot : 読むだけ
    ForwardPipeline ..> RenderView : カメラ / 影から生成

    CullingStage ..> VisibleSet : 出力
    DrawListBuilder ..> DrawList : 出力
    DrawList *-- DrawItem

    IRenderPass <|.. ShadowPass
    IRenderPass <|.. DepthPrepass
    IRenderPass <|.. ForwardOpaquePass
    IRenderPass ..> PassContext
    PassContext --> DrawList
    PassContext --> RenderView
```

1ビューごとの流れ: `BuildViews` → `CullingStage::Cull` → パスごとに `AcceptsView` → `DrawListBuilder::Build`（Material に PSO が無ければ除外） → `IRenderPass::Execute`

---

## 3. RHI（IRenderDevice / ICommandList / D3D12 実装）

```mermaid
classDiagram
    direction TB

    class Handle~Tag~ {
        <<struct>>
        +uint32_t index
        +uint32_t generation
        +IsValid() bool
    }
    note for Handle~Tag~ "BufferHandle / TextureHandle / ShaderHandle\nPipelineHandle / SamplerHandle"

    class IRenderDevice {
        <<interface>>
        +CreateBuffer(BufferDesc, initialData)* BufferHandle
        +CreateTexture(TextureDesc, initialData)* TextureHandle
        +CreateShader(ShaderBlob)* ShaderHandle
        +CreatePipeline(PipelineDesc)* PipelineHandle
        +CreateSampler(SamplerDesc)* SamplerHandle
        +Destroy(handle)*
        +UpdateBuffer(BufferHandle, data, size, offset)*
        +RequestReadback(BufferHandle, offset, size)* ReadbackTicket
        +TryGetReadback(ReadbackTicket, dst, size)* bool
        +BeginCommandList(name)* ICommandList*
        +Submit(ICommandList*)*
        +BeginFrame()*
        +EndFrame()*
        +Present()*
        +GetBackBuffer()* TextureHandle
        +GetBackBufferFormat()* Format
        +GetBackBufferSize(w, h)*
        +Resize(w, h)*
        +GetFramesInFlight()* uint32_t
    }

    class ICommandList {
        <<interface>>
        +Transition(handle, ResourceState)*
        +BeginRenderPass(RenderTargets, ClearValues)*
        +EndRenderPass()*
        +SetViewport(Viewport)*
        +SetScissor(ScissorRect)*
        +SetPipeline(PipelineHandle)*
        +SetBindings(Bindings)*
        +SetVertexBuffer(BufferHandle, stride, offset)*
        +SetIndexBuffer(BufferHandle, Format, offset)*
        +Draw(...)*
        +DrawIndexed(...)*
        +CopyBuffer(...)*
        +BeginDebugEvent(name)*
        +EndDebugEvent()*
    }

    class Bindings {
        <<struct>>
        +ConstantBinding constants[3]
        +ShaderResourceBinding resources[8]
        +SamplerHandle samplers[4]
    }
    class ConstantBinding {
        <<struct>>
        +void* data
        +uint32_t size
        +BufferHandle buffer
        +uint64_t offset
    }
    class ShaderResourceBinding {
        <<struct>>
        +Kind kind
        +TextureHandle texture
        +BufferHandle buffer
    }
    class BufferDesc {
        <<struct>>
        +uint64_t size
        +uint32_t stride
        +BufferUsage usage
        +MemoryType memory
    }
    class TextureDesc {
        <<struct>>
        +width, height, depthOrArraySize, mipLevels
        +Format format
        +TextureDimension dimension
        +TextureUsage usage
    }
    class PipelineDesc {
        <<struct>>
        +ShaderHandle vertexShader
        +ShaderHandle pixelShader
        +VertexAttribute vertexLayout[8]
        +BlendMode blend
        +CullMode cull
        +depthTest / depthWrite / depthCompare
        +Format renderTargetFormats[8]
        +Format depthFormat
    }

    class D3D12RenderDevice {
        -ID3D12Device / Queue / SwapChain / Fence
        -ID3D12RootSignature 共通ルートシグネチャ
        -ResourcePool 各種（ハンドル → 実体）
        -CpuDescriptorHeap RTV / DSV / SRV / Sampler
        -GpuDescriptorRing SRV / Sampler（フレームごと）
        -UploadRing 定数用
        -vector~FrameContext~ m_frames
        -deque~PendingDestroy~ 遅延破棄
        -map~PendingReadback~ ReadBack
    }
    class D3D12CommandList {
        -ID3D12GraphicsCommandList m_list
        -D3D12RenderDevice& m_device
        +Reset(allocator)
    }
    class ResourcePool~T~ {
        +Add(T) Handle~Tag~
        +Get(Handle~Tag~) T*
        +Take(Handle~Tag~, T&) bool
    }
    class CpuDescriptorHeap {
        +Allocate() CPU_HANDLE
        +Free(CPU_HANDLE)
    }
    class GpuDescriptorRing {
        +BeginFrame(frameIndex)
        +Allocate(count, cpu, gpu)
    }
    class UploadRing {
        +BeginFrame(frameIndex)
        +Push(data, size) GPU_VA
    }
    class CreateD3D12RenderDevice {
        <<function>>
        D3D12DeviceDesc → unique_ptr~IRenderDevice~
    }
    class CompileHlsl {
        <<function>>
        source, entry, stage → vector~uint8_t~
    }

    IRenderDevice ..> ICommandList : 発行
    IRenderDevice ..> BufferDesc
    IRenderDevice ..> TextureDesc
    IRenderDevice ..> PipelineDesc
    IRenderDevice ..> Handle~Tag~ : 返す
    ICommandList ..> Bindings
    Bindings *-- ConstantBinding
    Bindings *-- ShaderResourceBinding

    IRenderDevice <|.. D3D12RenderDevice
    ICommandList <|.. D3D12CommandList
    D3D12RenderDevice *-- D3D12CommandList : フレームごとにプール
    D3D12RenderDevice *-- ResourcePool~T~
    D3D12RenderDevice *-- CpuDescriptorHeap
    D3D12RenderDevice *-- GpuDescriptorRing
    D3D12RenderDevice *-- UploadRing
    D3D12CommandList --> D3D12RenderDevice
    CreateD3D12RenderDevice ..> D3D12RenderDevice : 生成
```

ポイント
- `ID3D12*` / `D3D12_*` / `DXGI_*` が出てくるのは `RHI/D3D12` の中だけ（`D3D12RenderDevice` と `D3D12CommandList` は .cpp 内の無名名前空間に隠している）。
- 共通ルートシグネチャ: `b0` View / `b1` Material / `b2` Object（ルート CBV）、`t0..t7`（SRV テーブル）、`s0..s3`（サンプラーテーブル）。
- 動作確認用に `Samples/TriangleSample` があり、`IRenderDevice` / `ICommandList` だけを使って三角形を描く。
