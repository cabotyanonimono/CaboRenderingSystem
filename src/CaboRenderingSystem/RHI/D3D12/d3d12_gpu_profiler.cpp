#include "pch.h"
#include "CaboRenderingSystem/RHI/D3D12/d3d12_gpu_profiler.h"

#include "CaboRenderingSystem/Core/log.h"

namespace CRS
{
uint32_t D3D12GpuProfiler::QueryBase(const uint32_t frame_index) const
{
    return frame_index * kQueriesPerFrame;
}

bool D3D12GpuProfiler::Init(ID3D12Device *device, ID3D12CommandQueue *queue)
{
    if (FAILED(queue->GetTimestampFrequency(&m_frequency_)) || m_frequency_ == 0)
    {
        LogError("GpuProfiler: failed to get timestamp frequency");
        return false;
    }

    D3D12_QUERY_HEAP_DESC heap_desc = {};
    heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heap_desc.Count = kQueriesPerFrame * kMaxFramesInFlight;
    if (FAILED(device->CreateQueryHeap(&heap_desc, IID_PPV_ARGS(&m_query_heap_))))
    {
        LogError("GpuProfiler: failed to create query heap");
        return false;
    }
    m_query_heap_->SetName(L"CRS GpuProfiler QueryHeap");

    const CD3DX12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE_READBACK);
    const auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint64_t) * kQueriesPerFrame);
    for (auto &readback : m_readbacks_)
    {
        if (FAILED(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc,
                                                   D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                   IID_PPV_ARGS(&readback))))
        {
            LogError("GpuProfiler: failed to create readback buffer");
            m_query_heap_.Reset();
            return false;
        }
        readback->SetName(L"CRS GpuProfiler Readback");
    }
    return true;
}

void D3D12GpuProfiler::BeginFrame(const uint32_t frame_index, ID3D12GraphicsCommandList *list)
{
    m_frame_index_ = frame_index;
    m_active_ = m_requested_enabled_ && m_query_heap_ != nullptr;
    m_scope_stack_.clear();

    auto &frame = m_frames_[frame_index];
    frame.scopes.clear();
    frame.query_count = 2;
    frame.draw_calls = 0;
    frame.frame_number = m_frame_counter_++;
    frame.pending = false;

    if (m_active_)
        list->EndQuery(m_query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QueryBase(frame_index));
}

void D3D12GpuProfiler::EndFrame(ID3D12GraphicsCommandList *list)
{
    if (!m_active_)
        return;

    while (!m_scope_stack_.empty())
        EndScope(list);

    auto &frame = m_frames_[m_frame_index_];
    const auto base = QueryBase(m_frame_index_);
    list->EndQuery(m_query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, base + 1);
    list->ResolveQueryData(m_query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, base, frame.query_count,
                           m_readbacks_[m_frame_index_].Get(), 0);
    frame.pending = true;
    m_active_ = false;
}

void D3D12GpuProfiler::BeginScope(ID3D12GraphicsCommandList *list, const char *name)
{
    if (!m_active_)
        return;

    auto &frame = m_frames_[m_frame_index_];
    if (frame.scopes.size() >= kMaxScopesPerFrame)
    {
        m_scope_stack_.push_back(kInvalidScope);
        return;
    }

    ScopeRecord scope;
    scope.name = name;
    scope.depth = static_cast<uint32_t>(m_scope_stack_.size());
    scope.begin_query = frame.query_count++;
    scope.end_query = frame.query_count++;
    list->EndQuery(m_query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QueryBase(m_frame_index_) + scope.begin_query);

    m_scope_stack_.push_back(static_cast<uint32_t>(frame.scopes.size()));
    frame.scopes.push_back(std::move(scope));
}

void D3D12GpuProfiler::EndScope(ID3D12GraphicsCommandList *list)
{
    if (!m_active_)
        return;
    if (m_scope_stack_.empty())
    {
        LogWarning("GpuProfiler: EndProfileScope called without BeginProfileScope");
        return;
    }

    const auto index = m_scope_stack_.back();
    m_scope_stack_.pop_back();
    if (index == kInvalidScope)
        return;

    const auto &scope = m_frames_[m_frame_index_].scopes[index];
    list->EndQuery(m_query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QueryBase(m_frame_index_) + scope.end_query);
}

void D3D12GpuProfiler::CountDraw(const uint64_t vertices, const uint64_t indices)
{
    if (!m_active_)
        return;

    auto &frame = m_frames_[m_frame_index_];
    ++frame.draw_calls;
    for (const auto index : m_scope_stack_)
    {
        if (index == kInvalidScope)
            continue;
        auto &scope = frame.scopes[index];
        ++scope.draw_calls;
        scope.vertices += vertices;
        scope.indices += indices;
    }
}

void D3D12GpuProfiler::Collect(const uint32_t frame_index)
{
    auto &frame = m_frames_[frame_index];
    if (!frame.pending)
        return;
    frame.pending = false;

    const D3D12_RANGE read_range = {0, sizeof(uint64_t) * frame.query_count};
    void *mapped = nullptr;
    if (FAILED(m_readbacks_[frame_index]->Map(0, &read_range, &mapped)))
        return;
    const auto timestamps = static_cast<const uint64_t *>(mapped);

    const double to_ms = 1000.0 / static_cast<double>(m_frequency_);
    const auto elapsed = [&](const uint32_t begin, const uint32_t end) {
        return timestamps[end] > timestamps[begin]
                   ? static_cast<double>(timestamps[end] - timestamps[begin]) * to_ms
                   : 0.0;
    };

    GpuProfileResult result;
    result.frame_number = frame.frame_number;
    result.frame_gpu_ms = elapsed(0, 1);
    result.draw_calls = frame.draw_calls;
    result.scopes.reserve(frame.scopes.size());
    for (const auto &scope : frame.scopes)
    {
        GpuProfileScope out;
        out.name = scope.name;
        out.depth = scope.depth;
        out.gpu_ms = elapsed(scope.begin_query, scope.end_query);
        out.draw_calls = scope.draw_calls;
        out.vertices = scope.vertices;
        out.indices = scope.indices;
        result.scopes.push_back(std::move(out));
    }

    const D3D12_RANGE write_range = {0, 0};
    m_readbacks_[frame_index]->Unmap(0, &write_range);

    m_latest_ = std::move(result);
    m_has_latest_ = true;
}

bool D3D12GpuProfiler::GetLatest(GpuProfileResult &out_result) const
{
    if (!m_has_latest_)
        return false;
    out_result = m_latest_;
    return true;
}
}
