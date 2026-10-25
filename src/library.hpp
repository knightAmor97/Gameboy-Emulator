#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <utility>

struct RomEntry {
    std::string path;
    std::string title;
};

inline std::string readTitle(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    char hdr[0x150] = {};
    f.read(hdr, sizeof(hdr));
    if (f.gcount() < 0x150) return "";
    std::string t(hdr + 0x134, 16);
    t = t.substr(0, t.find('\0'));
    return t;
}

class Romlibrary {
    public:
        explicit Romlibrary(std::string file) : file(std::move(file)) { load(); }
        bool add (const std::string& path) {
            for (auto& e : entries) if (e.path == path) return false;
            std::string title = readTitle(path);
            if (title.empty()) return false;
            entries.push_back({path, title});
            save();
            return true;
        }
        void remove (size_t i) {
            if (i < entries.size()) { entries.erase(entries.begin() + i); save(); }
        }

        const std::vector<RomEntry> &list() const { return entries; }
    private:
        std::string file;
        std::vector <RomEntry> entries;

        void load() {
            std::ifstream f(file);
            std::string line;
            while (std::getline(f, line)) {
                if (line.empty()) continue;
                std::string title = readTitle(line);
                if(!title.empty()) entries.push_back({line, title});
            }
        }
        void save() {
            std::ofstream f(file);
            for (auto& e : entries) f << e.path << "\n";
        }
};