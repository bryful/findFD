#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cwctype>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// 高速小文字化（ASCIIはビット演算、Unicodeはtowlower）
inline wchar_t fast_towlower(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return c + 32;
    if (c > 127) return std::towlower(c);
    return c;
}

// 部分一致（大文字小文字無視、事前小文字化クエリを使用）
bool contains_case_insensitive_fast(const wchar_t* str, size_t str_len, const std::wstring& lower_query) {
    if (lower_query.empty()) return true;
    size_t q_len = lower_query.size();
    if (str_len < q_len) return false;

    size_t max_i = str_len - q_len;
    for (size_t i = 0; i <= max_i; ++i) {
        bool match = true;
        for (size_t j = 0; j < q_len; ++j) {
            if (fast_towlower(str[i + j]) != lower_query[j]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

// 部分一致（大文字小文字区別あり）
bool contains_case_sensitive_fast(const wchar_t* str, size_t str_len, const std::wstring& query) {
    if (query.empty()) return true;
    size_t q_len = query.size();
    if (str_len < q_len) return false;

    return std::wstring_view(str, str_len).find(query) != std::wstring_view::npos;
}

namespace {
    thread_local std::string t_output_buffer;
}

// 高速・低オーバーヘッドな標準出力バッファリング
class OutputWriter {
public:
    static void write_line(std::wstring_view path) {
        char utf8_scratch[32768];
        int size = WideCharToMultiByte(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), utf8_scratch, sizeof(utf8_scratch) - 2, nullptr, nullptr);
        if (size <= 0) return;

        utf8_scratch[size] = '\r';
        utf8_scratch[size + 1] = '\n';

        t_output_buffer.append(utf8_scratch, size + 2);

        // バッファが 64KB 以上溜まったらフラッシュ
        if (t_output_buffer.size() >= 65536) {
            flush_thread();
        }
    }

    static void flush_thread() {
        if (!t_output_buffer.empty()) {
            flush_buffer(t_output_buffer);
        }
    }

private:
    static void flush_buffer(std::string& buf) {
        static std::mutex s_output_mutex;
        static HANDLE s_hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

        std::lock_guard<std::mutex> lock(s_output_mutex);
        DWORD written = 0;
        WriteFile(s_hStdout, buf.data(), static_cast<DWORD>(buf.size()), &written, nullptr);
        buf.clear();
    }
};

void print_line(std::wstring_view wstr) {
    char utf8[4096];
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), utf8, sizeof(utf8) - 2, nullptr, nullptr);
    if (size > 0) {
        utf8[size] = '\r';
        utf8[size + 1] = '\n';
        DWORD written = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), utf8, static_cast<DWORD>(size + 2), &written, nullptr);
    }
}

void print_err_line(std::wstring_view wstr) {
    char utf8[4096];
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), utf8, sizeof(utf8) - 2, nullptr, nullptr);
    if (size > 0) {
        utf8[size] = '\r';
        utf8[size + 1] = '\n';
        DWORD written = 0;
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), utf8, static_cast<DWORD>(size + 2), &written, nullptr);
    }
}

// 検索対象の種類
enum class TargetType {
    All,        // ファイルおよびフォルダの両方
    FilesOnly,  // ファイルのみ
    DirsOnly    // ディレクトリ（フォルダ）のみ
};

// 使い方・ヘルプの表示
void print_usage(const wchar_t* prog_name) {
    std::wstring exe_name = prog_name;
    size_t last_slash = exe_name.find_last_of(L"\\/");
    if (last_slash != std::wstring::npos) {
        exe_name = exe_name.substr(last_slash + 1);
    }

    print_line(L"使用方法:");
    print_line(L"  " + exe_name + L" <検索文字列...> [検索パス] [オプション]\n");
    print_line(L"説明:");
    print_line(L"  指定したパス内（サブディレクトリ含む）から、ファイル名または");
    print_line(L"  フォルダ名に検索文字列を含むものをマルチスレッドで超高速に再帰検索し、");
    print_line(L"  フルパスを出力します。検索文字列は複数指定可能です（デフォルト: OR検索）。\n");
    print_line(L"引数:");
    print_line(L"  <検索文字列...>        検索する文字（複数指定可・部分一致・必須）");
    print_line(L"  [検索パス]             検索を開始するフォルダ（省略時はカレントディレクトリ）");
    print_line(L"                         ※最後の引数が既存フォルダの場合は自動的に検索パスと判定します\n");
    print_line(L"オプション:");
    print_line(L"  -a, --and              指定したすべての文字列を含むものを検索（デフォルト: いずれかを含むOR検索）");
    print_line(L"  -p, --path <パス>      検索開始フォルダを明示的に指定");
    print_line(L"  -f, --file             ファイルのみを検索対象にします");
    print_line(L"  -d, --dir, --directory ディレクトリ（フォルダ）のみを検索対象にします");
    print_line(L"  -s, --case-sensitive   大文字と小文字を区別して検索します（デフォルト: 区別なし）");
    print_line(L"  -t <スレッド数>        並列スレッド数を指定（デフォルト: CPUの論理コア数）");
    print_line(L"  -h, --help             このヘルプメッセージを表示します\n");
    print_line(L"実行例:");
    print_line(L"  " + exe_name + L" main");
    print_line(L"  " + exe_name + L" .cpp .h");
    print_line(L"  " + exe_name + L" .cpp .h C:\\Users\\User\\source\\repos -f");
    print_line(L"  " + exe_name + L" Debug Release D:\\Source -d");
    print_line(L"  " + exe_name + L" 2026 report D:\\Document -f --and");
}

// マルチスレッド検索マネージャ
class ParallelSearcher {
public:
    ParallelSearcher(std::vector<std::wstring> queries, bool case_sensitive, unsigned int num_threads, TargetType target_type, bool match_all)
        : m_queries(std::move(queries)), m_case_sensitive(case_sensitive), m_num_threads(num_threads), m_target_type(target_type), m_match_all(match_all)
    {
        // 検索クエリを事前に小文字化しておくことで走査中の towlower 呼び出しを最小化
        m_lower_queries.reserve(m_queries.size());
        for (const auto& q : m_queries) {
            std::wstring lq = q;
            for (auto& c : lq) {
                c = fast_towlower(c);
            }
            m_lower_queries.push_back(std::move(lq));
        }
    }

    void run(const std::wstring& root_path) {
        m_work_queue.push_back(root_path);

        std::vector<std::thread> workers;
        workers.reserve(m_num_threads);

        for (unsigned int i = 0; i < m_num_threads; ++i) {
            workers.emplace_back(&ParallelSearcher::worker_loop, this);
        }

        for (auto& t : workers) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

private:
    void worker_loop() {
        std::vector<std::wstring> local_subdirs;
        local_subdirs.reserve(128);

        while (true) {
            std::wstring current_dir;
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_cv.wait(lock, [this] {
                    return !m_work_queue.empty() || m_stop || m_active_tasks == 0;
                });

                if (m_work_queue.empty()) {
                    m_stop = true;
                    m_cv.notify_all();
                    break;
                }

                current_dir = std::move(m_work_queue.back());
                m_work_queue.pop_back();
                ++m_active_tasks;
            }

            local_subdirs.clear();

            // current_dir\ * のワイルドカードパターンを構築
            std::wstring search_pattern = current_dir;
            if (search_pattern.empty() || search_pattern.back() != L'\\') {
                search_pattern += L'\\';
            }
            std::wstring base_path = search_pattern;
            search_pattern += L'*';

            WIN32_FIND_DATAW find_data;
            // 8.3形式の短縮ファイル名スキップ (FindExInfoBasic) と
            // ラージバッファフェッチ (FIND_FIRST_EX_LARGE_FETCH) でOSカーネル呼び出しを最小化
            HANDLE hFind = FindFirstFileExW(
                search_pattern.c_str(),
                FindExInfoBasic,
                &find_data,
                FindExSearchNameMatch,
                nullptr,
                FIND_FIRST_EX_LARGE_FETCH
            );

            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    const wchar_t* name = find_data.cFileName;

                    // "." および ".." は除外
                    if (name[0] == L'.' && (name[1] == L'\0' || (name[1] == L'.' && name[2] == L'\0'))) {
                        continue;
                    }

                    size_t name_len = wcslen(name);
                    bool is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    bool is_reparse = (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

                    // 検索対象種別（ファイルのみ / ディレクトリのみ）の判定
                    bool is_target = true;
                    if (m_target_type == TargetType::FilesOnly && is_dir) {
                        is_target = false;
                    } else if (m_target_type == TargetType::DirsOnly && !is_dir) {
                        is_target = false;
                    }

                    // ファイル名 / フォルダ名の一致判定
                    if (is_target) {
                        bool matched = false;
                        if (m_match_all) {
                            matched = true;
                            for (size_t i = 0; i < m_queries.size(); ++i) {
                                bool ok = m_case_sensitive
                                    ? contains_case_sensitive_fast(name, name_len, m_queries[i])
                                    : contains_case_insensitive_fast(name, name_len, m_lower_queries[i]);
                                if (!ok) {
                                    matched = false;
                                    break;
                                }
                            }
                        } else {
                            matched = false;
                            for (size_t i = 0; i < m_queries.size(); ++i) {
                                bool ok = m_case_sensitive
                                    ? contains_case_sensitive_fast(name, name_len, m_queries[i])
                                    : contains_case_insensitive_fast(name, name_len, m_lower_queries[i]);
                                if (ok) {
                                    matched = true;
                                    break;
                                }
                            }
                        }

                        if (matched) {
                            OutputWriter::write_line(base_path + name);
                        }
                    }

                    // サブディレクトリの収集（ジャンクション/シンボリックリンクは無限ループ防止のためスキップ）
                    if (is_dir && !is_reparse) {
                        local_subdirs.push_back(base_path + name);
                    }

                } while (FindNextFileW(hFind, &find_data));

                FindClose(hFind);
            }

            // 見つかったサブディレクトリを共有キューに一括追加
            {
                std::lock_guard<std::mutex> lock(m_queue_mutex);
                if (!local_subdirs.empty()) {
                    m_work_queue.insert(m_work_queue.end(),
                        std::make_move_iterator(local_subdirs.begin()),
                        std::make_move_iterator(local_subdirs.end()));
                }
                --m_active_tasks;
                if (m_work_queue.empty() && m_active_tasks == 0) {
                    m_stop = true;
                }
            }
            m_cv.notify_all();
        }

        // スレッド終了時に残存バッファをフラッシュ
        OutputWriter::flush_thread();
    }

    std::vector<std::wstring> m_queries;
    std::vector<std::wstring> m_lower_queries;
    bool m_case_sensitive;
    unsigned int m_num_threads;
    TargetType m_target_type;
    bool m_match_all;

    std::vector<std::wstring> m_work_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_cv;
    int m_active_tasks = 0;
    bool m_stop = false;
};

int wmain(int argc, wchar_t* argv[]) {
    // コンソールの入出力を UTF-8 に設定
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::vector<std::wstring> queries;
    std::wstring search_root = L".";
    std::wstring explicit_path;
    bool has_explicit_path = false;
    bool case_sensitive = false;
    bool match_all = false;
    TargetType target_type = TargetType::All;
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::vector<std::wstring> positional_args;

    // コマンドライン引数の解析
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == L"-f" || arg == L"--file") {
            if (target_type == TargetType::DirsOnly) {
                print_err_line(L"エラー: -f (ファイルのみ) と -d (ディレクトリのみ) は同時に指定できません。\n");
                print_usage(argv[0]);
                return 1;
            }
            target_type = TargetType::FilesOnly;
        } else if (arg == L"-d" || arg == L"--dir" || arg == L"--directory") {
            if (target_type == TargetType::FilesOnly) {
                print_err_line(L"エラー: -f (ファイルのみ) と -d (ディレクトリのみ) は同時に指定できません。\n");
                print_usage(argv[0]);
                return 1;
            }
            target_type = TargetType::DirsOnly;
        } else if (arg == L"-s" || arg == L"--case-sensitive") {
            case_sensitive = true;
        } else if (arg == L"-a" || arg == L"--and") {
            match_all = true;
        } else if ((arg == L"-p" || arg == L"--path") && i + 1 < argc) {
            explicit_path = argv[++i];
            has_explicit_path = true;
        } else if (arg == L"-t" && i + 1 < argc) {
            num_threads = static_cast<unsigned int>(_wtoi(argv[++i]));
            if (num_threads == 0) num_threads = 1;
        } else if (!arg.empty() && arg[0] == L'-') {
            print_err_line(L"エラー: 不明なオプションです: " + arg + L"\n");
            print_usage(argv[0]);
            return 1;
        } else {
            positional_args.push_back(arg);
        }
    }

    // A案（スマート位置引数判定）:
    if (has_explicit_path) {
        // -p で明示指定されている場合は位置引数がすべて検索文字列
        queries = positional_args;
        search_root = explicit_path;
    } else {
        if (positional_args.size() == 1) {
            // 1つの場合は検索文字列、パスはカレント "."
            queries.push_back(positional_args[0]);
            search_root = L".";
        } else if (positional_args.size() >= 2) {
            // 2つ以上ある場合、最後の引数が実在するフォルダかどうかを判定
            const std::wstring& last_arg = positional_args.back();
            DWORD attr = GetFileAttributesW(last_arg.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                // 最後の引数が既存フォルダならそれを検索パスとする
                search_root = last_arg;
                queries.assign(positional_args.begin(), positional_args.end() - 1);
            } else {
                // フォルダでなければすべて検索文字列とし、カレントディレクトリを検索
                queries = positional_args;
                search_root = L".";
            }
        }
    }

    // 必須引数チェック（検索文字列）
    if (queries.empty()) {
        print_err_line(L"エラー: 検索文字列を指定してください。\n");
        print_usage(argv[0]);
        return 1;
    }

    // パスをフルパスに事前解決（走査中の absolute() 呼び出しを不要にして超高速化）
    wchar_t full_root_buf[32768];
    DWORD full_len = GetFullPathNameW(search_root.c_str(), _countof(full_root_buf), full_root_buf, nullptr);
    if (full_len == 0) {
        print_err_line(L"エラー: パスの解決に失敗しました: " + search_root);
        return 1;
    }

    // パスの存在とディレクトリ判定
    DWORD attr = GetFileAttributesW(full_root_buf);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        print_err_line(L"エラー: 指定されたパスが存在しません: " + search_root);
        return 1;
    }
    if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        print_err_line(L"エラー: 指定されたパスはディレクトリではありません: " + search_root);
        return 1;
    }

    std::wstring root_path(full_root_buf, full_len);

    // マルチスレッド並列検索の実行
    ParallelSearcher searcher(std::move(queries), case_sensitive, num_threads, target_type, match_all);
    searcher.run(root_path);

    return 0;
}
