# Timeの型

[処理・構造](README.md)

| 型 | 内容と契約 | 定義 |
| --- | --- | --- |
| `TimeData` | float秒のdeltaTime、unscaledDeltaTime、totalTime、unscaledTotalTime、fixedDeltaTime、unscaledFixedDeltaTimeと倍率timeScale。時間は0、倍率は1で開始 | [TimeData.h](../../../Engine/Include/Time/TimeData.h) |
| `TimeSystem` | TimeDataを値所有。Update / UpdateFixedで書き込み、static getterで現在instanceを読む。初期化前・終了後の利用は許容しない | [TimeSystem.h](../../../Engine/Include/Time/TimeSystem.h) |
| `TimeAPI` | TimeSystem*とconst TimeData*を借用。Initializeで接続、Finalizeで解除。DLL利用側に読み取りと倍率変更を公開する | [TimeAPI.h](../../../Engine/Include/Time/TimeAPI.h) |

APIを利用する期間中はSystemが生存し初期化済みであること。getterのnullチェックはassertであり、未接続時の既定値返却は契約に含まない。
