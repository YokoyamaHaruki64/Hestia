/*=============================================================================

 File   : ScriptGeneration.h
 Desc   : ビルドと保存監視で共有するScript生成の入口

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _SCRIPT_GENERATION_H_
#define _SCRIPT_GENERATION_H_

#include <filesystem>
#include <set>
#include <span>

/// @brief Windows上で同じパスを指す通知と生成対象を照合する
struct ScriptPathLess
{
    bool operator()(const std::filesystem::path& left, const std::filesystem::path& right) const;
};

using ScriptPaths = std::set<std::filesystem::path, ScriptPathLess>;

/// @brief 1プロジェクトの生成先とキャッシュを指定する
struct ScriptGenerationPaths
{
    std::filesystem::path m_assets;
    std::filesystem::path m_output;
    std::filesystem::path m_cache;
};

/// @brief 生成前に行うメタデータ処理
enum class ScriptMetadataPreparation
{
    Existing,
    Missing,
    Scan
};

/// @brief 登録順にScriptを検証し生成する。変更通知を受けたパスは時刻が同じでも読み直す
/// @param paths 入力ルート、出力先、キャッシュファイル。キャッシュなしは空パス
/// @param inputs 登録順のAsset相対パス
/// @param metadata 生成前にMetaToolを呼ぶ条件
/// @param changed 保存通知から確定したAsset相対パス
/// @return 生成成功: true / 診断済みの失敗: false
bool GenerateScripts(const ScriptGenerationPaths& paths,
    std::span<const std::filesystem::path> inputs, ScriptMetadataPreparation metadata,
    const ScriptPaths& changed = {});

/// @brief カレントディレクトリのプロジェクトをCtrl+Cまで監視する
/// @param paths 監視と単発生成で共有するパス
/// @return 正常終了: 0 / 監視を継続できない場合: 1
int WatchScripts(const ScriptGenerationPaths& paths);

#endif // _SCRIPT_GENERATION_H_
