#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

struct DictEntry {
    uint32_t df = 0;
    uint64_t offset = 0;
    uint64_t len = 0;
};

static std::string to_upper_ascii(std::string s) {
    for (char& c : s) if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    return s;
}

static std::string to_lower_ascii(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

static std::vector<std::string> tokenize_query(const std::string& q) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : q) {
        if (std::isspace((unsigned char)c)) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else if (c == '(' || c == ')') {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            out.push_back(std::string(1, c));
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// операции над отсортированными списками docID
static std::vector<uint32_t> op_and(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> r;
    r.reserve(std::min(a.size(), b.size()));
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) { r.push_back(a[i]); i++; j++; }
        else if (a[i] < b[j]) i++;
        else j++;
    }
    return r;
}

static std::vector<uint32_t> op_or(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> r;
    r.reserve(a.size() + b.size());
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) { r.push_back(a[i]); i++; j++; }
        else if (a[i] < b[j]) { r.push_back(a[i]); i++; }
        else { r.push_back(b[j]); j++; }
    }
    while (i < a.size()) r.push_back(a[i++]);
    while (j < b.size()) r.push_back(b[j++]);
    r.erase(std::unique(r.begin(), r.end()), r.end());
    return r;
}

static std::vector<uint32_t> op_not(const std::vector<uint32_t>& a, const std::vector<uint32_t>& universe) {
    std::vector<uint32_t> r;
    r.reserve(universe.size());
    size_t i = 0, j = 0;
    while (i < universe.size() && j < a.size()) {
        if (universe[i] == a[j]) { i++; j++; }
        else if (universe[i] < a[j]) { r.push_back(universe[i]); i++; }
        else { j++; }
    }
    while (i < universe.size()) r.push_back(universe[i++]);
    return r;
}

static std::vector<uint32_t> read_postings(std::ifstream& bin, const DictEntry& e) {
    std::vector<uint32_t> docs;
    if (e.len == 0) return docs;
    docs.resize(e.len / sizeof(uint32_t));
    bin.seekg((std::streamoff)e.offset, std::ios::beg);
    bin.read(reinterpret_cast<char*>(docs.data()), (std::streamsize)e.len);
    return docs;
}

static void load_dict(const std::string& dict_path, std::unordered_map<std::string, DictEntry>& dict) {
    std::ifstream in(dict_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open dict: " + dict_path);

    std::string line;
    std::getline(in, line); // header

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string term, df_s, off_s, len_s;
        if (!std::getline(ss, term, '\t')) continue;
        if (!std::getline(ss, df_s, '\t')) continue;
        if (!std::getline(ss, off_s, '\t')) continue;
        if (!std::getline(ss, len_s, '\t')) continue;

        DictEntry e;
        e.df = (uint32_t)std::stoul(df_s);
        e.offset = (uint64_t)std::stoull(off_s);
        e.len = (uint64_t)std::stoull(len_s);
        dict.emplace(term, e);
    }
}

static uint32_t load_maxdoc(const std::string& maxdoc_path) {
    std::ifstream in(maxdoc_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open maxdoc: " + maxdoc_path);
    uint32_t x = 0;
    in >> x;
    if (!in || x == 0) throw std::runtime_error("Invalid maxdoc in: " + maxdoc_path);
    return x;
}

class Parser {
public:
    Parser(const std::vector<std::string>& toks,
           const std::unordered_map<std::string, DictEntry>& dict,
           std::ifstream& postings,
           const std::vector<uint32_t>& universe)
        : t(toks), dict(dict), postings(postings), universe(universe) {}

    std::vector<uint32_t> parse() {
        pos = 0;
        auto r = parse_or();
        if (pos != t.size()) throw std::runtime_error("Unexpected token at end: " + t[pos]);
        return r;
    }

private:
    const std::vector<std::string>& t;
    const std::unordered_map<std::string, DictEntry>& dict;
    std::ifstream& postings;
    const std::vector<uint32_t>& universe;
    size_t pos = 0;

    bool match_op(const std::string& op) {
        if (pos >= t.size()) return false;
        if (to_upper_ascii(t[pos]) == op) { pos++; return true; }
        return false;
    }

    bool match(const std::string& s) {
        if (pos >= t.size()) return false;
        if (t[pos] == s) { pos++; return true; }
        return false;
    }

    std::vector<uint32_t> postings_for_term(const std::string& raw) {
        std::string term = to_lower_ascii(raw);
        auto it = dict.find(term);
        if (it == dict.end()) return {};
        return read_postings(postings, it->second);
    }

    std::vector<uint32_t> parse_primary() {
        if (match("(")) {
            auto r = parse_or();
            if (!match(")")) throw std::runtime_error("Expected ')'");
            return r;
        }
        if (pos >= t.size()) throw std::runtime_error("Unexpected end");

        std::string u = to_upper_ascii(t[pos]);
        if (u == "AND" || u == "OR" || u == "NOT") {
            throw std::runtime_error("Expected term, got operator: " + t[pos]);
        }
        return postings_for_term(t[pos++]);
    }

    std::vector<uint32_t> parse_not() {
        if (match_op("NOT")) {
            auto r = parse_not();
            return op_not(r, universe);
        }
        return parse_primary();
    }

    std::vector<uint32_t> parse_and() {
        auto left = parse_not();
        while (match_op("AND")) {
            auto right = parse_not();
            left = op_and(left, right);
        }
        return left;
    }

    std::vector<uint32_t> parse_or() {
        auto left = parse_and();
        while (match_op("OR")) {
            auto right = parse_and();
            left = op_or(left, right);
        }
        return left;
    }
};

static void usage() {
    std::cerr
        << "Usage: boolean_search --dict index/dict.tsv --postings index/postings.bin --maxdoc index/maxdoc.txt\n"
        << "Then type queries (AND/OR/NOT, parentheses) line by line.\n";
}

int main(int argc, char** argv) {
    std::string dict_path, postings_path, maxdoc_path;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--dict" && i + 1 < argc) dict_path = argv[++i];
        else if (a == "--postings" && i + 1 < argc) postings_path = argv[++i];
        else if (a == "--maxdoc" && i + 1 < argc) maxdoc_path = argv[++i];
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else { std::cerr << "Unknown arg: " << a << "\n"; usage(); return 2; }
    }

    if (dict_path.empty() || postings_path.empty() || maxdoc_path.empty()) {
        usage();
        return 2;
    }

    std::unordered_map<std::string, DictEntry> dict;
    dict.reserve(200000);

    uint32_t maxDoc = 0;
    try {
        load_dict(dict_path, dict);
        maxDoc = load_maxdoc(maxdoc_path);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    std::ifstream postings(postings_path, std::ios::binary);
    if (!postings) {
        std::cerr << "Cannot open postings: " << postings_path << "\n";
        return 1;
    }

    std::vector<uint32_t> universe;
    universe.reserve(maxDoc);
    for (uint32_t d = 1; d <= maxDoc; d++) universe.push_back(d);

    std::cerr << "Loaded terms: " << dict.size() << "\n";
    std::cerr << "Universe docs: 1.." << maxDoc << "\n";
    std::cerr << "Enter queries. Ctrl+D to exit.\n";

    std::string query;
    while (std::getline(std::cin, query)) {
        if (query.empty()) continue;

        try {
            auto toks = tokenize_query(query);
            Parser p(toks, dict, postings, universe);
            auto res = p.parse();

            std::cout << "RESULTS " << res.size() << "\n";
            for (uint32_t d : res) std::cout << d << "\n";
            std::cout << "END\n";
        } catch (const std::exception& e) {
            std::cout << "ERROR " << e.what() << "\n";
        }
    }

    return 0;
}
