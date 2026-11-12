#pragma once
#include "CaboRenderingSystem/export.h"
#include <array>
#include <initializer_list>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "CaboRenderingSystem/Core/math_types.h"
#include "CaboRenderingSystem/RHI/i_render_device.h"

namespace CRS
{
/// <summary>
/// 定数の1つの変数の位置です。slotは space0 の b レジスタ番号です。
/// </summary>
struct MaterialProperty
{
    uint32_t slot = 0;
    uint32_t offset = 0;
    uint32_t size = 0;
    ShaderVariableType type = ShaderVariableType::Unknown;
};

/// <summary>
/// シェーダーのリフレクションから作った名前の表と、space0 の定数・リソース・サンプラー・UAVの値を持ちます。
/// マテリアルとコンピュートシェーダーで共通に使い、名前で値を設定してBindingsに詰めます。
/// </summary>
class CRS_API ShaderParameters
{
    // 名前から、定数の位置 / リソースのtレジスタ / サンプラーのsレジスタ / UAVのuレジスタを引く表
    std::unordered_map<std::string, MaterialProperty> m_properties_;
    std::unordered_map<std::string, uint32_t> m_resource_slots_;
    std::unordered_map<std::string, uint32_t> m_sampler_slots_;
    std::unordered_map<std::string, uint32_t> m_unordered_access_slots_;
    // レジスタごとの、シェーダーが宣言しているリソースの種類（宣言されていないスロットは空）
    std::array<std::optional<ShaderResourceKind>, kResourceSlotCount> m_resource_kinds_;
    std::array<std::optional<ShaderResourceKind>, kUnorderedAccessSlotCount> m_unordered_access_kinds_;

    std::array<std::vector<uint8_t>, kConstantSlotCount> m_constants_;
    std::array<ResourceBinding, kResourceSlotCount> m_resources_ = {};
    std::array<SamplerHandle, kSamplerSlotCount> m_samplers_ = {};
    std::array<UnorderedAccessBinding, kUnorderedAccessSlotCount> m_unordered_access_ = {};

    // 型と大きさを確かめてから、変数の位置に書き込む
    bool Write(const std::string &name, ShaderVariableType type, const void *data, uint32_t size);

protected:
    /// <summary>
    /// シェーダーのリフレクションから名前の表を作り直します。既に書き込まれている定数は、名前と型が同じであれば引き継ぎます。
    /// allow_unordered_accessがfalseなら、RWTextureなどのUAVは警告を出して表に入れません。
    /// </summary>
    void BuildTable(const IRenderDevice &device, std::initializer_list<ShaderHandle> shaders,
                    bool allow_unordered_access);

public:
    // ---- 指定された
    // 名前で値を書き込みます。名前がない場合や型が違う場合はfalseを返します ----
    bool SetInt(const std::string &name, int32_t value);
    bool SetUInt(const std::string &name, uint32_t value);
    bool SetFloat(const std::string &name, float value);
    bool SetVector(const std::string &name, const Vector4 &value);
    bool SetColor(const std::string &name, const Color &value);
    bool SetMatrix(const std::string &name, const Matrix &value);
    /// <summary>
    /// 型を問わず、変数の位置にバイト列を書き込みます。sizeが変数より大きい場合は変数の大きさまで書きます。
    /// </summary>
    bool SetData(const std::string &name, const void *data, uint32_t size);
    /// <summary>
    /// 指定された名前のテクスチャを設定します。tレジスタのTexture2D / TextureCube、またはuレジスタのRWTextureに設定します。
    /// </summary>
    bool SetTexture(const std::string &name, TextureHandle texture);
    /// <summary>
    /// 指定された名前のバッファを設定します。tレジスタのStructuredBuffer / ByteAddressBuffer、またはuレジスタのRWバッファに設定します。
    /// </summary>
    bool SetBuffer(const std::string &name, BufferHandle buffer);
    /// <summary>
    /// 指定された名前のサンプラー（space0のsレジスタ）を設定します。
    /// </summary>
    bool SetSampler(const std::string &name, SamplerHandle sampler);

    /// <summary>
    /// シェーダーにその名前の定数があるかどうかです。
    /// </summary>
    bool HasProperty(const std::string &name) const;
    /// <summary>
    /// シェーダーにその名前のリソース（テクスチャ、バッファ、UAV）があるかどうかです。
    /// </summary>
    bool HasResource(const std::string &name) const;
    /// <summary>
    /// tレジスタのslot番にリソースが設定されているかどうかです。
    /// </summary>
    bool IsResourceSet(uint32_t slot) const;

    /// <summary>
    /// 定数を、space0 の b レジスタの位置に設定します。
    /// </summary>
    void BindConstants(Bindings &bindings) const;
    /// <summary>
    /// リソースを、space0 の t レジスタの位置に設定します。
    /// 設定されていないTexture2Dのスロットと、シェーダーが宣言していないスロットにはdefault_textureを入れます。
    /// それ以外（TextureCube、バッファ）は空のままにします（シェーダーからは0が読めます）。
    /// </summary>
    void BindResources(Bindings &bindings, TextureHandle default_texture) const;
    /// <summary>
    /// UAVを、space0 の u レジスタの位置に設定します。
    /// </summary>
    void BindUnorderedAccess(Bindings &bindings) const;
    /// <summary>
    /// 定数・リソース・サンプラー・UAVをまとめて設定します。
    /// </summary>
    void Bind(Bindings &bindings, TextureHandle default_texture = {}) const;

    /// <summary>
    /// s0..s3の位置に並べたサンプラーです。
    /// </summary>
    const std::array<SamplerHandle, kSamplerSlotCount> &Samplers() const
    {
        return m_samplers_;
    }
};
}
