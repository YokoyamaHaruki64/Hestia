# Editor
---
Application.md は HESTIA_EDITOR の場合に Application が Editor を値として持つ。
Runtime と Game が形になってから詳細設計することを決定した（U08）。下記の lifecycle は仮置きのまま維持する。

## Editor
---
**役割**

Application が管理する Editor の状態と UI 操作をまとめる。以下の lifecycle と描画入口は追加草案。

```cpp
class Editor
{
public:
    bool Initialize(HWND window);
    void FrameExecute();
    void Finalize();

    void ProcessMessage(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

private:
    // 未確定：編集する情報への参照とUI状態の主要メンバ
};
```

[Application](./Application.md#application)  
[Engine](./Engine.md#engine)

**所有・参照**：Application が HESTIA_EDITOR の場合に Editor を値として所有する点だけがノート由来。HWND は Window への非所有の関連付けとする提案。Engine／Game の情報をどう参照するかは未確定。

**API とメンバ**：ProcessMessage は Application の WindowProc からの void の通知を受け取る（U13）。Initialize／Finalize は UI の利用開始・終了、FrameExecute は Editor UI の一回の処理を表す仮置き。編集対象の API はまだ設けない。

**未確定（U08）**：Editor の DLL／exe 配置、Engine と Game の情報経路、描画・Window Message の接続、主要状態の型は記載されていない。

**提案**：Application の所有を維持し、Runtime と Game が形になった後で Editor の配置と編集対象を確認する。配置が決まるまで Engine.dll への組み込みや Editor 専用 Boundary API を追加しない。

**未確定（U08）**：Application と Engine のフレーム処理のどこで Editor を実行するかは記載されていない。

**提案**：編集対象と描画経路を決めた後に通常の初期化・フレーム・破棄順へ一度ずつ配置する。Command／Undo-Redo、GPU 借用、ImGui backend の接続方式を過去資料から採用しない。
