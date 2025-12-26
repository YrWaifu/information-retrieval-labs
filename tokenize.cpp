#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

static inline char to_lower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
    return c;
}

static inline bool is_alnum_ascii(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

std::vector<std::string> tokenize_stream(std::istream& in, std::size_t min_len = 2, bool keep_hyphen = true) {
    std::vector<std::string> tokens;
    std::string cur;
    cur.reserve(64);

    char c;
    char prev = 0;

    auto flush = [&]() {
        if (cur.size() >= min_len) tokens.push_back(cur);
        cur.clear();
    };

    while (in.get(c)) {
        if (is_alnum_ascii(c)) {
            cur.push_back(to_lower_ascii(c));
        } else if (keep_hyphen && c == '-') {
            if (!cur.empty()) {
                char next = static_cast<char>(in.peek());
                if (is_alnum_ascii(next)) {
                    cur.push_back('-');
                } else {
                    flush();
                }
            } else {
            }
        } else {
            if (!cur.empty()) flush();
        }
        prev = c;
    }
    if (!cur.empty()) flush();
    return tokens;
}

static void print_usage() {
    std::cerr
        << "Usage:\n"
        << "  tokenize --file <path> [--min-len N] [--no-hyphen]\n"
        << "  tokenize --dir  <corpus_dir> --out <out_dir> [--min-len N] [--no-hyphen]\n"
        << "\n"
        << "Modes:\n"
        << "  --file: tokenize one file, print tokens to stdout (one per line)\n"
        << "  --dir : tokenize all .txt files in a directory, write token lists to out_dir/<same_name>.tok\n";
}

int main(int argc, char** argv) {
    std::string file_path;
    std::string dir_path;
    std::string out_dir;
    std::size_t min_len = 2;
    bool keep_hyphen = true;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--file" && i + 1 < argc) file_path = argv[++i];
        else if (a == "--dir" && i + 1 < argc) dir_path = argv[++i];
        else if (a == "--out" && i + 1 < argc) out_dir = argv[++i];
        else if (a == "--min-len" && i + 1 < argc) min_len = static_cast<std::size_t>(std::stoul(argv[++i]));
        else if (a == "--no-hyphen") keep_hyphen = false;
        else if (a == "--help" || a == "-h") { print_usage(); return 0; }
        else {
            std::cerr << "Unknown arg: " << a << "\n";
            print_usage();
            return 2;
        }
    }

    if (!file_path.empty() && !dir_path.empty()) {
        std::cerr << "Choose one mode: --file OR --dir\n";
        return 2;
    }

    // Mode 1: one file -> stdout
    if (!file_path.empty()) {
        std::ifstream in(file_path, std::ios::binary);
        if (!in) {
            std::cerr << "Cannot open file: " << file_path << "\n";
            return 1;
        }
        auto tokens = tokenize_stream(in, min_len, keep_hyphen);
        for (const auto& t : tokens) std::cout << t << "\n";
        return 0;
    }

    // Mode 2: directory -> out_dir
    if (!dir_path.empty()) {
        if (out_dir.empty()) {
            std::cerr << "--out is required for --dir mode\n";
            return 2;
        }
        fs::create_directories(out_dir);

        std::size_t files = 0;
        std::uint64_t total_tokens = 0;

        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (!entry.is_regular_file()) continue;
            const auto p = entry.path();
            if (p.extension() != ".txt") continue;

            std::ifstream in(p, std::ios::binary);
            if (!in) {
                std::cerr << "Skip (cannot open): " << p.string() << "\n";
                continue;
            }

            auto tokens = tokenize_stream(in, min_len, keep_hyphen);

            fs::path out_path = fs::path(out_dir) / (p.stem().string() + ".tok");
            std::ofstream out(out_path, std::ios::binary);
            if (!out) {
                std::cerr << "Skip (cannot write): " << out_path.string() << "\n";
                continue;
            }
            for (const auto& t : tokens) out << t << "\n";

            files++;
            total_tokens += static_cast<std::uint64_t>(tokens.size());
            if (files % 200 == 0) {
                std::cerr << "Tokenized files: " << files << ", total tokens: " << total_tokens << "\n";
            }
        }

        std::cerr << "Done. Files: " << files << ", total tokens: " << total_tokens << "\n";
        return 0;
    }

    print_usage();
    return 2;
}
