#pragma once
#include "CaboRenderingSystem/export.h"
#include <array>
#include <optional>

#include "CaboRenderingSystem/Core/slot_pool.h"
#include "CaboRenderingSystem/Material/material_id.h"
#include "CaboRenderingSystem/Material/shader_parameters.h"
#include "CaboRenderingSystem/RHI/i_render_device.h"

namespace CRS
{
/// <summary>
/// 1つのパスで使うシェーダーと描画設定です。PSOそのものは持たず、描画時に描画先の設定と合わせて引きます。
/// </summary>
struct MaterialPass
{
    ShaderHandle vertex_shader;
    // 無効なら深度だけを描く（影のパスなど）
    ShaderHandle pixel_shader;
    RenderState render_state;
};

/// <summary>
/// 今のパイプライン用のマテリアルです。パスごとのシェーダー、定数、テクスチャを持ちます。
/// 定数とテクスチャは名前で扱い、位置はシェーダーのリフレクションから引きます。
/// space0 の cbuffer は名前に関係なくすべてマテリアルの定数です（b0..b3 まで）。
/// </summary>
class CRS_API Material : public ShaderParameters
{
    // パスごとのシェーダーと描画設定。使わないパスは空
    std::array<std::optional<MaterialPass>, static_cast<size_t>(PassTag::Count)> m_passes_;

    // パスの構成が変わったら進める。PSOのキャッシュの無効化に使う
    uint32_t m_version_ = 0;

public:
    // 描く順番のグループ。小さい順に描く（同じグループの中では、不透明は手前から、半透明は奥から）
    uint16_t render_queue = 2000;

    /// <summary>
    /// パスのシェーダーと描画設定を設定します。プロパティ表は変わらないので、ForwardパスならBuildPropertyTableも呼んでください。
    /// </summary>
    void SetPass(PassTag tag, const MaterialPass &pass);
    /// <summary>
    /// パスを外します。外したパスでは、このマテリアルを使うメッシュは描かれません。
    /// </summary>
    void RemovePass(PassTag tag);
    /// <summary>
    /// パスの設定を取得します。設定されていないパスならnullptrを返します。
    /// </summary>
    const MaterialPass *GetPass(PassTag tag) const;

    /// <summary>
    /// メインのパス（Forward）の頂点・ピクセルシェーダーのリフレクションから、プロパティ表を作り直します。
    /// 既に書き込まれている値は、名前と型が同じであれば引き継ぎます。
    /// </summary>
    void BuildPropertyTable(const IRenderDevice &device);

    /// <summary>
    /// パスの構成が変わるたびに増える番号です。PSOのキャッシュのキーに含めます。
    /// </summary>
    uint32_t Version() const
    {
        return m_version_;
    }
};

/// <summary>
/// マテリアルの本体を持ちます。メッシュのデータはMaterialIdだけを持ち、ここで引きます。
/// </summary>
class CRS_API MaterialRegistry
{
public:
    MaterialRegistry() = default;
    // DLLから公開するクラスはコピー関数も実体化されるので、コピーできないことを明示する
    MaterialRegistry(const MaterialRegistry &) = delete;
    MaterialRegistry &operator=(const MaterialRegistry &) = delete;

private:
    SlotPool<MaterialTag, Material> m_materials_;

public:
    /// <summary>
    /// マテリアルを登録します。
    /// </summary>
    MaterialId Add(Material material);
    /// <summary>
    /// マテリアルを取り除きます。このIDを持つメッシュは、そのサブメッシュを描かなくなります。
    /// </summary>
    void Remove(MaterialId id);
    /// <summary>
    /// マテリアルを取得します。無効なIDならnullptrを返します。
    /// </summary>
    Material *Get(MaterialId id);
    const Material *Get(MaterialId id) const;
    /// <summary>
    /// 登録されているマテリアルの数です。
    /// </summary>
    size_t Count() const;
};
}
