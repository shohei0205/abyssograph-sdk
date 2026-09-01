# Abyssograph Add-on SDK

[Abyssograph](https://www.microsoft.com/store/apps/9NF7CR4V2SJP) is an application for rendering and exploring images generated through numerical computation in real time.

With the Abyssograph Add-on SDK, you can implement custom rendering logic as a native DLL and extend Abyssograph with your own algorithms. An add-on receives data from the host application, performs **calculation, normalization, and colorization**, and returns a single rendered frame. The generated image can then be previewed instantly within the application.

- Add custom fractals, numerical fields, and rendering algorithms
- Implement high-performance native code in C++
- Distribute add-ons as standard Windows x64 DLLs

| | |
|---|---|
| ABI Version | **2** |
| Supported Application | Abyssograph 0.4.0.0 or later |
| Add-on Format | Windows x64 native DLL |
| Requirements | Visual Studio 2022 or later (Desktop Development with C++) and CMake 3.21 or later |
| License | MIT ([LICENSE](LICENSE)) |

## Directory Layout

```text
AbyssoSdk/
├── README.md                        This file
├── LICENSE                          MIT License
├── include/
│   ├── AbyssoAddon.h                Required header
│   └── AbyssoAddonKit.h             Optional helper classes and macros
├── docs/
│   ├── api-reference.ja.md          API Reference (Japanese)
│   └── api-reference.en.md          API Reference (English)
└── samples/
    ├── CMakeLists.txt               Sample build configuration
    ├── build.cmd                    Convenience wrapper for CMake
    ├── MinimalAddon/                Minimal add-on example
    └── JuliaAddon/                  Julia set example
```

## Documentation

- [docs/api-reference.en.md](docs/api-reference.en.md) - API Reference (English)
- [docs/api-reference.ja.md](docs/api-reference.ja.md) - API Reference (Japanese)

## Building the Samples

Using the provided build script:

```bat
samples\build.cmd Release
```

After a successful build, `MinimalAddon.dll` and `JuliaAddon.dll` will be generated in `samples\bin\`.

Available build configurations are `Debug` and `Release`. If omitted, `Release` is used by default.

You may also invoke CMake directly:

```bat
cmake -S samples -B samples/build -A x64
cmake --build samples/build --config Release
```

> **Note**
>
> Abyssograph is a 64-bit application. Add-ons must be built for **x64**. 32-bit (x86) DLLs cannot be loaded.

The `build.cmd` script prefers the CMake version bundled with Visual Studio. This helps avoid issues caused by older CMake installations on the system `PATH` that may not recognize newer Visual Studio generators.

## Installing an Add-on

Place the DLL you built, including any DLLs it depends on, in the following folder:

```text
%USERPROFILE%\Documents\Abyssograph\Addons
```

The location is the same for both the Microsoft Store build and the non-packaged build.

This folder is not created automatically by the application. If it does not exist, create it manually.

Add-ons are detected and loaded only when the application starts. Restart Abyssograph after adding, updating, or removing a DLL.

### DLLs Downloaded from the Internet

A DLL downloaded from a website may be blocked by Windows.

In that case, open the properties of the DLL, select **Unblock**, and then start Abyssograph.

A blocked DLL is not loaded. It appears under **Help → About → Add-ons** as *Unblock required*.

### Related Information

For the load order, how to read the add-on list, and detailed diagnostics when loading fails, see section 8 of [docs/api-reference.en.md](docs/api-reference.en.md).

## Contributing

This repository is a copy exported from the Abyssograph source tree. Pull requests are not accepted here, because the next export would overwrite them. Please open an issue instead.

## License

This SDK is distributed under the MIT License. See [LICENSE](LICENSE) for details.

---

# Abyssograph アドオン SDK

[Abyssograph](https://www.microsoft.com/store/apps/9NF7CR4V2SJP) は、数値計算によって生成される画像をリアルタイムに描画・探索するためのアプリケーションです。

Abyssograph アドオン SDK を使用すると、独自のレンダリングロジックをネイティブ DLL として実装し、Abyssograph に追加できます。アドオンはホストから渡される情報をもとに **計算・正規化・着色** を行い、1フレーム分のイメージを生成して返します。生成結果はアプリ上で即座にプレビューできます。

- 独自のフラクタルや数値場の描画アルゴリズムを追加可能
- 高速なネイティブコード（C++）で実装可能
- Windows x64 DLL として配布可能

| | |
|---|---|
| ABI バージョン | **2** |
| 対応アプリ | Abyssograph 0.4.0.0 以降 |
| アドオン形式 | Windows x64 ネイティブ DLL |
| 開発環境 | Visual Studio 2022 以降（C++ デスクトップ開発） + CMake 3.21 以降 |
| ライセンス | MIT（[LICENSE](LICENSE)） |

## ディレクトリ構成

```text
AbyssoSdk/
├── README.md                        このファイル
├── LICENSE                          MIT ライセンス
├── include/
│   ├── AbyssoAddon.h                必須ヘッダ
│   └── AbyssoAddonKit.h             任意。実装サポート用のクラスとマクロを提供
├── docs/
│   ├── api-reference.ja.md          API リファレンス（日本語）
│   └── api-reference.en.md          API Reference (English)
└── samples/
    ├── CMakeLists.txt               サンプルのビルド設定
    ├── build.cmd                    CMake 呼び出し用ラッパー
    ├── MinimalAddon/                最小構成のサンプル
    └── JuliaAddon/                  ジュリア集合のサンプル
```

## ドキュメント

- [docs/api-reference.ja.md](docs/api-reference.ja.md) - API リファレンス（日本語）
- [docs/api-reference.en.md](docs/api-reference.en.md) - API Reference (English)

## サンプルのビルド

付属のビルドスクリプトを使用する場合:

```bat
samples\build.cmd Release
```

ビルドに成功すると、`samples\bin\` に `MinimalAddon.dll` と `JuliaAddon.dll` が出力されます。

利用できる構成は `Debug` と `Release` で、省略時は `Release` が使用されます。

CMake を直接実行してビルドすることもできます。

```bat
cmake -S samples -B samples/build -A x64
cmake --build samples/build --config Release
```

> **注意**
>
> Abyssograph は x64 アプリケーションです。アドオンも必ず x64 でビルドしてください。x86 版 DLL は読み込まれません。

`build.cmd` は Visual Studio に同梱される CMake を優先して使用します。PATH 上の CMake が古い場合、インストール済みの Visual Studio 用ジェネレーターを認識できないことがあるためです。

## アドオンの配置

生成した DLL を、依存する DLL がある場合はそれらも含めて次のフォルダへ配置してください。

```text
%USERPROFILE%\Documents\Abyssograph\Addons
```

配置先は Microsoft Store 版、非パッケージ版のいずれも共通です。

このフォルダはアプリケーションによって自動作成されません。存在しない場合は、利用者が手動で作成してください。

アドオンの検出と読み込みはアプリケーション起動時にのみ行われます。DLL の追加、更新、削除を行った場合は、Abyssograph を再起動してください。

### インターネットから取得した DLL について

Web サイトなどからダウンロードした DLL は、Windows によってブロックされることがあります。

その場合は DLL のプロパティを開き、**［許可する］** にチェックを入れてから Abyssograph を起動してください。

ブロックされた DLL は読み込まれず、**［ヘルプ］→［バージョン情報］→［アドオン］** に「ブロックの解除が必要」として表示されます。

### 関連情報

アドオンの読み込み順序、アドオン一覧の見方、および読み込みに失敗した場合の詳細な診断については、[docs/api-reference.ja.md](docs/api-reference.ja.md) の第 8 章を参照してください。

## 修正の提案について

このリポジトリは Abyssograph 本体から書き出した写しです。プルリクエストはマージしても次回の書き出しで失われるため、受け付けていません。修正の提案や不具合の報告は Issue へお寄せください。

## ライセンス

この SDK は MIT License のもとで提供されています。詳細は [LICENSE](LICENSE) を参照してください。