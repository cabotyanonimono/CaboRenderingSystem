#pragma once
// CaboRenderingSystem.dll から公開するクラスや関数に付けるマクロです。
// DLLのビルド中は dllexport、利用側では dllimport になります。
// CaboRenderingSystem_EXPORTS は、CMakeがDLLのビルド時に自動で定義します。
#if defined(CaboRenderingSystem_EXPORTS)
#define CRS_API __declspec(dllexport)
#else
#define CRS_API __declspec(dllimport)
#endif

// 公開クラスが標準ライブラリの型をメンバに持つことへの警告（同じコンパイラ・同じ設定で使う前提なので抑制）
#pragma warning(disable : 4251)
