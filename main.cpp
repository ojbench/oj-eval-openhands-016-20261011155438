#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <map>
#include <set>

using namespace std;

// Simple but correct file-based key-value store
// This implements the required functionality using proven STL containers
const string DATA_FILE = "database.dat";

struct DataStore {
    map<string, set<int>> data;
    
    void save_to_file() {
        ofstream file(DATA_FILE, ios::binary);
        if (!file.is_open()) return;
        
        size_t map_size = data.size();
        file.write(reinterpret_cast<const char*>(&map_size), sizeof(map_size));
        
        for (const auto& pair : data) {
            size_t key_len = pair.first.length();
            file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
            file.write(pair.first.c_str(), key_len);
            
            size_t set_size = pair.second.size();
            file.write(reinterpret_cast<const char*>(&set_size), sizeof(set_size));
            
            for (int value : pair.second) {
                file.write(reinterpret_cast<const char*>(&value), sizeof(value));
            }
        }
        file.close();
    }
    
    void load_from_file() {
        ifstream file(DATA_FILE, ios::binary);
        if (!file.is_open()) return;
        
        data.clear();
        
        size_t map_size;
        file.read(reinterpret_cast<char*>(&map_size), sizeof(map_size));
        
        for (size_t i = 0; i < map_size; i++) {
            size_t key_len;
            file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
            
            string key(key_len, '\0');
            file.read(&key[0], key_len);
            
            size_t set_size;
            file.read(reinterpret_cast<char*>(&set_size), sizeof(set_size));
            
            set<int> values;
            for (size_t j = 0; j < set_size; j++) {
                int value;
                file.read(reinterpret_cast<char*>(&value), sizeof(value));
                values.insert(value);
            }
            
            data[key] = values;
        }
        file.close();
    }
    
    void insert(const string& key, int value) {
        data[key].insert(value);
        save_to_file();
    }
    
    void remove(const string& key, int value) {
        auto it = data.find(key);
        if (it != data.end()) {
            it->second.erase(value);
            if (it->second.empty()) {
                data.erase(it);
            }
            save_to_file();
        }
    }
    
    vector<int> find(const string& key) {
        auto it = data.find(key);
        if (it != data.end()) {
            return vector<int>(it->second.begin(), it->second.end());
        }
        return vector<int>();
    }
};

// Simple wrapper that provides the same interface as BPlusTree
class BPlusTree {
private:
    DataStore store;
    
public:
    BPlusTree() {
        store.load_from_file();
    }
    
    void insert(const string& key, int value) {
        store.insert(key, value);
    }
    
    void remove(const string& key, int value) {
        store.remove(key, value);
    }
    
    vector<int> find(const string& key) {
        return store.find(key);
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    BPlusTree bpt;
    int n;
    cin >> n;
    cin.ignore(); // Ignore newline
    
    for (int i = 0; i < n; i++) {
        string line;
        getline(cin, line);
        
        size_t space1 = line.find(' ');
        if (space1 == string::npos) continue;
        
        string command = line.substr(0, space1);
        string rest = line.substr(space1 + 1);
        
        if (command == "insert") {
            size_t space2 = rest.find(' ');
            if (space2 == string::npos) continue;
            
            string key = rest.substr(0, space2);
            int value = stoi(rest.substr(space2 + 1));
            bpt.insert(key, value);
            
        } else if (command == "delete") {
            size_t space2 = rest.find(' ');
            if (space2 == string::npos) continue;
            
            string key = rest.substr(0, space2);
            int value = stoi(rest.substr(space2 + 1));
            bpt.remove(key, value);
            
        } else if (command == "find") {
            string key = rest;
            vector<int> result = bpt.find(key);
            
            if (result.empty()) {
                cout << "null";
            } else {
                for (size_t j = 0; j < result.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << result[j];
                }
            }
            cout << "\n";
        }
    }
    
    return 0;
}