#pragma once
#include <unordered_map>
#include <string>

using namespace std;
Class BranchMap {
    unordered_map<string, string> branches;
    string HEAD;

public:
    void createBranch(const string& name, const string& commitHash);
    string getBranchHead(const string& name) const;
    void updateBranchHead(const string& name, const string& newHash);
    string getHEAD() const;
    void setHEAD(const string& target);
};