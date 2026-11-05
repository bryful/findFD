# findFD - ファイル・フォルダ超高速再帰検索コンソールアプリ

マルチスレッド並列探索と Windows 高速API を活用した、超高速・軽量なコマンドライン検索ツールです。  
指定したパス（省略時はカレントディレクトリ）内のサブディレクトリを含めて再帰的に走査し、ファイル名またはフォルダ名に検索文字列を含む対象の**フルパス**を一覧出力します。

---

## 特長

1. **超高速マルチスレッド走査**: CPUの全コア（論理スレッド）を使った並列ディレクトリ探索で、大規模ドライブや数万〜数十万ファイルも瞬時に検索
2. **Win32 高速探索API**: `FindFirstFileExW`（8.3短縮名スキップ・ラージフェッチバッファ）採用によりファイルシステム呼び出しを極小化
3. **日本語・Unicode完全対応**: 日本語のファイル名・パスやコンソール表示で文字化けしません
4. **安全なエラーハンドリング**: 管理者権限が必要なシステムフォルダ等のアクセス拒否（Access Denied）に遭遇してもスキップして走査を継続
5. **直感的な検索**: デフォルトで大文字・小文字を区別しない（Case-insensitive）検索。オプションで区別する検索も可能

---

## 使い方

```bash
findFD.exe <検索文字列...> [検索パス] [オプション]
```

### 引数
- `<検索文字列...>` （必須）: 検索する文字（**スペース区切りで複数指定可能**）
- `[検索パス]` （任意）: 検索を開始するディレクトリ（省略時はカレントディレクトリ）  
  ※最後の引数が実在するフォルダの場合、自動的に検索パスとして判定します（スマート判定）。

### オプション
- `-a`, `--and`: **AND検索**（指定したすべての文字列を含むものを検索。デフォルトは**OR検索**）
- `-p`, `--path <パス>`: 検索開始ディレクトリを明示的に指定
- `-f`, `--file`: **ファイルのみ**を検索対象にします
- `-d`, `--dir`, `--directory`: **ディレクトリ（フォルダ）のみ**を検索対象にします
- `-s`, `--case-sensitive`: 大文字と小文字を厳密に区別して検索します（指定しない場合は区別なし）
- `-t <スレッド数>`: 並列スレッド数を指定（デフォルト: CPUの論理コア数）
- `-h`, `--help`: ヘルプを表示します

---

## 実行例

### 1. 複数の文字列を検索（OR検索: .cpp または .h）
```bash
findFD.exe .cpp .h
```

### 2. 複数の文字列をパス指定して検索
```bash
findFD.exe .cpp .h C:\Users\User\source\repos -f
```

### 3. AND検索（すべての文字列を含む）
```bash
findFD.exe 2026 report D:\Documents --and
```

### 4. 明示的にパスを指定 (`-p`)
```bash
findFD.exe Debug Release -p D:\Source -d
```

### 5. ファイルのみを検索 (`-f`)
```bash
findFD.exe .cpp C:\Users\User\source\repos -f
```

### 6. ディレクトリ（フォルダ）のみを検索 (`-d`)
```bash
findFD.exe test C:\Users\User\source\repos -d
```

### 7. 日本語文字の検索
```bash
findFD.exe 議事録 仕様書 "C:\Users\User\OneDrive - Work"
```

### 8. 大文字・小文字を区別する検索 (`-s`)
```bash
findFD.exe README . -s
```

---

## ビルド方法

### 方法1: 付属のバッチファイルでビルド（推奨）
Visual Studio がインストールされている環境で、コマンドプロンプトまたは PowerShell から以下を実行します。

```cmd
build.bat
```
自動的に Visual Studio の C++ コンパイラ環境を検出し、最適化オプション付きで `findFD.exe` を生成します。

### 方法2: CMake を使用したビルド
```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 方法3: Visual Studio IDE から開いてビルド
- **`findFD.slnx`** または **`findFD.sln`** をダブルクリックして開きます。
- 構成（`Debug` または `Release` / `x64`）を選び、**Ctrl + Shift + B** でビルドできます。
- 出力先: `bin\Release\findFD.exe` または `bin\Debug\findFD.exe`

