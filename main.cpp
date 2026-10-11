#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <set>

using namespace std;

// B+ Tree implementation for file-based key-value storage
const string DATA_FILE = "bptree.dat";
const int ORDER = 50;  // Order of B+ Tree (maximum children per node)
const int MAX_KEYS = ORDER - 1;  // Maximum keys per node

// B+ Tree node structure
struct BPTNode {
    bool is_leaf;
    int key_count;
    char keys[MAX_KEYS][65];  // Keys (64 bytes + null terminator)
    int children[MAX_KEYS + 1]; // Child node file positions
    int values[MAX_KEYS][100]; // Values for leaf nodes (up to 100 values per key)
    int value_counts[MAX_KEYS]; // Number of values per key
    long next_leaf; // Next leaf node file position
    long file_pos;  // This node's file position
    
    BPTNode() : is_leaf(false), key_count(0), next_leaf(-1), file_pos(-1) {
        memset(children, -1, sizeof(children));
        memset(value_counts, 0, sizeof(value_counts));
    }
};

class BPlusTree {
private:
    fstream data_file;
    long root_pos;
    vector<long> free_positions;
    
    long get_new_position() {
        if (!free_positions.empty()) {
            long pos = free_positions.back();
            free_positions.pop_back();
            return pos;
        }
        data_file.seekp(0, ios::end);
        return data_file.tellp();
    }
    
    void write_node(const BPTNode& node) {
        data_file.seekp(node.file_pos);
        data_file.write(reinterpret_cast<const char*>(&node), sizeof(BPTNode));
        data_file.flush();
    }
    
    void read_node(long pos, BPTNode& node) {
        if (pos == -1) {
            node = BPTNode();
            return;
        }
        data_file.seekg(pos);
        data_file.read(reinterpret_cast<char*>(&node), sizeof(BPTNode));
    }
    
    int find_key_pos(const BPTNode& node, const char* key) {
        int low = 0, high = node.key_count - 1;
        while (low <= high) {
            int mid = (low + high) / 2;
            int cmp = strcmp(key, node.keys[mid]);
            if (cmp == 0) return mid;
            if (cmp < 0) high = mid - 1;
            else low = mid + 1;
        }
        return low;
    }
    
    void insert_into_leaf(BPTNode& leaf, const char* key, int value) {
        int pos = find_key_pos(leaf, key);
        
        // Check if key already exists
        if (pos < leaf.key_count && strcmp(leaf.keys[pos], key) == 0) {
            // Add value if not duplicate
            for (int i = 0; i < leaf.value_counts[pos]; i++) {
                if (leaf.values[pos][i] == value) return;
            }
            
            // Insert value in sorted order
            int insert_pos = 0;
            while (insert_pos < leaf.value_counts[pos] && leaf.values[pos][insert_pos] < value) {
                insert_pos++;
            }
            
            for (int i = leaf.value_counts[pos]; i > insert_pos; i--) {
                leaf.values[pos][i] = leaf.values[pos][i - 1];
            }
            leaf.values[pos][insert_pos] = value;
            leaf.value_counts[pos]++;
        } else {
            // Insert new key
            for (int i = leaf.key_count; i > pos; i--) {
                strcpy(leaf.keys[i], leaf.keys[i - 1]);
                leaf.value_counts[i] = leaf.value_counts[i - 1];
                for (int j = 0; j < leaf.value_counts[i]; j++) {
                    leaf.values[i][j] = leaf.values[i - 1][j];
                }
            }
            
            strcpy(leaf.keys[pos], key);
            leaf.values[pos][0] = value;
            leaf.value_counts[pos] = 1;
            leaf.key_count++;
        }
    }
    
    bool delete_from_leaf(BPTNode& leaf, const char* key, int value) {
        int pos = find_key_pos(leaf, key);
        
        if (pos >= leaf.key_count || strcmp(leaf.keys[pos], key) != 0) {
            return false;
        }
        
        // Find and remove value
        for (int i = 0; i < leaf.value_counts[pos]; i++) {
            if (leaf.values[pos][i] == value) {
                for (int j = i; j < leaf.value_counts[pos] - 1; j++) {
                    leaf.values[pos][j] = leaf.values[pos][j + 1];
                }
                leaf.value_counts[pos]--;
                
                // Remove key if no more values
                if (leaf.value_counts[pos] == 0) {
                    for (int j = pos; j < leaf.key_count - 1; j++) {
                        strcpy(leaf.keys[j], leaf.keys[j + 1]);
                        leaf.value_counts[j] = leaf.value_counts[j + 1];
                        for (int k = 0; k < leaf.value_counts[j]; k++) {
                            leaf.values[j][k] = leaf.values[j + 1][k];
                        }
                    }
                    leaf.key_count--;
                }
                return true;
            }
        }
        return false;
    }
    
    void find_in_leaf(const BPTNode& leaf, const char* key, vector<int>& result) {
        int pos = find_key_pos(leaf, key);
        
        if (pos < leaf.key_count && strcmp(leaf.keys[pos], key) == 0) {
            for (int i = 0; i < leaf.value_counts[pos]; i++) {
                result.push_back(leaf.values[pos][i]);
            }
        }
    }
    
    void search_recursive(long node_pos, const char* key, vector<int>& result) {
        if (node_pos == -1) return;
        
        BPTNode node;
        read_node(node_pos, node);
        
        if (node.is_leaf) {
            find_in_leaf(node, key, result);
            return;
        }
        
        int child_pos = find_key_pos(node, key);
        search_recursive(node.children[child_pos], key, result);
    }
    
    void insert_recursive(long node_pos, const char* key, int value, bool& split, char* split_key, long& new_child_pos) {
        BPTNode node;
        read_node(node_pos, node);
        
        if (node.is_leaf) {
            insert_into_leaf(node, key, value);
            
            if (node.key_count > MAX_KEYS) {
                // Split leaf
                split = true;
                int mid = node.key_count / 2;
                strcpy(split_key, node.keys[mid]);
                
                BPTNode new_leaf;
                new_leaf.is_leaf = true;
                new_leaf.next_leaf = node.next_leaf;
                new_leaf.file_pos = get_new_position();
                
                // Move second half
                new_leaf.key_count = node.key_count - mid;
                for (int i = 0; i < new_leaf.key_count; i++) {
                    strcpy(new_leaf.keys[i], node.keys[mid + i]);
                    new_leaf.value_counts[i] = node.value_counts[mid + i];
                    for (int j = 0; j < new_leaf.value_counts[i]; j++) {
                        new_leaf.values[i][j] = node.values[mid + i][j];
                    }
                }
                
                node.key_count = mid;
                node.next_leaf = new_leaf.file_pos;
                
                write_node(node);
                write_node(new_leaf);
                new_child_pos = new_leaf.file_pos;
            } else {
                write_node(node);
                split = false;
            }
        } else {
            int child_pos = find_key_pos(node, key);
            bool child_split = false;
            char child_split_key[65];
            long child_new_child;
            
            insert_recursive(node.children[child_pos], key, value, child_split, child_split_key, child_new_child);
            
            if (child_split) {
                // Insert split key and child
                int insert_pos = child_pos + 1;
                for (int i = node.key_count; i > insert_pos; i--) {
                    strcpy(node.keys[i], node.keys[i - 1]);
                    node.children[i + 1] = node.children[i];
                }
                strcpy(node.keys[insert_pos], child_split_key);
                node.children[insert_pos + 1] = child_new_child;
                node.key_count++;
                
                if (node.key_count > MAX_KEYS) {
                    // Split internal node
                    split = true;
                    int mid = node.key_count / 2;
                    strcpy(split_key, node.keys[mid]);
                    
                    BPTNode new_internal;
                    new_internal.is_leaf = false;
                    new_internal.file_pos = get_new_position();
                    new_internal.key_count = node.key_count - mid - 1;
                    
                    for (int i = 0; i < new_internal.key_count; i++) {
                        strcpy(new_internal.keys[i], node.keys[mid + 1 + i]);
                        new_internal.children[i] = node.children[mid + 1 + i];
                    }
                    new_internal.children[new_internal.key_count] = node.children[node.key_count];
                    
                    node.key_count = mid;
                    
                    write_node(node);
                    write_node(new_internal);
                    new_child_pos = new_internal.file_pos;
                } else {
                    write_node(node);
                    split = false;
                }
            } else {
                split = false;
            }
        }
    }
    
    bool delete_recursive(long node_pos, const char* key, int value) {
        if (node_pos == -1) return false;
        
        BPTNode node;
        read_node(node_pos, node);
        
        if (node.is_leaf) {
            bool result = delete_from_leaf(node, key, value);
            write_node(node);
            return result;
        } else {
            int child_pos = find_key_pos(node, key);
            return delete_recursive(node.children[child_pos], key, value);
        }
    }
    
public:
    BPlusTree() : root_pos(-1) {
        data_file.open(DATA_FILE, ios::in | ios::out | ios::binary);
        if (!data_file.is_open()) {
            data_file.open(DATA_FILE, ios::out | ios::binary);
            data_file.close();
            data_file.open(DATA_FILE, ios::in | ios::out | ios::binary);
            
            // Create root
            BPTNode root;
            root.is_leaf = true;
            root.file_pos = get_new_position();
            write_node(root);
            root_pos = root.file_pos;
        } else {
            // Read root position
            data_file.seekg(0);
            data_file.read(reinterpret_cast<char*>(&root_pos), sizeof(root_pos));
            if (root_pos == -1) {
                BPTNode root;
                root.is_leaf = true;
                root.file_pos = get_new_position();
                write_node(root);
                root_pos = root.file_pos;
            }
        }
    }
    
    ~BPlusTree() {
        data_file.seekp(0);
        data_file.write(reinterpret_cast<const char*>(&root_pos), sizeof(root_pos));
        data_file.close();
    }
    
    void insert(const string& key, int value) {
        bool split = false;
        char split_key[65];
        long new_child_pos;
        
        insert_recursive(root_pos, key.c_str(), value, split, split_key, new_child_pos);
        
        if (split) {
            // Create new root
            BPTNode new_root;
            new_root.is_leaf = false;
            new_root.key_count = 1;
            strcpy(new_root.keys[0], split_key);
            new_root.children[0] = root_pos;
            new_root.children[1] = new_child_pos;
            new_root.file_pos = get_new_position();
            
            write_node(new_root);
            root_pos = new_root.file_pos;
        }
    }
    
    void remove(const string& key, int value) {
        delete_recursive(root_pos, key.c_str(), value);
    }
    
    vector<int> find(const string& key) {
        vector<int> result;
        search_recursive(root_pos, key.c_str(), result);
        return result;
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