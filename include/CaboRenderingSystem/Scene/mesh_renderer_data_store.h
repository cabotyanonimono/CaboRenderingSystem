#pragma once
#include <memory>
#include <vector>

#include "CaboRenderingSystem/Core/math_types.h"
#include "CaboRenderingSystem/Core/slot_pool.h"
#include "CaboRenderingSystem/Material/material_id.h"
#include "CaboRenderingSystem/RHI/rhi_types.h"

namespace CRS
{
// MeshRendererDataStoreに登録したメッシュを指すID
using MeshRendererId = Handle<struct MeshRendererTag>;

/// <summary>
/// インデックスバッファの一部を1つのマテリアルで描く範囲です。サブメッシュごとに1つ持ちます。
/// </summary>
struct DrawRange
{
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    // インデックスの値に足す頂点番号
    int32_t base_vertex = 0;
    MaterialId material;
};

/// <summary>
/// メッシュの描画に必要な最小限のデータです。ゲーム側のオブジェクトへの参照は持ちません。
/// </summary>
struct MeshRendererData
{
    BufferHandle vertex_buffer;
    BufferHandle index_buffer;
    // 同じメッシュの間で共有する頂点の並び（PSOのキャッシュのキーになる）
    std::shared_ptr<const VertexLayout> vertex_layout;
    // スキニング用のボーン行列（StructuredBuffer<float4x4>）。無効ならスキニングしない
    BufferHandle bone_buffer;
    // 前フレームのボーン行列（モーションベクトル用）。無効ならbone_bufferを使う
    BufferHandle previous_bone_buffer;
    std::vector<DrawRange> ranges;
    Matrix world;
    // 前フレームのworld。AdvanceFrameで自動的に入る（Update / UpdateTransformでは変わらない）
    Matrix previous_world;
    // メッシュのローカル座標でのAABB。worldで変換して視錐台カリングに使う
    DirectX::BoundingBox local_bounds;
    // CameraCommon::culling_maskとANDを取り、0なら描かない
    uint32_t layer_mask = 1;
    bool cast_shadow = true;
};

/// <summary>
/// メッシュの描画データを持つストアです。ゲーム側が登録・更新し、エンジンがスナップショットを取ります。
/// </summary>
class MeshRendererDataStore
{
    SlotPool<MeshRendererTag, MeshRendererData> m_data_;

public:
    /// <summary>
    /// メッシュを登録します。
    /// </summary>
    MeshRendererId Register(MeshRendererData data);
    /// <summary>
    /// メッシュを取り除きます。
    /// </summary>
    void Unregister(MeshRendererId id);
    /// <summary>
    /// まだ登録されているIDかどうかです。
    /// </summary>
    bool IsValid(MeshRendererId id) const;

    /// <summary>
    /// データを差し替えます。previous_worldは今の値を引き継ぎます。
    /// </summary>
    void Update(MeshRendererId id, MeshRendererData data);
    /// <summary>
    /// ワールド行列だけを更新します。
    /// </summary>
    void UpdateTransform(MeshRendererId id, const Matrix &world);

    /// <summary>
    /// 登録されているデータを取得します。無効なIDならnullptrを返します。
    /// </summary>
    const MeshRendererData *Get(MeshRendererId id) const;
    /// <summary>
    /// 登録されているメッシュの数です。
    /// </summary>
    size_t Count() const;

    /// <summary>
    /// 読み取り専用のコピーを作ります。
    /// </summary>
    std::vector<MeshRendererData> TakeSnapshot() const;
    /// <summary>
    /// 描画の後に呼び、全メッシュのprevious_worldを今のworldにします。
    /// 1フレームに何回Updateしても、1回もUpdateしなくても、前フレームの値がずれないようにするためです。
    /// </summary>
    void AdvanceFrame();
};
}
