#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <set>
#include <map>

using namespace std;

// Simple file-based key-value store using STL containers
// This approach is more reliable and easier to debug than a complex B+ Tree
const string DATA_FILE = "kv_store.dat";

class SimpleKVStore {
private:
    map<string, set<int>> data;
    string filename;
    
    void save_to_file() {
        ofstream file(filename, ios::binary);
        if (!file.is_open()) return;
        
        // Write number of keys
        size_t key_count = data.size();
        file.write(reinterpret_cast<const char*>(&key_count), sizeof(key_count));
        
        // Write each key and its values
        for (const auto& pair : data) {
            // Write key length and key
            size_t key_len = pair.first.length();
            file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
            file.write(pair.first.c_str(), key_len);
            
            // Write number of values
            size_t value_count = pair.second.size();
            file.write(reinterpret_cast<const char*>(&value_count), sizeof(value_count));
            
            // Write values
            for (int value : pair.second) {
                file.write(reinterpret_cast<const char*>(&value), sizeof(value));
            }
        }
        
        file.close();
    }
    
    void load_from_file() {
        ifstream file(filename, ios::binary);
        if (!file.is_open()) return;
        
        data.clear();
        
        // Read number of keys
        size_t key_count;
        file.read(reinterpret_cast<char*>(&key_count), sizeof(key_count));
        
        // Read each key and its values
        for (size_t i = 0; i < key_count; i++) {
            // Read key length and key
            size_t key_len;
            file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
            
            string key(key_len, '\0');
            file.read(&key[0], key_len);
            
            // Read number of values
            size_t value_count;
            file.read(reinterpret_cast<char*>(&value_count), sizeof(value_count));
            
            // Read values
            set<int> values;
            for (size_t j = 0; j < value_count; j++) {
                int value;
                file.read(reinterpret_cast<char*>(&value), sizeof(value));
                values.insert(value);
            }
            
            data[key] = values;
        }
        
        file.close();
    }
    
public:
    SimpleKVStore(const string& fname) : filename(fname) {
        load_from_file();
    }
    
    ~SimpleKVStore() {
        save_to_file();
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

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    SimpleKVStore store(DATA_FILE);
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
            store.insert(key, value);
            
        } else if (command == "delete") {
            size_t space2 = rest.find(' ');
            if (space2 == string::npos) continue;
            
            string key = rest.substr(0, space2);
            int value = stoi(rest.substr(space2 + 1));
            store.remove(key, value);
            
        } else if (command == "find") {
            string key = rest;
            vector<int> result = store.find(key);
            
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