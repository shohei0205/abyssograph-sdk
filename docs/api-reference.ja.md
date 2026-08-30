# Abyssograph アドオン API リファレンス

| 項目 | 内容 |
|---|---|
| ABI バージョン | 2（`ABYSSO_ADDON_ABI_VERSION`） |
| 必須ヘッダ | `include/AbyssoAddon.h` |
| 任意ヘッダ | `include/AbyssoAddonKit.h`（実装サポート用） |
| アドオン形式 | Windows x64 ネイティブ DLL |
| 言語 | C++17（公開インタフェースは C リンケージ） |

English: [api-reference.en.md](api-reference.en.md)

---

## 目次

- [1. 概要](#1-概要)
- [2. 読み込みの流れ](#2-読み込みの流れ)
- [3. 共通規約](#3-共通規約)
  - [3.1 リンクと呼び出し規約](#31-リンクと呼び出し規約)
  - [3.2 GUID](#32-guid)
  - [3.3 structSize](#33-structsize)
  - [3.4 戻り値とエラーコード](#34-戻り値とエラーコード)
  - [3.5 文字列の扱い](#35-文字列の扱い)
  - [3.6 スレッド](#36-スレッド)
- [4. エクスポート関数](#4-エクスポート関数)
  - [4.1 GetAbyssoAddonAbiVersion](#41-getabyssoaddonabiversion)
  - [4.2 GetAbyssoAddonInfo](#42-getabyssoaddoninfo)
  - [4.3 InitializeAbyssoAddon / ShutdownAbyssoAddon](#43-initializeabyssoaddon--shutdownabyssoaddon)
  - [4.4 SetAbyssoAddonLang](#44-setabyssoaddonlang)
  - [4.5 項目ツリーの列挙](#45-項目ツリーの列挙)
  - [4.6 GetAbyssoAddonProp](#46-getabyssoaddonprop)
  - [4.7 SetAbyssoAddonValue](#47-setabyssoaddonvalue)
  - [4.8 ResetAbyssoAddonValue](#48-resetabyssoaddonvalue)
- [5. 描画](#5-描画)
  - [5.1 AbyssoRenderRequest](#51-abyssorenderrequest)
  - [5.2 座標系](#52-座標系)
  - [5.3 出力画素](#53-出力画素)
  - [5.4 qualityDivisor](#54-qualitydivisor)
  - [5.5 flags](#55-flags)
  - [5.6 timeSeconds](#56-timeseconds)
  - [5.7 描画の中断](#57-描画の中断)
  - [5.8 再現性](#58-再現性)
- [6. 情報オーバーレイ](#6-情報オーバーレイ)
- [7. AbyssoAddonKit.h](#7-abyssoaddonkith)
  - [7.1 最小構成](#71-最小構成)
  - [7.2 abysso::Text](#72-abyssotext)
  - [7.3 abysso::Request](#73-abyssorequest)
  - [7.4 abysso::AddonKit](#74-abyssoaddonkit)
  - [7.5 情報オーバーレイ](#75-情報オーバーレイ)
  - [7.6 中断](#76-中断)
  - [7.7 マクロ](#77-マクロ)
- [8. 配置と動作確認](#8-配置と動作確認)
- [9. よくある問題](#9-よくある問題)
- [10. サンプル](#10-サンプル)

## 1. 概要

Abyssograph のアドオンは、描画アルゴリズムを実装するための Windows ネイティブ DLL です。
ホストから渡される描画要求に対して計算・正規化・着色を行い、1 フレーム分の画素データを生成して返します。

描画対象はマンデルブロ集合である必要はありません。ホストが提供するのは視点情報と出力サイズのみであり、
何をどのように描画するかはアドオンが決定します。

アドオンは自身の設定項目をツリー構造として公開します。ホストは各項目の意味を解釈せず、項目型のみを参照して UI を構築します。
たとえばホストにとって「パレット」や「正規化方式」は特別な概念ではなく、いずれも単なる ComboBox 項目です。

アドオンからホストへコールバックする仕組みはありません。初期化情報・言語設定・描画要求は、
すべてホストからアドオンへ一方向に渡されます。そのため SDK はヘッダファイルのみで完結します。

## 2. 読み込みの流れ

ホスト（`AbyssographHost.dll`）は、自身と同じフォルダにある DLL を順に読み込み、次の手順でアドオンを検出します。

1. `GetAbyssoAddonAbiVersion` の有無を確認します。存在しない場合は通常の DLL とみなし、アドオン一覧にも表示しません
2. 必須エクスポート関数（`GetAbyssoAddonAbiVersion` / `GetAbyssoAddonInfo` / `RenderAbyssoAddonFrame`）の有無を確認します。欠けている場合は読み込みません
3. `GetAbyssoAddonAbiVersion()` の戻り値が `ABYSSO_ADDON_ABI_VERSION` と一致するかを確認します。一致しない場合は読み込みません
4. `InitializeAbyssoAddon()` が存在する場合は呼び出します。負の値を返した場合はそのアドオンを無効とします
5. `SetAbyssoAddonLang()` が存在する場合は呼び出します
6. `GetAbyssoAddonInfo()` を呼び出してアドオン GUID を取得します。同一 GUID のアドオンが読み込み済みの場合、後から読み込まれた側を無効とします
7. 項目ツリーを列挙して GUID の索引を作成します。項目 GUID が他のアドオンと重複した場合、そのアドオン全体を無効とします

利用者がそのアドオンを選択している間、ホストは次の処理を繰り返します。

- 項目ツリーと属性を取得してパラメータ UI を構築する
- 値が変更されるたびに `SetAbyssoAddonValue()` を呼び出し、戻り値の変更通知フラグに従って UI と表示を更新する
- 画像が必要になるたびに `RenderAbyssoAddonFrame()` を呼び出す
- 情報オーバーレイに表示する値を取得する

アプリケーション終了時には、`ShutdownAbyssoAddon()` が存在する場合はそれを呼び出したうえで DLL を解放します。

## 3. 共通規約

### 3.1 リンクと呼び出し規約

- すべてのエクスポート関数は C リンケージとします。`ABYSSO_ADDON_API` が `extern "C"` と `__declspec(dllexport)` を付与します
- DLL は x64 ターゲットでビルドしてください。Abyssograph は x64 アプリケーションのため、x86 DLL は読み込まれません
- CRT は静的リンク（`/MT`）を推奨します。アドオン間でメモリの確保と解放をまたがないため、ランタイムが分かれていても問題ありません
- C ABI 境界を越えて例外を送出しないでください。ネイティブ例外が DLL 境界を越えるとアプリケーション全体が異常終了します。
  `AbyssoAddonKit.h` が生成するエクスポート関数は、内部で `catch (...)` により例外を捕捉します

### 3.2 GUID

`AbyssoGuid` は Windows の `GUID` 型に依存しない 16 バイトの識別子です。

```cpp
typedef struct { uint8_t bytes[16]; } AbyssoGuid;
```

GUID はアドオン本体、パラメータ項目、情報項目のそれぞれに必要です。
GUID は全アドオンを通じて一意でなければならず、重複が検出された場合は後から読み込まれたアドオンが無効になります。

サンプルに含まれる GUID は再利用せず、PowerShell の `New-Guid` や `uuidgen` などで新しい GUID を生成してください。
また、一度配布した GUID は変更しないでください。プロジェクトファイル（`.abysso`）では
`addon.<AddonGuid>.<ItemGuid>` の形式で保存されるため、GUID を変更すると保存済みプロジェクトとの互換性が失われます。

`AbyssoAddonKit.h` を使用する場合は、`ABYSSO_GUID` マクロで一般的な GUID 表記のまま記述できます。

```cpp
// 7276a1f7-bcbd-4496-a390-dc547f4f7407
constexpr AbyssoGuid kAddon = ABYSSO_GUID(72,76,a1,f7,bc,bd,44,96,
                                          a3,90,dc,54,7f,4f,74,07);
```

### 3.3 structSize

公開構造体の先頭には必ず `structSize` フィールドがあり、構造体を渡す側が設定します。

| 方向 | 構造体 | 実装上の注意 |
|---|---|---|
| ホスト → アドオン | `AbyssoHostInfo` / `AbyssoRenderRequest` | `structSize` の範囲外のフィールドを参照しないでください。判定には `ABYSSO_REQUEST_HAS` を使用します |
| アドオン → ホスト | `AbyssoAddonInfo` / `AbyssoParamItem` | ホストが設定した `structSize` が `sizeof` より小さい場合は `ABYSSO_ERROR_BUFFER_TOO_SMALL` を返してください |

この仕組みにより、ABI バージョンを変更せずに構造体末尾へフィールドを追加できます。
古いアドオンは短い構造体を受け取り、未知の末尾を参照しないまま動作を継続します。

### 3.4 戻り値とエラーコード

戻り値は 0 以上が成功、負の値がエラーを表します。

| 定数 | 値 | 意味 |
|---|---|---|
| `ABYSSO_ERROR_INVALID_ARG` | -1 | 不正な引数 |
| `ABYSSO_ERROR_UNKNOWN_GUID` | -2 | 存在しない GUID |
| `ABYSSO_ERROR_BUFFER_TOO_SMALL` | -3 | バッファ不足 |
| `ABYSSO_ERROR_READONLY` | -4 | 読み取り専用項目への書き込み |
| `ABYSSO_ERROR_NOT_SUPPORTED` | -5 | 未サポートの組み合わせ |

### 3.5 文字列の扱い

`GetAbyssoAddonProp()` などが返す文字列は UTF-8 の `const char*` です。ホストは所有権を取得せず、
必要に応じて内容を複製します。

| 種別 | 有効期間 |
|---|---|
| 項目名・説明 | 次に `SetAbyssoAddonLang()` が呼ばれるまで |
| 情報項目の値 | 次に同一アドオンへ問い合わせが行われるまで |

静的文字列リテラルをそのまま返して構いません。

### 3.6 スレッド

次の関数は UI スレッドから呼び出されます。

- `GetAbyssoAddonParamChildItemCount`
- `GetAbyssoAddonParamChildItem`
- `GetAbyssoAddonProp`
- `SetAbyssoAddonValue`
- `ResetAbyssoAddonValue`
- `SetAbyssoAddonLang`

`RenderAbyssoAddonFrame()` は描画スレッドから呼び出されます。描画中に設定値が変更される可能性があるため、
描画開始時に必要なパラメータをローカルへ複製してください。
`CancelAbyssoAddonFrame()` は、描画中に別スレッドから呼び出されます。
情報項目の値は描画とは異なるタイミングで取得されるため、更新はアトミックに行ってください。

パラメータツリーの `Static` 項目の値も、上記のとおり UI スレッドから取得されます。
描画スレッドが算出した値を `Static` に表示する場合は、`std::atomic` などを介して受け渡してください。
情報オーバーレイ（6 章）と異なり、`Static` 項目の値には二重化の仕組みがないため、
同期はアドオン側の責任です。

## 4. エクスポート関数

必須の関数は `GetAbyssoAddonAbiVersion` / `GetAbyssoAddonInfo` / `RenderAbyssoAddonFrame` の 3 つです。
それ以外はすべて任意で、エクスポートされていない場合、ホストはその機能をサポートしていないものとして扱います。
ホストが不足分を補完することはありません。

| 関数 | 要否 | 未実装時の扱い |
|---|---|---|
| `GetAbyssoAddonAbiVersion` | 必須 | DLL をアドオンとして認識しない |
| `GetAbyssoAddonInfo` | 必須 | 読み込まない |
| `RenderAbyssoAddonFrame` | 必須 | 読み込まない |
| `InitializeAbyssoAddon` | 任意 | 初期化不要とみなす |
| `ShutdownAbyssoAddon` | 任意 | 終了処理不要とみなす |
| `SetAbyssoAddonLang` | 任意 | 言語切り替えに非対応とみなし、呼び出さない |
| `GetAbyssoAddonParamChildItemCount` | 任意 | 子項目数 0 とみなす |
| `GetAbyssoAddonParamChildItem` | 任意 | 同上 |
| `GetAbyssoAddonProp` | 任意 | パラメータ項目・情報項目とも無しとみなす |
| `SetAbyssoAddonValue` | 任意 | 全項目を読み取り専用として扱う |
| `ResetAbyssoAddonValue` | 任意 | `ABYSSO_CHANGE_NONE` を返したものとみなす |
| `CancelAbyssoAddonFrame` | 任意 | 中断に非対応とみなす |
| `GetAbyssoAddonInfoItemCount` | 任意 | 情報項目数 0 とみなす |
| `GetAbyssoAddonInfoItem` | 任意 | 同上 |

次の組み合わせは、一方だけを実装しても機能しません。

- `GetAbyssoAddonParamChildItemCount` と `GetAbyssoAddonParamChildItem`
- `GetAbyssoAddonInfoItemCount` と `GetAbyssoAddonInfoItem`
- 上記いずれの機能にも `GetAbyssoAddonProp` が必要です

### 4.1 GetAbyssoAddonAbiVersion

```cpp
ABYSSO_ADDON_API int32_t GetAbyssoAddonAbiVersion(void);
```

必須。アドオンが対応する ABI バージョンを返します。実装は通常 `ABYSSO_ADDON_ABI_VERSION` をそのまま返します。

ホストが最初に呼び出す関数であり、初期化前に呼び出されます。この関数の有無が、DLL をアドオンとして認識するかどうかの判定に使われます。

### 4.2 GetAbyssoAddonInfo

```cpp
ABYSSO_ADDON_API int32_t GetAbyssoAddonInfo(AbyssoAddonInfo* out);
```

必須。アドオンの識別情報を返します。

```cpp
typedef struct
{
    int32_t     structSize;
    AbyssoGuid  guid;
    const char* name;
    const char* version;
} AbyssoAddonInfo;
```

| フィールド | 説明 |
|---|---|
| `structSize` | ホストが設定します |
| `guid` | アドオンを一意に識別する GUID |
| `name` | 表示名（UTF-8） |
| `version` | バージョン文字列。書式は任意 |

`name` には DLL 名ではなく、利用者に表示する名称（描画対象を表す名前）を返してください。
`SetAbyssoAddonLang()` を実装している場合は、現在の言語設定に応じた名称を返して構いません。

### 4.3 InitializeAbyssoAddon / ShutdownAbyssoAddon

```cpp
ABYSSO_ADDON_API int32_t InitializeAbyssoAddon(const AbyssoHostInfo* host);
ABYSSO_ADDON_API void    ShutdownAbyssoAddon(void);
```

どちらも任意。テーブル生成、キャッシュの初期化、設定ファイルの読み込みなどに使用できます。

```cpp
typedef struct
{
    int32_t     structSize;
    int32_t     abiVersion;   // ABYSSO_ADDON_ABI_VERSION
    const char* hostVersion;  // ホストの識別名（参考情報）
} AbyssoHostInfo;
```

`InitializeAbyssoAddon()` はアドオン読み込み時に 1 回だけ呼び出されます。成功時は 0 以上を返してください。
負の値を返した場合、そのアドオンは読み込みに失敗したものとして扱われます。

`ShutdownAbyssoAddon()` はアプリケーション終了時に呼び出されます。確保したリソースを解放してください。

### 4.4 SetAbyssoAddonLang

```cpp
ABYSSO_ADDON_API void SetAbyssoAddonLang(const char* bcp47);
```

任意。表示言語を BCP 47 言語タグ（`en-US`、`ja-JP` など）で通知します。既定は `en-US` です。

呼び出し後、項目名・項目説明・情報項目の値は指定された言語で返してください。
また、それ以前に返した文字列へのポインタは無効になって構いません。
単一言語のみを提供するアドオンでは、この関数を実装する必要はありません。

### 4.5 項目ツリーの列挙

```cpp
ABYSSO_ADDON_API int32_t GetAbyssoAddonParamChildItemCount(const AbyssoGuid* target);
ABYSSO_ADDON_API int32_t GetAbyssoAddonParamChildItem(const AbyssoGuid* target,
                                                      int32_t index,
                                                      AbyssoParamItem* out);
```

両方そろって任意。パラメータツリーを列挙します。`target` が `nullptr` の場合はアドオンのルート、
それ以外は指定 GUID の項目を対象とします。

```cpp
typedef struct
{
    int32_t    structSize;
    int32_t    type;       // kAbyssoParamItemType*
    AbyssoGuid guid;
    int32_t    isReadOnly;
    int32_t    isEnabled;
} AbyssoParamItem;
```

#### 項目型

| 定数 | UI | 値 |
|---|---|---|
| `kAbyssoParamItemTypeGroup` | グループ枠 | なし |
| `kAbyssoParamItemTypeComboBox` | コンボボックス | 選択中の `ComboBoxItem` の GUID |
| `kAbyssoParamItemTypeComboBoxItem` | 選択肢 | なし（親は必ず ComboBox） |
| `kAbyssoParamItemTypeCheckBox` | チェックボックス | `int32_t`（0 / 1） |
| `kAbyssoParamItemTypeStatic` | ラベルと値 | `const char*`（読み取り専用） |
| `kAbyssoParamItemTypeDescription` | 説明文 | なし |
| `kAbyssoParamItemTypeInt32` | スライダー | `int32_t` |
| `kAbyssoParamItemTypeDouble` | スライダー | `double` |
| `kAbyssoParamItemTypeString` | テキストボックス | `const char*`（UTF-8） |

`ComboBox` の値は選択中の `ComboBoxItem` の GUID であり、プロジェクトファイルにもこの GUID が保存されます。
インデックスは保存されないため、選択肢を並べ替えたり途中に追加したりしても、
保存済みプロジェクトの選択内容はずれません。

#### isReadOnly と isEnabled

| フィールド | 意味 | 値の設定要求 | プロジェクトファイル |
|---|---|---|---|
| `isReadOnly` | 値を変更できない項目 | `ABYSSO_ERROR_READONLY` を返して拒否します | 保存されません |
| `isEnabled` | 0 のとき UI 上で無効表示にします | 拒否してはいけません | 保存されます |

`isEnabled` は表示状態のみを制御します。プロジェクトファイルの読み込みではツリー順に値が流し込まれるため、
無効状態の項目への設定要求を拒否すると、保存した値が既定値へ戻ってしまいます。

**条件によって一時的にロックする項目には `isReadOnly` を使用しないでください。**
ホストは読み取り専用の項目をプロジェクトファイルの保存対象から除外するため、
ロック中に保存すると、その項目の値はファイルに書き込まれません。
読み込み時は保存されていない項目が既定値へ戻るため、ロックを解除した時点で値が失われています。

たとえば「自動」チェックボックスが ON のあいだ「手動値」を `isReadOnly` にすると、
自動の状態で保存したプロジェクトを開き直したとき、手動値は既定値に戻っています。
この用途には `isEnabled` を使用してください。`isReadOnly` は値を持たない項目
（`Static` など）と、常に読み取り専用である項目に限定してください。

#### 制限

- 子項目数が負の値、または 4096 を超える場合、ホストは 0 として扱います
- ツリーの深さは最大 16 階層です。これを超えるとアドオン全体が無効になります
- ホストが子をたどるのは `Group` と `ComboBox` のみです

#### エラー

| 定数 | 条件 |
|---|---|
| `ABYSSO_ERROR_INVALID_ARG` | `index` が範囲外 |
| `ABYSSO_ERROR_UNKNOWN_GUID` | `target` が不明 |
| `ABYSSO_ERROR_BUFFER_TOO_SMALL` | `out->structSize` が不足 |

### 4.6 GetAbyssoAddonProp

```cpp
ABYSSO_ADDON_API int32_t GetAbyssoAddonProp(const AbyssoGuid* target, int32_t prop,
                                            void* data, int32_t dataSize);
```

任意。ただし項目を 1 つでも公開する場合は必須です。項目の属性および値を取得します。
成功時は `data` へ書き込んだバイト数を返します。

| `prop` | 書き込む型 | 説明 |
|---|---|---|
| `kAbyssoPropType` | `int32_t` | 項目型 |
| `kAbyssoPropCaption` | `const char*` | 項目名。文字列そのものではなくポインタを書き込みます |
| `kAbyssoPropDescription` | `const char*` | 説明。無い場合は空文字列 |
| `kAbyssoPropCurrentValue` | 項目型による | 現在値 |
| `kAbyssoPropDefaultValue` | 項目型による | 既定値 |
| `kAbyssoPropMinValue` | `int32_t` / `double` | 最小値（`Int32` / `Double`） |
| `kAbyssoPropMaxValue` | `int32_t` / `double` | 最大値。`String` では最大文字数（`int32_t`） |
| `kAbyssoPropStep` | `int32_t` / `double` | スライダーのステップ幅。0 の場合はスナップしません |

`kAbyssoPropCurrentValue` と `kAbyssoPropDefaultValue` で扱う型は項目型に対応します。

| 項目型 | 値の型 |
|---|---|
| `ComboBox` | `AbyssoGuid`（選択中の `ComboBoxItem` の GUID） |
| `CheckBox` / `Int32` | `int32_t` |
| `Double` | `double` |
| `String` / `Static` | `const char*`（UTF-8） |
| `Group` / `ComboBoxItem` / `Description` | 値を持たないため `ABYSSO_ERROR_NOT_SUPPORTED` |

`String` の `kAbyssoPropMaxValue` は最大文字数であり、UTF-8 のバイト数ではありません。

サポートしない組み合わせ（範囲を設定していない `Double` の最小値など）には `ABYSSO_ERROR_NOT_SUPPORTED` を、
`dataSize` が不足している場合は `ABYSSO_ERROR_BUFFER_TOO_SMALL` を返してください。

情報項目の GUID もこの関数へ渡されます（[6. 情報オーバーレイ](#6-情報オーバーレイ)）。

### 4.7 SetAbyssoAddonValue

```cpp
ABYSSO_ADDON_API int32_t SetAbyssoAddonValue(const AbyssoGuid* target,
                                             const void* data, int32_t dataSize);
```

任意。項目値を更新します。`data` の形式は `kAbyssoPropCurrentValue` と同じで、
`ComboBox` には選択する `ComboBoxItem` の `AbyssoGuid` が渡されます。

実装しない場合、ホストはこのアドオンの全項目を読み取り専用として UI に渡します。
表示のみを行うアドオンは、この関数を実装しないことで実現できます。

成功時はエラーコードではなく、変更通知フラグのビット論理和を返します。

| 定数 | 値 | UI の動作 |
|---|---|---|
| `ABYSSO_CHANGE_NONE` | 0x0 | 何も行いません |
| `ABYSSO_CHANGE_REDRAW` | 0x1 | 再描画します |
| `ABYSSO_CHANGE_PROPS` | 0x2 | 項目属性と値を取得し直します（コントロールは再生成しません） |
| `ABYSSO_CHANGE_TREE` | 0x4 | ツリーを列挙し直し、パネルを再構築します |

`ABYSSO_CHANGE_PROPS` は、値の変更に連動して他の項目が変化した場合に指定します
（プリセットの適用、`isEnabled` や `isReadOnly` の変化など）。
アドオンからホストへのコールバックを持たない設計のため、連動はこのフラグで通知します。

戻り値は入力値が妥当だったかどうかを表すものではなく、UI が何を更新すべきかを表します。
入力を解釈できなかったことを利用者へ伝える場合は、`Static` 項目に理由を表示し、
`ABYSSO_CHANGE_PROPS` を返してください。

### 4.8 ResetAbyssoAddonValue

```cpp
ABYSSO_ADDON_API int32_t ResetAbyssoAddonValue(const AbyssoGuid* target);
```

任意。`target` が `nullptr` の場合は全項目、それ以外は指定項目を既定値へ戻します。
戻り値は `SetAbyssoAddonValue()` と同じ変更通知フラグです。

リセット後の値は `kAbyssoPropDefaultValue` が返す値と一致しなければなりません。
実装しない場合、ホストは `ABYSSO_CHANGE_NONE` を返したものとして扱います。

## 5. 描画

```cpp
ABYSSO_ADDON_API int32_t RenderAbyssoAddonFrame(const AbyssoRenderRequest* req);
```

必須。1 フレーム分の画像を生成し、`outPixels` へ格納します。

| 戻り値 | 意味 |
|---|---|
| 1 | 描画完了 |
| 0 | 描画中断（`outPixels` の内容は破棄されます） |
| 負数 | エラー |

### 5.1 AbyssoRenderRequest

```cpp
typedef struct
{
    int32_t structSize;

    int32_t width;
    int32_t height;
    int32_t qualityDivisor;
    int32_t flags;
    int32_t reserved0;      // 予約。常に 0

    double  centerX;
    double  centerY;
    double  scale;

    double  timeSeconds;

    int32_t* outPixels;

    const volatile int32_t* cancelFlag;
} AbyssoRenderRequest;
```

構造体は末尾へフィールドが追加される可能性があります。追加されたフィールドを参照する前に、
`ABYSSO_REQUEST_HAS` で `structSize` の範囲に含まれるかを確認してください。

```cpp
if (ABYSSO_REQUEST_HAS(req, timeSeconds))
{
    // req->timeSeconds を参照できます
}
```

範囲外の場合は「ホストがその情報を提供していない」ことを意味します。既定値はアドオン側で決定してください。

### 5.2 座標系

画面座標から複素平面への変換は次の式に従ってください。異なる変換を用いると、
同じ視点でも他のアドオンと表示結果が一致しません。

```cpp
const double aspect = (double)width / (double)height;

const double minX = centerX - scale * aspect;
const double maxX = centerX + scale * aspect;
const double minY = centerY - scale;
const double maxY = centerY + scale;

const double x0 = minX + (maxX - minX) * px / width;
const double y0 = minY + (maxY - minY) * py / height;
```

`scale` は表示領域の高さの半分を表します。全体表示はおよそ
`centerX = -0.5`、`centerY = 0`、`scale = 1.5` です。

### 5.3 出力画素

`outPixels` は `width * height` 個の `int32_t` からなる配列で、ホストが確保します。
配置は行優先（`outPixels[py * width + px]`）です。

画素形式は `0x00RRGGBB` で、上位 8 ビットは未使用です。
`qualityDivisor` の値にかかわらず、常に `width * height` 個すべてを埋めてください。

### 5.4 qualityDivisor

描画品質を落としてもよいことを示すヒントです。たとえば 2 の場合、縦横それぞれ 1/2 の解像度で計算し、
拡大して出力しても構いません。対応は任意であり、常に等倍で描画しても結果は正しく扱われます。
1 未満の値は 1 とみなしてください。

### 5.5 flags

| 定数 | 値 | 意味 |
|---|---|---|
| `ABYSSO_RENDER_LIVE` | 0x1 | 再生中のフレーム。停止中は 0 |

キャッシュ戦略や中断判定の材料として利用できます。再生中でも等倍描画が選択される場合があるため、
`qualityDivisor` だけでは再生中かどうかを判定できません。

### 5.6 timeSeconds

タイムライン上の現在位置を秒単位で表します。ホストは常にこの値を設定しますが、参照するかどうかは任意です。

- 同じ位置に対しては必ず同じ画像を生成してください
- 再生開始時に 0 となり、停止するとその位置で保持されます
- シークによって値が減少する場合があります（単調増加ではありません）
- 新規作成時およびプロジェクト読み込み直後は 0 です
- 連番書き出しでは、システム時刻ではなくフレーム番号から算出されます

### 5.7 描画の中断

中断の通知経路は 2 つあり、ホストは両方を使用します。

| 経路 | 説明 |
|---|---|
| `req->cancelFlag` | 0 以外になった場合、描画を打ち切って構いません。フレーム開始時は必ず 0 です。`nullptr` の場合があるため、参照前に確認してください |
| `CancelAbyssoAddonFrame()` | 別スレッドから呼び出されます。任意実装です |

`cancelFlag` を参照する場合、`CancelAbyssoAddonFrame()` の実装は不要です。
どちらにも対応しない場合、描画は常に最後まで実行されるため、処理の重いアドオンでは再生が滞ります。
中断した場合は 0 を返してください。

中断は `flags` の `ABYSSO_RENDER_LIVE` とは独立です。`live` のときだけ判定するサンプル
（`JuliaAddon`）がありますが、これは判定の回数を抑えるための実装上の選択であり、規約ではありません。
`live` と組み合わせずに常に判定しても構いません。

**0.4.0.0 時点のホストは描画要求を直列に処理するため、中断を発行しません。**
上記 2 つの経路は、将来ホストが中断を発行するようになったときのために用意されています。
実装しなくても現時点で問題は生じませんが、実装しておくと将来のホストでも再生が滞りません。

### 5.8 再現性

描画結果は描画要求の内容のみから決定してください。システム時刻（`clock()`、
`std::chrono::system_clock` など）、乱数、前フレームから引き継いだ状態に依存すると、
再生時と書き出し時で結果が食い違い、プロジェクトを開き直しても同じ画像を再現できません。

アドオンが保持するパラメータ値も描画要求の一部とみなします。描画中に値が変更される可能性があるため、
描画開始時にローカルへ複製してください。

## 6. 情報オーバーレイ

```cpp
ABYSSO_ADDON_API int32_t GetAbyssoAddonInfoItemCount(void);
ABYSSO_ADDON_API int32_t GetAbyssoAddonInfoItem(int32_t index, AbyssoGuid* out);
```

両方そろって任意。詳細情報オーバーレイに表示する項目を列挙します。
描画結果のメタデータや統計情報の表示に使用します。

情報項目はパラメータツリーとは独立した表示専用の一覧で、次の性質を持ちます。

- パラメータツリーには現れません
- 編集対象になりません
- プロジェクトファイルへ保存されません
- 表示内容は `GetAbyssoAddonProp()` の `kAbyssoPropCaption` と `kAbyssoPropCurrentValue` から取得されます

ホストは列挙結果を保持し、表示中は `kAbyssoPropCurrentValue` のみを継続的に取得します。
項目名は列挙時と `SetAbyssoAddonLang()` の呼び出し後にのみ取得し直します。

値は完成したフレームに対応するものだけを公開してください。描画が中断された場合は、前回の値を維持することを推奨します。
値の取得は描画とは異なるタイミングで行われるため、更新はアトミックに行ってください。

`kAbyssoPropCurrentValue` が空文字列の場合、その行は表示されません。
特定のモードでのみ表示したい項目は、この仕様を利用して制御できます。
1 行も公開しない場合、オーバーレイの枠自体が表示されません。

## 7. AbyssoAddonKit.h

`AbyssoAddonKit.h` は、アドオン実装をサポートするヘッダオンリーのライブラリです。利用は任意で、
ABI には影響しません。導入の有無によってアドオンの互換性が変わることはありません。

削減できる定型コードは次のとおりです。

- 項目ツリーの管理
- `GetAbyssoAddonProp` / `SetAbyssoAddonValue` / `ResetAbyssoAddonValue` の実装
- 既定値の管理
- 多言語対応
- 情報オーバーレイの管理
- エクスポート関数の実装

計算処理や着色処理はサポート対象に含まれません。

### 7.1 最小構成

```cpp
#include "AbyssoAddon.h"
#include "AbyssoAddonKit.h"

namespace {

constexpr AbyssoGuid kAddon = ABYSSO_GUID(/* 16 バイト */);
constexpr AbyssoGuid kGroup = ABYSSO_GUID(/* ... */);
constexpr AbyssoGuid kReal  = ABYSSO_GUID(/* ... */);
constexpr AbyssoGuid kInfoC = ABYSSO_GUID(/* ... */);

struct Values { double cr = -0.123; };
Values& V() { static Values v; return v; }

abysso::AddonKit& Kit()
{
    static abysso::AddonKit kit([](abysso::AddonKit& k) {
        auto g = k.Group(kGroup, { "JULIA SET", "ジュリア集合" });
        g.Double(kReal, { "c (real)", "c の実部" }, &V().cr)
         .Range(-2.0, 2.0).Step(0.001).Default(-0.123);
        k.InfoRow(kInfoC, { "c", "c" });
    });
    return kit;
}

} // namespace

ABYSSO_ADDON_ABI_VERSION_IMPL()
ABYSSO_ADDON_INFO_IMPL(kAddon,
                       Kit().Japanese() ? "ジュリア集合" : "Julia set",
                       "1.0.0.0")
ABYSSO_ADDON_EXPORT_ALL(Kit())

ABYSSO_ADDON_API int32_t RenderAbyssoAddonFrame(const AbyssoRenderRequest* req)
{
    abysso::Request r(req);
    if (!r.Valid()) return ABYSSO_ERROR_INVALID_ARG;
    Kit().BeginFrame();
    // 描画処理
    return 1;
}
```

`AddonKit` はコピーもムーブもできないため、関数ローカルの静的変数として保持してください。
名前空間スコープの変数にすると、GUID 定数やバインド先との初期化順に依存します。

より実践的な例はサンプルの `JuliaAddon` を参照してください。

### 7.2 abysso::Text

多言語対応の文字列を表します。文字列は複製されないため、静的文字列リテラルを渡してください。

```cpp
{ "Preset" }                    // 単一言語
{ "Preset", "プリセット" }      // en / ja

abysso::Text::Langs({ { "en", "..." }, { "ja", "..." }, { "fr", "..." } });
```

`"en"` は `"en-US"` に一致します。どの言語にも一致しない場合は、登録された最初の文字列が使用されます。

### 7.3 abysso::Request

`AbyssoRenderRequest` のラッパークラスです。`ABYSSO_REQUEST_HAS` による確認を内部で行うため、
アドオン側で構造体のバージョン差を意識する必要がありません。

| メソッド | 説明 |
|---|---|
| `Valid()` | 描画開始前の妥当性確認（`nullptr`、サイズ、出力バッファ） |
| `Width()` / `Height()` | 描画サイズ |
| `CenterX()` / `CenterY()` / `Scale()` | 視点 |
| `Aspect()` / `MinX()` / `MaxX()` / `MinY()` / `MaxY()` | 5.2 の座標系 |
| `Pixels()` | 出力バッファ |
| `Flags()` / `Live()` | 描画フラグ |
| `QualityDivisor()` | 1 未満を 1 に補正した値 |
| `TimeSeconds(fallback = 0.0)` | タイムライン位置。未提供時は `fallback` |
| `CancelFlag()` | 中断フラグ。未提供時は `nullptr` |
| `Raw()` | 元の構造体 |

### 7.4 abysso::AddonKit

項目ツリーと値の管理を提供します。`AddonKit` 自体はパラメータ値を保持せず、
アドオン側の変数へバインドします。描画処理は従来どおり自身の変数を参照します。

ルート直下の項目は `AddonKit` のメソッドで、入れ子の項目は返却された `Node` の同名メソッドで登録します。

| メソッド | 作成する項目 | バインド先 |
|---|---|---|
| `Group(guid, caption)` | `Group` | なし |
| `Combo(guid, caption, int32_t*)` | `ComboBox` | 選択中のインデックス |
| `.Item(guid, caption)` | `ComboBoxItem` | なし（戻り値は ComboBox 自身） |
| `Check(guid, caption, int32_t*)` | `CheckBox` | 0 / 1 |
| `Int(guid, caption, int32_t*)` | `Int32` | `int32_t` |
| `Double(guid, caption, double*)` | `Double` | `double` |
| `String(guid, caption, std::string*)` | `String` | `std::string` |
| `Static(guid, caption, lambda)` | `Static` | ラムダの戻り値 |
| `Note(guid, text)` | `Description` | なし |

属性の設定メソッドはいずれも `Node` を返すため、チェーン形式で記述できます。

| メソッド | 説明 |
|---|---|
| `Default(value)` | 既定値。`kAbyssoPropDefaultValue` とリセット処理の双方がこの値を使用します |
| `Range(min, max)` | 許容範囲。範囲外の値は設定時に自動補正されます |
| `Step(step)` | スライダーのステップ幅 |
| `MaxLength(chars)` | `String` の最大文字数（バイト数ではありません） |
| `Desc(text)` | 項目の説明 |
| `ReadOnly(bool)` | 読み取り専用にします（値はプロジェクトファイルに保存されません。4.5 を参照） |
| `ReadOnlyIf(predicate)` | 条件により読み取り専用にします。**値を持つ項目では `EnabledIf()` として登録されます**（後述） |
| `EnabledIf(predicate)` | 条件により無効表示にします（値の設定要求は受け付けます） |
| `Redraw(bool)` | 値の変更時に再描画を要求しない項目として扱います |
| `OnChange(lambda)` | 値の更新後に呼び出されます。追加の `ABYSSO_CHANGE_*` を返します |
| `OnReset(lambda)` | 個別項目のリセット後に呼び出されます |

`OnChange` と `OnReset` はバインド先が更新された後に呼び出されるため、引数を取りません。
この中で他の項目を更新し、`ABYSSO_CHANGE_PROPS` を返すことで連動を表現できます。
`abysso::kNoRedraw` を返すと、その呼び出しに限り再描画要求を取り下げます。

`ComboBox` のバインド先は登録順のインデックス（0 始まり）です。GUID との変換は AddonKit が行います。
このインデックスはアドオン内部の表現であり、プロジェクトファイルに保存されるのは選択肢の GUID です（4.5）。

`Static` のラムダは UI スレッドから呼び出されます（3.6）。描画スレッドが更新する値を返す場合は、
`std::atomic` などを介して受け渡してください。

値を持つ項目に `ReadOnlyIf()` を指定した場合、`AddonKit` はそれを `EnabledIf()` として登録します。
読み取り専用の項目はプロジェクトファイルに保存されず、ロック中に保存した値が失われるためです（4.5）。
無効表示であれば同じくコントロールを操作できませんが、値の設定要求は受け付けるため、値は保存され復元されます。

この変換の結果、値を持つ項目の `isReadOnly` は 0 のままで、`isEnabled` が述語の否定になります。
`EnabledIf()` を併用している場合、両方が許可するあいだだけ有効になります。
値を持たない項目（`Group` / `Static` / `Description`）では変換は行われません。

条件によらず常に読み取り専用にする `ReadOnly(true)` は変換の対象外です。
値が利用者の操作で変化しないため、保存されなくても失われるものがありません。

### 7.5 情報オーバーレイ

```cpp
kit.InfoRow(kInfoIter, { "Iterations", "反復回数" });   // 登録順が表示順

// フレームの描画完了後に呼び出す
kit.Publish([iter, manual](abysso::AddonKit::Writer& w) {
    w.Setf(kInfoIter, "%d", iter);
    if (manual) w.Append(kInfoIter, w.Ja() ? " (手動)" : " (manual)");
});

kit.ClearInfo();   // 全行を空にする
```

`Publish()` に渡すラムダは、参照キャプチャではなく値のコピーを渡してください。
言語変更時に同じラムダが再実行されるため、参照キャプチャでは有効期間を過ぎた変数を参照する可能性があります。

`Writer` は `Set()` / `Setf()` / `Append()` と、言語判定の `Ja()` / `Lang()` を提供します。
`kit.ChoiceCaption(combo, index)` を使うと、選択中の `ComboBoxItem` の項目名を取得できます。

### 7.6 中断

```cpp
kit.BeginFrame();                       // フレーム開始時にフラグを初期化

if (kit.Cancelled(r)) return 0;         // ループ内で判定
```

`Cancelled()` は `req->cancelFlag` と `CancelAbyssoAddonFrame()` の両方を統一的に扱います。
`ABYSSO_ADDON_EXPORT_CANCEL` は `CancelAbyssoAddonFrame()` を `kit.RequestCancel()` へ転送します。

### 7.7 マクロ

| マクロ | 生成する関数 |
|---|---|
| `ABYSSO_ADDON_ABI_VERSION_IMPL()` | `GetAbyssoAddonAbiVersion` |
| `ABYSSO_ADDON_INFO_IMPL(guid, name, version)` | `GetAbyssoAddonInfo`（`name` と `version` は式） |
| `ABYSSO_ADDON_EXPORT_PARAMS(kit)` | `GetAbyssoAddonParamChildItemCount` / `GetAbyssoAddonParamChildItem` / `SetAbyssoAddonValue` / `ResetAbyssoAddonValue` |
| `ABYSSO_ADDON_EXPORT_PROP(kit)` | `GetAbyssoAddonProp`（PARAMS と INFO の双方に必要） |
| `ABYSSO_ADDON_EXPORT_INFO(kit)` | `GetAbyssoAddonInfoItemCount` / `GetAbyssoAddonInfoItem` |
| `ABYSSO_ADDON_EXPORT_LANG(kit)` | `SetAbyssoAddonLang` |
| `ABYSSO_ADDON_EXPORT_CANCEL(kit)` | `CancelAbyssoAddonFrame` |
| `ABYSSO_ADDON_EXPORT_ALL(kit)` | 上記のうち PARAMS / PROP / INFO / LANG / CANCEL |

`RenderAbyssoAddonFrame`、`InitializeAbyssoAddon`、`ShutdownAbyssoAddon` は自身で実装します。
マクロが生成する関数はすべて `catch (...)` を含むため、例外が C ABI 境界を越えることはありません。

## 8. 配置と動作確認

1. アドオンを x64 ターゲットでビルドします
2. 次のフォルダを作成します（存在しない場合）

   ```text
   %USERPROFILE%\Documents\Abyssograph\Addons\
   ```

3. 生成した DLL を、依存する DLL があればそれらも含めて上記のフォルダへ配置します
4. インターネットから取得した DLL の場合は、先にブロックを解除します（後述）
5. Abyssograph を起動します

配置先は Microsoft Store 版でも非パッケージ版でも同じです。
このフォルダはアプリケーションでは作成しないため、利用者が作成してください。
`Documents` は既知のフォルダであるため、実際のパスはリダイレクト設定（OneDrive など）に従います。

アドオンの読み込みは起動時に 1 回だけ行われます。
DLL の追加・削除・有効化・無効化・ブロックの解除は、次回起動時に反映されます。

**読み込み順** … アプリケーションに同梱されたアドオンを先に読み込み、
続いて上記フォルダの DLL をファイル名の序数順に読み込みます。
同じ GUID を返すアドオンが 2 つある場合は後から読み込んだ方が破棄されるため、
同梱アドオンが常に優先され、起動直後に選択されているのも必ず同梱のアドオンになります。
アドオンが依存する DLL は、そのアドオン自身のフォルダから解決されます。

**インターネットから取得した DLL は読み込まれません** …
Windows は取得元の情報を代替データストリーム `Zone.Identifier` としてファイルに記録します。
このゾーン識別子が付いた DLL を Abyssograph は読み込みません。
アドオン一覧には「ブロックの解除が必要」として表示され、そこから有効にすることはできません。
エクスプローラーで DLL を右クリックしてプロパティを開き、**［許可する］** にチェックを入れてから、
アプリケーションを起動し直してください。
アプリケーション側でこのゾーン識別子を削除することはありません
（ブロックの解除は、アプリケーションの外で明示的に行う操作です）。

アドオンが 2 つ以上ある場合、パラメータパネルのタイトル行にアドオン選択が表示されます。

読み込み結果は「ヘルプ」→「バージョン情報」→「アドオン」で確認できます。
名前、バージョン、有効状態のほか、読み込みに失敗したアドオンについてはその理由
（ABI バージョン不一致、必須エクスポート関数の不足、GUID の重複、初期化の失敗、ブロック未解除）が
表示されます。有効・無効の切り替えは次回起動時に反映されます。

アドオンがクラッシュまたはハングアップの原因になった場合、次回起動時に自動的に無効化されます。
再び使用する場合は、アドオン一覧から手動で有効化してください。

## 9. よくある問題

| 症状・原因 | 対処 |
|---|---|
| GUID をサンプルから再利用した | 他のアドオンと重複すると読み込みに失敗します。独自の GUID を生成してください |
| 配布後に GUID を変更した | 保存済みプロジェクトとの互換性が失われます。配布後は変更しないでください |
| `isEnabled` を書き込み拒否と解釈した | 無効状態でも値の設定要求は受け付けてください。拒否するとプロジェクトの復元が正しく動作しません |
| 既定値を複数箇所で管理した | `kAbyssoPropDefaultValue` とリセット結果が食い違います。既定値は一箇所で管理してください |
| `MaxLength` をバイト数として扱った | 単位は文字数です。バイト単位で切り詰めると UTF-8 が破損します |
| 描画開始時に値を複製しなかった | 描画中のパラメータ変更により結果が不定になります |
| システム時刻や乱数を参照した | 再描画や書き出しで再現性が失われます（5.8） |
| 例外を DLL 境界の外へ送出した | アプリケーション全体が異常終了します |
| x86 でビルドした | 読み込まれません。x64 でビルドしてください |
| `structSize` を確認しなかった | 将来 SDK が構造体を拡張した際に互換性問題が発生します |
| CRT を動的リンクした | ランタイムが存在しない環境で読み込みに失敗します。`/MT` を推奨します |

## 10. サンプル

| サンプル | 説明 |
|---|---|
| `samples/MinimalAddon` | 必須の 3 関数のみを実装した最小構成。設定項目も情報項目も持ちません |
| `samples/JuliaAddon` | `AbyssoAddonKit.h` を使用した実践例。パラメータ、項目の連動、情報オーバーレイ、中断処理、`qualityDivisor` への対応を含みます |

ビルド方法は [../README.md](../README.md) を参照してください。
