#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <set>
#include <map>

using namespace std;

// Constants for B+ Tree
const int MAX_KEYS = 100;  // Maximum keys per node
const int MAX_VALUES = 50; // Maximum values per key in leaf
const string DATA_FILE = "bptree_data.dat";

// Structure to store key-value pairs
struct KeyValue {
    char key[65];  // 64 bytes + null terminator
    int value;
    
    bool operator<(const KeyValue& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }
};

// B+ Tree Node structure
struct BPTNode {
    bool is_leaf;
    int key_count;
    char keys[MAX_KEYS][65];  // Keys
    int children[MAX_KEYS + 1]; // Child node indices (for internal nodes)
    int values[MAX_KEYS][MAX_VALUES]; // Values for leaf nodes
    int value_counts[MAX_KEYS]; // Number of values per key for leaf nodes
    int next_leaf; // Next leaf node (for leaf nodes)
    
    BPTNode() : is_leaf(false), key_count(0), next_leaf(-1) {
        memset(children, -1, sizeof(children));
        memset(value_counts, 0, sizeof(value_counts));
    }
};

class BPlusTree {
private:
    fstream data_file;
    int root_index;
    vector<int> free_nodes;
    
    int allocate_node() {
        if (!free_nodes.empty()) {
            int index = free_nodes.back();
            free_nodes.pop_back();
            return index;
        }
        
        data_file.seekp(0, ios::end);
        return data_file.tellp() / sizeof(BPTNode);
    }
    
    void write_node(int index, const BPTNode& node) {
        data_file.seekp(index * sizeof(BPTNode));
        data_file.write(reinterpret_cast<const char*>(&node), sizeof(BPTNode));
        data_file.flush();
    }
    
    void read_node(int index, BPTNode& node) {
        data_file.seekg(index * sizeof(BPTNode));
        data_file.read(reinterpret_cast<char*>(&node), sizeof(BPTNode));
    }
    
    int find_child(const BPTNode& node, const char* key) {
        for (int i = node.key_count - 1; i >= 0; i--) {
            if (strcmp(key, node.keys[i]) >= 0) {
                return i + 1;
            }
        }
        return 0;
    }
    
    int find_key_index(const BPTNode& node, const char* key) {
        for (int i = 0; i < node.key_count; i++) {
            if (strcmp(node.keys[i], key) == 0) {
                return i;
            }
        }
        return -1;
    }
    
    void insert_into_leaf(BPTNode& leaf, const char* key, int value) {
        int pos = 0;
        while (pos < leaf.key_count && strcmp(leaf.keys[pos], key) < 0) {
            pos++;
        }
        
        if (pos < leaf.key_count && strcmp(leaf.keys[pos], key) == 0) {
            // Key exists, add value if not duplicate
            for (int i = 0; i < leaf.value_counts[pos]; i++) {
                if (leaf.values[pos][i] == value) {
                    return; // Value already exists
                }
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
            // New key, shift existing keys and values
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
        int key_idx = find_key_index(leaf, key);
        if (key_idx == -1) return false;
        
        // Find and remove the value
        for (int i = 0; i < leaf.value_counts[key_idx]; i++) {
            if (leaf.values[key_idx][i] == value) {
                // Remove value
                for (int j = i; j < leaf.value_counts[key_idx] - 1; j++) {
                    leaf.values[key_idx][j] = leaf.values[key_idx][j + 1];
                }
                leaf.value_counts[key_idx]--;
                
                // If no more values for this key, remove the key
                if (leaf.value_counts[key_idx] == 0) {
                    for (int j = key_idx; j < leaf.key_count - 1; j++) {
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
        int key_idx = find_key_index(leaf, key);
        if (key_idx != -1) {
            for (int i = 0; i < leaf.value_counts[key_idx]; i++) {
                result.push_back(leaf.values[key_idx][i]);
            }
        }
    }
    
    void search_leaf(int node_index, const char* key, vector<int>& result) {
        if (node_index == -1) return;
        
        BPTNode node;
        read_node(node_index, node);
        
        if (node.is_leaf) {
            find_in_leaf(node, key, result);
            return;
        }
        
        int child_idx = find_child(node, key);
        search_leaf(node.children[child_idx], key, result);
    }
    
    void insert_recursive(int node_index, const char* key, int value, bool& split, char* split_key, int& new_child) {
        BPTNode node;
        read_node(node_index, node);
        
        if (node.is_leaf) {
            insert_into_leaf(node, key, value);
            
            if (node.key_count > MAX_KEYS) {
                // Split leaf node
                split = true;
                int mid = node.key_count / 2;
                strcpy(split_key, node.keys[mid]);
                
                BPTNode new_leaf;
                new_leaf.is_leaf = true;
                new_leaf.next_leaf = node.next_leaf;
                node.next_leaf = allocate_node();
                
                // Move second half to new leaf
                new_leaf.key_count = node.key_count - mid;
                for (int i = 0; i < new_leaf.key_count; i++) {
                    strcpy(new_leaf.keys[i], node.keys[mid + i]);
                    new_leaf.value_counts[i] = node.value_counts[mid + i];
                    for (int j = 0; j < new_leaf.value_counts[i]; j++) {
                        new_leaf.values[i][j] = node.values[mid + i][j];
                    }
                }
                node.key_count = mid;
                
                write_node(node_index, node);
                write_node(node.next_leaf, new_leaf);
                new_child = node.next_leaf;
            } else {
                write_node(node_index, node);
                split = false;
            }
        } else {
            int child_idx = find_child(node, key);
            bool child_split = false;
            char child_split_key[65];
            int child_new_child;
            
            insert_recursive(node.children[child_idx], key, value, child_split, child_split_key, child_new_child);
            
            if (child_split) {
                // Insert split key and new child into internal node
                int pos = child_idx + 1;
                for (int i = node.key_count; i > pos; i--) {
                    strcpy(node.keys[i], node.keys[i - 1]);
                    node.children[i + 1] = node.children[i];
                }
                strcpy(node.keys[pos], child_split_key);
                node.children[pos + 1] = child_new_child;
                node.key_count++;
                
                if (node.key_count > MAX_KEYS) {
                    // Split internal node
                    split = true;
                    int mid = node.key_count / 2;
                    strcpy(split_key, node.keys[mid]);
                    
                    BPTNode new_internal;
                    new_internal.is_leaf = false;
                    new_internal.key_count = node.key_count - mid - 1;
                    
                    for (int i = 0; i < new_internal.key_count; i++) {
                        strcpy(new_internal.keys[i], node.keys[mid + 1 + i]);
                        new_internal.children[i] = node.children[mid + 1 + i];
                    }
                    new_internal.children[new_internal.key_count] = node.children[node.key_count];
                    
                    node.key_count = mid;
                    
                    int new_internal_idx = allocate_node();
                    write_node(node_index, node);
                    write_node(new_internal_idx, new_internal);
                    new_child = new_internal_idx;
                } else {
                    write_node(node_index, node);
                    split = false;
                }
            } else {
                split = false;
            }
        }
    }
    
public:
    BPlusTree() : root_index(-1) {
        // Open or create data file
        data_file.open(DATA_FILE, ios::in | ios::out | ios::binary);
        if (!data_file.is_open()) {
            // Create new file
            data_file.open(DATA_FILE, ios::out | ios::binary);
            data_file.close();
            data_file.open(DATA_FILE, ios::in | ios::out | ios::binary);
            
            // Create root node
            BPTNode root;
            root.is_leaf = true;
            root_index = allocate_node();
            write_node(root_index, root);
        } else {
            // Read root index from file header
            data_file.seekg(0);
            data_file.read(reinterpret_cast<char*>(&root_index), sizeof(root_index));
            if (root_index == -1) {
                BPTNode root;
                root.is_leaf = true;
                root_index = allocate_node();
                write_node(root_index, root);
            }
        }
    }
    
    ~BPlusTree() {
        // Write root index to file header
        data_file.seekp(0);
        data_file.write(reinterpret_cast<const char*>(&root_index), sizeof(root_index));
        data_file.close();
    }
    
    void insert(const string& key, int value) {
        bool split = false;
        char split_key[65];
        int new_child;
        
        insert_recursive(root_index, key.c_str(), value, split, split_key, new_child);
        
        if (split) {
            // Create new root
            BPTNode new_root;
            new_root.is_leaf = false;
            new_root.key_count = 1;
            strcpy(new_root.keys[0], split_key);
            new_root.children[0] = root_index;
            new_root.children[1] = new_child;
            
            int new_root_idx = allocate_node();
            write_node(new_root_idx, new_root);
            root_index = new_root_idx;
        }
    }
    
    void remove(const string& key, int value) {
        // For simplicity, we'll just mark as deleted by not finding it
        // Full B+ tree deletion with merging/borrowing is complex
        // For this problem, we can use a simpler approach
        delete_recursive(root_index, key.c_str(), value);
    }
    
    bool delete_recursive(int node_index, const char* key, int value) {
        if (node_index == -1) return false;
        
        BPTNode node;
        read_node(node_index, node);
        
        if (node.is_leaf) {
            bool result = delete_from_leaf(node, key, value);
            write_node(node_index, node);
            return result;
        } else {
            int child_idx = find_child(node, key);
            return delete_recursive(node.children[child_idx], key, value);
        }
    }
    
    vector<int> find(const string& key) {
        vector<int> result;
        search_leaf(root_index, key.c_str(), result);
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