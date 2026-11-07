#pragma once
#include <array>
#include <string>
#include <vector>

#include "CaboRenderingSystem/RHI/gpu_profile.h"
#include "CaboRenderingSystem/RHI/D3D12/d3d12_common.h"

namespace CRS
{
/// <summary>
/// タイムスタンプクエリで区間ごとのGPU時間を測ります。フレームごとにクエリとReadbackバッファを持ち、
/// そのフレームのGPU処理が終わった後（Collect）に結果を読みます。
/// </summary>
class D3D12GpuProfiler
{
    static constexpr uint32_t kMaxScopesPerFrame = 256;
    static constexpr uint32_t kQueriesPerFrame = kMaxScopesPerFrame * 2 + 2;
    static constexpr uint32_t kInvalidScope = UINT32_MAX;

    struct ScopeRecord
    {
        std::string name;
        uint32_t depth = 0;
        uint32_t begin_query = 0;
        uint32_t end_query = 0;
        uint32_t draw_calls = 0;
        uint64_t vertices = 0;
        uint64_t indices = 0;
    };

    struct FrameRecord
    {
        std::vector<ScopeRecord> scopes;
        uint32_t query_count = 0;
        uint32_t draw_calls = 0;
        uint64_t frame_number = 0;
        bool pending = false;
    };

    ComPtr<ID3D12QueryHeap> m_query_heap_;
    std::array<ComPtr<ID3D12Resource>, kMaxFramesInFlight> m_readbacks_;
    std::array<FrameRecord, kMaxFramesInFlight> m_frames_;
    std::vector<uint32_t> m_scope_stack_;
    uint64_t m_frequency_ = 0;
    uint64_t m_frame_counter_ = 0;
    uint32_t m_frame_index_ = 0;
    bool m_requested_enabled_ = false;
    bool m_active_ = false;
    bool m_has_latest_ = false;
    GpuProfileResult m_latest_;

    uint32_t QueryBase(uint32_t frame_index) const;

public:
    /// <summary>
    /// クエリヒープとReadbackバッファを作ります。失敗した場合はfalseを返し、プロファイラは使えません。
    /// </summary>
    bool Init(ID3D12Device *device, ID3D12CommandQueue *queue);

    /// <summary>
    /// 有効 / 無効を切り替えます。次のBeginFrameから反映します。
    /// </summary>
    void SetEnabled(bool enabled)
    {
        m_requested_enabled_ = enabled;
    }

    /// <summary>
    /// 有効にするよう設定されているかどうかです。
    /// </summary>
    bool IsEnabled() const
    {
        return m_requested_enabled_;
    }

    /// <summary>
    /// フレームの記録を始め、フレーム全体の開始時刻を書き込みます。
    /// </summary>
    void BeginFrame(uint32_t frame_index, ID3D12GraphicsCommandList *list);
    /// <summary>
    /// 閉じていない区間を閉じ、フレーム全体の終了時刻を書いて、クエリをReadbackバッファへ解決します。Closeの前に呼びます。
    /// </summary>
    void EndFrame(ID3D12GraphicsCommandList *list);

    /// <summary>
    /// 区間を始めます。1フレームの上限を超えた区間は測りません。
    /// </summary>
    void BeginScope(ID3D12GraphicsCommandList *list, const char *name);
    /// <summary>
    /// 一番内側の区間を終えます。
    /// </summary>
    void EndScope(ID3D12GraphicsCommandList *list);
    /// <summary>
    /// 描画1回分を、開いているすべての区間に加えます。
    /// </summary>
    void CountDraw(uint64_t vertices, uint64_t indices);

    /// <summary>
    /// frame_indexのGPU処理が終わった後に呼び、結果を読んで最新の結果にします。
    /// </summary>
    void Collect(uint32_t frame_index);

    /// <summary>
    /// 最新の結果を返します。まだなければfalseです。
    /// </summary>
    bool GetLatest(GpuProfileResult &out_result) const;
};
}
