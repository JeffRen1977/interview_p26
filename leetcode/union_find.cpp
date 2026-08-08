#include <iostream>
#include <vector>
#include <numeric>

class UnionFind {
private:
    std::vector<int> parent;
    std::vector<int> size;
    int numSets;

public:
    // Initialize DSU with n elements (0 to n - 1)
    explicit UnionFind(int n) : parent(n), size(n, 1), numSets(n) {
        std::iota(parent.begin(), parent.end(), 0); // parent[i] = i
    }

    // Find the representative (root) of the set containing 'i'
    // Uses Path Compression to flatten the tree structure
    int find(int i) {
        if (parent[i] == i)
            return i;
        return parent[i] = find(parent[i]); // Path compression
    }

    // Connect elements 'i' and 'j'
    // Uses Union by Size to keep trees balanced
    bool unite(int i, int j) {
        int rootI = find(i);
        int rootJ = find(j);

        if (rootI != rootJ) {
            // Attach the smaller tree under the root of the larger tree
            if (size[rootI] < size[rootJ]) {
                std::swap(rootI, rootJ);
            }
            parent[rootJ] = rootI;
            size[rootI] += size[rootJ];
            numSets--;
            return true; // Successfully merged two different sets
        }
        return false; // Already in the same set
    }

    // Check if element 'i' and 'j' are in the same set
    bool connected(int i, int j) {
        return find(i) == find(j);
    }

    // Get the size of the set containing element 'i'
    int getSetSize(int i) {
        return size[find(i)];
    }

    // Return the total number of disjoint sets
    int count() const {
        return numSets;
    }
};
