#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

struct Pair {
    std::string term;
    uint32_t doc;
};

static uint32_t parse_doc_id_from_filename(const fs::path& p) {
    // stems/000123.stm -> 123
    std::string stem = p.stem().string();
    size_t i = 0;
    while (i < stem.size() && stem[i] == '0') i++;
    if (i == stem.size()) return 0;
    return (uint32_t)std::stoul(stem.substr(i));
}

static void ensure_dir(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
}

static bool read_line(std::ifstream& in, std::string& s) {
    return static_cast<bool>(std::getline(in, s));
}

int main(int argc, char** argv) {
    std::string stems_dir;
    std::string out_dir = "index";

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--stems" && i + 1 < argc) stems_dir = argv[++i];
        else if (a == "--out" && i + 1 < argc) out_dir = argv[++i];
        else if (a == "--help" || a == "-h") {
            std::cerr << "Usage: build_index --stems <stems_dir> --out <index_dir>\n";
            return 0;
        } else {
            std::cerr << "Unknown arg: " << a << "\n";
            return 2;
        }
    }

    if (stems_dir.empty()) {
        std::cerr << "--stems is required\n";
        return 2;
    }

    ensure_dir(out_dir);

    std::vector<Pair> pairs;
    pairs.reserve(2'000'000);

    size_t files = 0;
    uint32_t maxDoc = 0;

    for (const auto& entry : fs::directory_iterator(stems_dir)) {
        if (!entry.is_regular_file()) continue;
        const auto p = entry.path();
        if (p.extension() != ".stm") continue;

        uint32_t docId = parse_doc_id_from_filename(p);
        if (docId > maxDoc) maxDoc = docId;

        std::ifstream in(p, std::ios::binary);
        if (!in) continue;

        std::vector<std::string> terms;
        terms.reserve(2048);

        std::string w;
        while (read_line(in, w)) {
            if (!w.empty()) terms.push_back(w);
        }
        if (terms.empty()) continue;

        // уникализация термов в документе
        std::sort(terms.begin(), terms.end());
        terms.erase(std::unique(terms.begin(), terms.end()), terms.end());

        for (const auto& t : terms) {
            pairs.push_back({t, docId});
        }

        files++;
        if (files % 500 == 0) {
            std::cerr << "Processed docs: " << files << ", pairs: " << pairs.size() << "\n";
        }
    }

    if (pairs.empty()) {
        std::cerr << "No pairs collected. Check stems directory.\n";
        return 1;
    }

    std::sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) {
        if (a.term != b.term) return a.term < b.term;
        return a.doc < b.doc;
    });

    fs::path postings_path = fs::path(out_dir) / "postings.bin";
    fs::path dict_path = fs::path(out_dir) / "dict.tsv";
    fs::path maxdoc_path = fs::path(out_dir) / "maxdoc.txt";

    std::ofstream postings(postings_path, std::ios::binary);
    std::ofstream dict(dict_path, std::ios::binary);
    std::ofstream maxdoc(maxdoc_path, std::ios::binary);

    if (!postings || !dict || !maxdoc) {
        std::cerr << "Cannot open output files in: " << out_dir << "\n";
        return 1;
    }

    // сохраняем maxDoc для корректного NOT
    maxdoc << maxDoc << "\n";

    dict << "term\tdf\toffset\tlen\n";

    uint64_t offset = 0;
    size_t i = 0;

    while (i < pairs.size()) {
        size_t j = i;
        const std::string& term = pairs[i].term;

        std::vector<uint32_t> docs;
        docs.reserve(64);

        uint32_t last = 0;
        bool have_last = false;

        while (j < pairs.size() && pairs[j].term == term) {
            uint32_t d = pairs[j].doc;
            if (!have_last || d != last) {
                docs.push_back(d);
                last = d;
                have_last = true;
            }
            j++;
        }

        uint64_t len_bytes = (uint64_t)docs.size() * sizeof(uint32_t);
        postings.write(reinterpret_cast<const char*>(docs.data()), (std::streamsize)len_bytes);

        dict << term << "\t" << docs.size() << "\t" << offset << "\t" << len_bytes << "\n";

        offset += len_bytes;
        i = j;
    }

    std::cerr << "Index built.\n";
    std::cerr << "Docs processed: " << files << "\n";
    std::cerr << "maxDoc: " << maxDoc << "\n";
    std::cerr << "Output: " << out_dir << "/dict.tsv, postings.bin, maxdoc.txt\n";
    return 0;
}
