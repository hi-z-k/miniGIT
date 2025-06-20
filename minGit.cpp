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



    void recordAuthor(const string& branch, const string& author) {
        path authorPath = repoPath / "authors" / branch;
        ofstream out(authorPath);
        out << author;
        out.close();
    }
    unordered_map<string, string> stageOf(const path& stagePath) {
        unordered_map<string, string> stage;
        ifstream sStream(stagePath);
        string line;
        while (getline(sStream, line)) {
            istringstream lStream(line);
            string file, hash;
            if (lStream >> file >> hash)
                stage[file] = hash;
        }
        return stage;
    }
    string blob(const path& filePath) {
        path fullPath = repoPath.parent_path() / filePath;
        string blobFile = hashOf(fullPath);
        if (blobFile.empty()) return "";
    
        path blobPath = repoPath / "objects" / blobFile;
        if (!exists(blobPath))
            copy_file(fullPath, blobPath, copy_options::overwrite_existing);
        return blobFile;
    }
    string activeBranch() {
        ifstream head(repoPath / "HEAD");
        string branch;
        getline(head, branch);
        return branch;
    }
    string hashOf(const path& filePath) {
        ifstream file(filePath, ios::binary);
        if (!file) return "";
        stringstream buffer;
        buffer << file.rdbuf();
        return hashOf(buffer.str());
    }
    string hashOf(const string& content) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);
        EVP_DigestUpdate(ctx, content.c_str(), content.size());
    
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int len;
        EVP_DigestFinal_ex(ctx, hash, &len);
        EVP_MD_CTX_free(ctx);
    
        stringstream ss;
        for (unsigned int i = 0; i < len; ++i)
            ss << hex << setw(2) << setfill('0') << static_cast<int>(hash[i]);
        return ss.str();
    }


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


    string findHash(const string& targetName) {
        path branchPath = repoPath / "refs" / targetName;
        path commitPath = repoPath / "commits" / targetName;
    
        if (exists(branchPath)) {
            ifstream bStream(branchPath);
            string commitID;
            getline(bStream, commitID);
            bStream.close();
            return commitID;
        }
    
        if (exists(commitPath)) return targetName;
    
        cerr << "reference - " << targetName << " doesn't exist\n";
        return "";
    }
    // void recover(const string& commitID);


    unordered_map<string, string> stageOf(const string& commitID) {
        string stageHash = CommitData(commitID).stageSnap;
        path stagePath = repoPath / "objects" / stageHash;
        return stageOf(stagePath);
    }
    CommitNode CommitData(const string& id) {
        CommitNode node;
        node.id = id;
    
        path commitPath = repoPath / "commits" / id;
        ifstream file(commitPath);
        if (!file) {
            cerr << "Unable to load the commit - " << id << "\n";
            return node;
        }
    
        string line;
        getline(file, line);
        node.stageSnap = line.substr(string("stageSnapshot: ").size());
    
        getline(file, line);
        node.prevCommit = line.substr(string("prevCommit: ").size());
    
        getline(file, line);
        node.timestamp = line.substr(string("timestamp: ").size());
    
        getline(file, line);
        node.author = line.substr(string("author: ").size());
    
        getline(file, line);
        node.comment = line.substr(string("comment: ").size());
    
        return node;
    }


    string manageConflict(const string& base, const string& current, const string& source) {  
    if (current == source || source.empty()) return current;  
    if (current.empty())                     return source;  
    if (base == current)                     return source;  
    if (base == source)                      return current;  
    return "";  
}
    string mergeStage(const unordered_map<string, string>& stageMap) {  
    string pathStr = (repoPath / "merge-stage").string();  
    ofstream oStream(pathStr);  
    for (const auto& [fileName, hash] : stageMap)  
        oStream << fileName << " " << hash << "\n";  
    oStream.close();  
    return pathStr;  
} 
    string time() {
    time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
    string t = ctime(&now);
    t.pop_back();
    return t;
}
    // string commonAncestor(const string& commitA, const string& commitB);


public:

    MinGit(const string& path, const string& author) {
        repoPath = path + "/.minigit";
        repoName = repoPath.parent_path().filename().string();
        this->author = author;
    }

      void init() {
        if (exists(repoPath)) {
            cout << "It is already initialized\n";
            return;
        }

        create_directory(repoPath);
        create_directory(repoPath / "objects");
        create_directory(repoPath / "refs");
        create_directory(repoPath / "commits");
        create_directory(repoPath / "stages");
        create_directory(repoPath / "authors");

        ofstream headFile(repoPath / "HEAD");
        headFile << "master";
        headFile.close();

        branch("master", author);
        cout << repoName << " is initialized successfully\n";
    }

    void add(const path& filePath) {
        string branch = activeBranch();
        path stagePath = repoPath / "stages" / branch;

        if (!exists(stagePath)) {
            ofstream indexStream(stagePath, ios::app);
            indexStream.close();
        }

        path rootPath = repoPath.parent_path();
        path relativeRepo = rootPath / filePath;
        path absolutePath = filesystem::weakly_canonical(relativeRepo);

        string relativePath = relative(absolutePath, repoPath.parent_path()).string();

        unordered_map<string, string> stageMap = stageOf(stagePath);

        if (!exists(absolutePath)) {
            cerr << "File doesn't exist - " << relativePath << "\n";
            return;
        }

        string blobFile = blob(absolutePath);
        if (blobFile.empty()) {
            cerr << "blobbing failed - " << relativePath << "\n";
            return;
        }

        bool alreadyStaged = stageMap.count(relativePath);

        if (alreadyStaged && stageMap[relativePath] == blobFile) {
            cout << "File is already staged - " << relativePath << "\n";
            return;
        }

        stageMap[relativePath] = blobFile;

        ofstream stageOutput(stagePath, ios::trunc);
        for (const auto& [file, hash] : stageMap) {
            stageOutput << file << " " << hash << "\n";
        }
        stageOutput.close();

        if (alreadyStaged)
            cout << "File is restaged with new content - " << relativePath << "\n";
        else
            cout << "File is staged - " << relativePath << "\n";
    }

    void commit(const string& comment) {
        string branch = activeBranch();
        path stagePath = repoPath / "stages" / branch;

        if (!exists(stagePath)) {
            cerr << "Stage file is missing\n";
            return;
        }

        unordered_map<string, string> staged = stageOf(stagePath);
        if (staged.empty()) {
            cerr << "No changes to commit — stage is empty\n";
            return;
        }

        string snapBlob = blob(stagePath);
        if (snapBlob.empty()) {
            cerr << "Unable to snapshot the stage\n";
            return;
        }

        string lastCommit   = latestCommit();
        string timestamp    = time();
        string branchAuthor = Author(branch);

        CommitNode node;
        node.stageSnap  = snapBlob;
        node.prevCommit = lastCommit;
        node.timestamp  = timestamp;
        node.author     = branchAuthor;
        node.comment    = comment;

        string commit = commitString(node);
        string commitHash = hashOf(commit);

        ofstream(repoPath / "commits" / commitHash) << commit;
        ofstream(repoPath / "refs" / branch)        << commitHash;
        ofstream(repoPath / "stages" / branch, ios::trunc).close();

        cout << "commit @" << commitHash << " - " << comment << "\n";
    }

    void log() {
        string id = latestCommit();
        if (id == "NONE") {
            cout << "There are no commits yet on this branch.\n";
            return;
        }

        cout << "\n=========== LOG HISTORY ===========\n\n";
        while (!id.empty() && id != "NONE") {
            CommitNode node = CommitData(id);

            if (node.id.empty()) {
                cerr << "Invalid or unreadable commit: " << id << "\n";
                break;
            }

            cout << "<" << node.author << "> @ " << node.id << " - \"" << node.comment << "\"\n";
            cout << node.timestamp << "\n";

            if (node.prevCommit.empty()) {
                cout << "Previous commit: NONE\n";
                break;
            }

            int commaIdx = node.prevCommit.find(',');
            string prevCommitID = (commaIdx != string::npos) ? node.prevCommit.substr(0, commaIdx) : node.prevCommit;

            cout << "Previous commit: " << prevCommitID << "\n";
            cout << "----------------------------------------\n";
            id = prevCommitID;
        }
    }

    void branch(const string& name, const string& Author) {
        path branchPath = repoPath / "refs" / name;
        if (exists(branchPath)) {
            cout << "It already exists - Branch" << name << "\n";
            return;
        }

        string commitHash = latestCommit();

        ofstream branchRef(branchPath);
        branchRef << commitHash;
        branchRef.close();

        recordAuthor(name, Author);

        cout << "Branch @" << name << " successfully created at commit " << commitHash << "\n";
    }

    // void checkout(const string& targetHash);

    // void merge(const string& branch);
};


int main() {
    string repoPath, author;

    cout << "===========================\n";
    cout << "     Welcome to MiniGit    \n";
    cout << "===========================\n";

    cout << "Enter a path for a folder: ";
    getline(cin, repoPath);

    cout << "Enter Author: ";
    getline(cin, author);

    MinGit git(repoPath, author);
    string directoryName = path(repoPath).filename().string();

    cout << "Type 'help' to see available commands.\n\n";

    string input;
    while (true) {
        cout << "miniGIT<" << directoryName << ">$ ";
        getline(cin, input);
        istringstream stream(input);
        string keyword;
        stream >> keyword;

        if (keyword == "exit") {
            break;

        } else if (keyword == "help") {
            cout << "Commands:\n"
                 << "  init\n"
                 << "  add <file>\n"
                 << "  commit <message>\n"
                 << "  log\n"
                 << "  branch <name> <author>\n"
                 << "  checkout <target>\n"
                 << "  merge <branch>\n"
                 << "  exit\n";

        } else if (keyword == "init") {
            git.init();

        } else if (keyword == "add") {
            string file;
            stream >> file;
            if (file.empty())
                cout << "Please input the file path.\n";
            else
                git.add(file);

        } else if (keyword == "commit") {
            string message;
            getline(stream, message);
            if (!message.empty() && message[0] == ' ') message.erase(0, 1);
            git.commit(message);

        } else if (keyword == "log") {
            git.log();

        } else if (keyword == "branch") {
            string name, branchAuthor;
            stream >> name >> branchAuthor;
            if (name.empty() || branchAuthor.empty())
                cout << "Please input the branch name and author.\n";
            else
                git.branch(name, branchAuthor);

        } else if (keyword == "checkout") {
            string target;
            stream >> target;
            if (target.empty())
                cout << "Please input the target (branch or commit hash).\n";
            else
                git.checkout(target);

        } else if (keyword == "merge") {
            string branch;
            stream >> branch;
            if (branch.empty())
                cout << "Please input the branch to merge.\n";
            else
                git.merge(branch);

        } else {
            cout << "Unknown command.\n";
        }

        cout << "\n";
    }

    cout << "Terminated.\n";
    return 0;
}


