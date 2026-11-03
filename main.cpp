#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <system_error>
#include <algorithm>
#include <cwctype>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

#ifdef _WIN32
// std::wstring を UTF-8 の std::string に変換
std::string to_utf8(std::wstring_view wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return std::string();
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size, nullptr, nullptr);
    return result;
}

void print_line(std::wstring_view wstr) {
    std::cout << to_utf8(wstr) << "\n";
}

void print_err_line(std::wstring_view wstr) {
    std::cerr << to_utf8(wstr) << "\n";
}
#else
void print_line(std::wstring_view wstr) {
    std::wcout << wstr << L"\n";
}
void print_err_line(std::wstring_view wstr) {
    std::wcerr << wstr << L"\n";
}
#endif

// 大文字小文字を区別しない部分一致検索
bool contains_case_insensitive(std::wstring_view source, std::wstring_view query) {
    if (query.empty()) return true;
    auto it = std::search(
        source.begin(), source.end(),
        query.begin(), query.end(),
        [](wchar_t a, wchar_t b) {
            return std::towlower(a) == std::towlower(b);
        }
    );
    return it != source.end();
}

// 大文字小文字を区別する部分一致検索
bool contains_case_sensitive(std::wstring_view source, std::wstring_view query) {
    if (query.empty()) return true;
    return source.find(query) != std::wstring_view::npos;
}

// 使い方・ヘルプの表示
void print_usage(const wchar_t* prog_name) {
    std::wstring exe_name = fs::path(prog_name).filename().wstring();

    print_line(L"使用方法:");
    print_line(L"  " + exe_name + L" <検索文字列> [検索パス] [オプション]\n");
    print_line(L"説明:");
    print_line(L"  指定したパス内（サブディレクトリ含む）から、ファイル名または");
    print_line(L"  フォルダ名に検索文字列を含むものを再帰的に検索し、フルパスを出力します。\n");
    print_line(L"引数:");
    print_line(L"  <検索文字列>           検索する文字（部分一致・必須）");
    print_line(L"  [検索パス]             検索を開始するフォルダ（省略時はカレントディレクトリ）\n");
    print_line(L"オプション:");
    print_line(L"  -s, --case-sensitive   大文字と小文字を区別して検索します（デフォルト: 区別なし）");
    print_line(L"  -h, --help             このヘルプメッセージを表示します\n");
    print_line(L"実行例:");
    print_line(L"  " + exe_name + L" main");
    print_line(L"  " + exe_name + L" .cpp C:\\Users\\User\\source\\repos");
    print_line(L"  " + exe_name + L" 議事録 D:\\Document -s");
}

int wmain(int argc, wchar_t* argv[]) {
#ifdef _WIN32
    // Windows コンソールの入出力を UTF-8 に設定
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::wstring query;
    std::wstring search_root = L".";
    bool case_sensitive = false;

    std::vector<std::wstring> positional_args;

    // コマンドライン引数の解析
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == L"-s" || arg == L"--case-sensitive") {
            case_sensitive = true;
        } else if (!arg.empty() && arg[0] == L'-') {
            print_err_line(L"エラー: 不明なオプションです: " + arg + L"\n");
            print_usage(argv[0]);
            return 1;
        } else {
            positional_args.push_back(arg);
        }
    }

    // 必須引数チェック（第1引数: 検索文字列）
    if (positional_args.empty()) {
        print_err_line(L"エラー: 検索文字列を指定してください。\n");
        print_usage(argv[0]);
        return 1;
    }

    query = positional_args[0];

    // 第2引数: 検索開始パス（指定無しの場合はカレント "."）
    if (positional_args.size() >= 2) {
        search_root = positional_args[1];
    }

    fs::path root_path(search_root);
    std::error_code ec;

    // パスの存在・ディレクトリチェック
    if (!fs::exists(root_path, ec)) {
        print_err_line(L"エラー: 指定されたパスが存在しません: " + search_root);
        return 1;
    }
    if (!fs::is_directory(root_path, ec)) {
        print_err_line(L"エラー: 指定されたパスはディレクトリではありません: " + search_root);
        return 1;
    }

    // 検索一致判定関数の選択
    auto search_func = case_sensitive ? contains_case_sensitive : contains_case_insensitive;

    // ディレクトリ走査のオプション設定（アクセス権限のないフォルダをスキップ）
    fs::directory_options options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator iter(root_path, options, ec);

    if (ec) {
        print_err_line(L"エラー: ディレクトリを開けませんでした: " + search_root);
        return 1;
    }

    fs::recursive_directory_iterator end_iter;

    // 再帰的ディレクトリ探索
    while (iter != end_iter) {
        if (ec) {
            // アクセス権限エラー等の発生時はリセットして次へ進む
            ec.clear();
            iter.increment(ec);
            continue;
        }

        const fs::directory_entry& entry = *iter;
        std::wstring filename = entry.path().filename().wstring();

        // ファイル名またはフォルダ名に検索文字列が含まれているか
        if (search_func(filename, query)) {
            std::error_code abs_ec;
            fs::path abs_path = fs::absolute(entry.path(), abs_ec);
            if (!abs_ec) {
                print_line(abs_path.wstring());
            } else {
                print_line(entry.path().wstring());
            }
        }

        iter.increment(ec);
    }

    return 0;
}
