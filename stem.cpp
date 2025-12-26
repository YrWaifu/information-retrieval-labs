#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static bool ends_with(const std::string& w, const std::string& suf) {
    return w.size() >= suf.size() &&
           w.compare(w.size() - suf.size(), suf.size(), suf) == 0;
}

static void replace_suffix(std::string& w, const std::string& suf, const std::string& repl) {
    w.replace(w.size() - suf.size(), suf.size(), repl);
}

static std::string stem_word(std::string w) {
    if (w.size() < 3) return w;

    // plural
    if (ends_with(w, "sses")) replace_suffix(w, "sses", "ss");
    else if (ends_with(w, "ies")) replace_suffix(w, "ies", "i");
    else if (ends_with(w, "s") && !ends_with(w, "ss")) w.pop_back();

    // past / gerund
    if (ends_with(w, "ing") && w.size() > 5) w.erase(w.size() - 3);
    else if (ends_with(w, "ed") && w.size() > 4) w.erase(w.size() - 2);

    // common suffixes
    if (ends_with(w, "ational")) replace_suffix(w, "ational", "ate");
    else if (ends_with(w, "tional")) replace_suffix(w, "tional", "tion");
    else if (ends_with(w, "izer")) replace_suffix(w, "izer", "ize");
    else if (ends_with(w, "ness")) w.erase(w.size() - 4);
    else if (ends_with(w, "ment")) w.erase(w.size() - 4);
    else if (ends_with(w, "ful")) w.erase(w.size() - 3);
    else if (ends_with(w, "less")) w.erase(w.size() - 4);

    return w;
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string token;
    while (std::cin >> token) {
        std::cout << stem_word(token) << "\n";
    }
    return 0;
}
