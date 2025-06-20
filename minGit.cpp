#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <chrono>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <queue>


using namespace std;
using namespace filesystem;

struct CommitNode {
    string id;
    string stageSnap;
    string prevCommit;
    string timestamp;
    string author;
    string comment;
};

class MinGit {
private:
    path repoPath;
    string repoName;
    string author;



    // void recordAuthor(const string& branch, const string& author);
    // unordered_map<string, string> stageOf(const path& stagePath);
    // string blob(const path& filePath);
    // string activeBranch();
    // string hashOf(const path& filePath);
    // string hashOf(const string& content);


    string latestCommit() {
        ifstream h(repoPath / "HEAD");
        string ref;
        getline(h, ref);
        h.close();
        return latestCommit(ref);
    }
    string latestCommit(const string& branch) {
        path refPath = repoPath / "refs" / branch;
        if (!exists(refPath)) return "";
        ifstream in(refPath);
        string id;
        getline(in, id);
        in.close();
        return id;
    }
    string Author(const string& branch) {
        path authorPath = repoPath / "authors" / branch;
        ifstream in(authorPath);
        string name;
        if (in) getline(in, name);
        return name;
    }
    string commitString(const CommitNode& node) {
        string commit;
        commit += "stageSnapshot: " + node.stageSnap + "\n";
        commit += "prevCommit: "    + node.prevCommit + "\n";
        commit += "timestamp: "     + node.timestamp + "\n";
        commit += "author: "        + node.author    + "\n";
        commit += "comment: "       + node.comment   + "\n";
        return commit;
    }


    // string findHash(const string& targetName);
    // void recover(const string& commitID);


    // unordered_map<string, string> stageOf(const string& commitID);
    // CommitNode CommitData(const string& id);


    // string manageConflict(const string& base, const string& current, const string& source);
    // string mergeStage(const unordered_map<string, string>& stageMap);
    // string time();
    // string commonAncestor(const string& commitA, const string& commitB);


public:

    MinGit(const string& path, const string& author) {
        repoPath = path + "/.minigit";
        repoName = repoPath.parent_path().filename().string();
        this->author = author;
    }
    // void init();
    // void add(const path& filePath);
    // void commit(const string& comment);
    // void log();
    // void branch(const string& name, const string& author);
    // void checkout(const string& targetHash);
    // void merge(const string& branch);
};
