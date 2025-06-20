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


/**
 * recordAuthor - registers the author and associates it with a branch name
 * @branch: the name of the branch
 * @author: the author name to be recorded
 */
    void recordAuthor(const string& branch, const string& author) {
        path authorPath = repoPath / "authors" / branch;
        ofstream out(authorPath);
        out << author;
        out.close();
    }
/**
 * stageOf - transforms a stage from a stage file path into a map
 * @stagePath: path to the stage snapshot file
 * Return: map of staged files and their hashes
 */
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
/**
 * blob - Stores a file by the hash as a name in the object directory if it is not already stored
 * @filePath: path to the file to be stored
 * Return: hash filename for the blob
 */
    string blob(const path& filePath) {
        path fullPath = repoPath.parent_path() / filePath;
        string blobFile = hashOf(fullPath);
        if (blobFile.empty()) return "";
    
        path blobPath = repoPath / "objects" / blobFile;
        if (!exists(blobPath))
            copy_file(fullPath, blobPath, copy_options::overwrite_existing);
        return blobFile;
    }
/**
 * activeBranch - takes the current active branch from HEAD file
 * Return: name of the current branch
 */
    string activeBranch() {
        ifstream head(repoPath / "HEAD");
        string branch;
        getline(head, branch);
        return branch;
    }
/**
 * hashOf - Generates SHA hash when provided a file path
 * @filePath: path to the file
 * Return: SHA hash of the file's contents
 */
    string hashOf(const path& filePath) {
        ifstream file(filePath, ios::binary);
        if (!file) return "";
        stringstream buffer;
        buffer << file.rdbuf();
        return hashOf(buffer.str());
    }
/**
 * hashOf - Generates SHA hash from a string provided
 * @content: input string
 * Return: SHA hash 
 */
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

/**
 * latestCommit - fetches the latest commit ID from the currently active branch
 * Return: latest commit ID 
 */
    string latestCommit() {
        ifstream h(repoPath / "HEAD");
        string ref;
        getline(h, ref);
        h.close();
        return latestCommit(ref);
    }
/**
 * latestCommit - fetchs latest commit ID of a given branch
 * @branch: the branch name
 * Return: latest commit ID for the branch
 */
    string latestCommit(const string& branch) {
        path refPath = repoPath / "refs" / branch;
        if (!exists(refPath)) return "";
        ifstream in(refPath);
        string id;
        getline(in, id);
        in.close();
        return id;
    }
/**
 * Author - gets the author of a branch
 * @branch: name of the branch
 * Return: author's name as string
 */
    string Author(const string& branch) {
        path authorPath = repoPath / "authors" / branch;
        ifstream in(authorPath);
        string name;
        if (in) getline(in, name);
        return name;
    }
/**
 * commitString - converts the commit node to strings
 * @node: the commit node structure
 * Return: string of the commit content
 */
    string commitString(const CommitNode& node) {
        string commit;
        commit += "stageSnapshot: " + node.stageSnap + "\n";
        commit += "prevCommit: "    + node.prevCommit + "\n";
        commit += "timestamp: "     + node.timestamp + "\n";
        commit += "author: "        + node.author    + "\n";
        commit += "comment: "       + node.comment   + "\n";
        return commit;
    }

/**
 * findHash - resolves a reference (branch or commit ID) into a valid commit hash
 * @targetName: the name of branch or commit ID
 * Return: resolved commit hash 
 */
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
/**
 * recover - Restores files when provided a given commit ID
 * @commitID: ID of the commit to recover from
 */
    void recover(const string& commitID) {
        unordered_map<string, string> stage = stageOf(commitID);
        if (stage.empty()) {
            cerr << "no stage found for the commit - " << commitID << "\n";
            return;
        }
    
        for (const auto& [relativePath, blobHash] : stage) {
            path blobPath = repoPath / "objects" / blobHash;
            path fullPath = repoPath.parent_path() / relativePath;
    
            create_directories(fullPath.parent_path());
    
            ifstream bStream(blobPath, ios::binary);
            ofstream rStream(fullPath, ios::binary);
            rStream << bStream.rdbuf();
            bStream.close();
            rStream.close();
        }
    
        cout << "Files are restored successfully from commit - " << commitID << "\n";
    }

/**
 * stageOf - transforms a stage from a commit ID into a map 
 * @commitID: hash of the commit
 * Return: map of staged files and their hashes
 */
    unordered_map<string, string> stageOf(const string& commitID) {
        string stageHash = CommitData(commitID).stageSnap;
        path stagePath = repoPath / "objects" / stageHash;
        return stageOf(stagePath);
    }

/**
 * CommitData - Loads commit details from a commit ID
 * @id: the commit hash
 * Return: CommitNode struct with data
 */
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

/**
 * manageConflict - resolves merge conflicts using a simple 3 way merge
 * @base: ancestor version of the file
 * @current: current version in active branch
 * @source: version from incoming branch
 * Return: resolved version or empty string if conflict remains
 */

    string manageConflict(const string& base, const string& current, const string& source) {  
    if (current == source || source.empty()) return current;  
    if (current.empty())                     return source;  
    if (base == current)                     return source;  
    if (base == source)                      return current;  
    return "";  
}

/**
 * mergeStage - records merged staging map to disk as temporary stage file
 * @stageMap: map of filename to blob hash
 * Return: path to the temporary stage file
 */
    string mergeStage(const unordered_map<string, string>& stageMap) {  
    string pathStr = (repoPath / "merge-stage").string();  
    ofstream oStream(pathStr);  
    for (const auto& [fileName, hash] : stageMap)  
        oStream << fileName << " " << hash << "\n";  
    oStream.close();  
    return pathStr;  
} 

/**
 * time - Gets the current time as string
 * Return: timestamp string
 */
    string time() {
    time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
    string t = ctime(&now);
    t.pop_back();
    return t;
}

/**
 * commonAncestor - finds the first common ancestor between two commits
 * @commitA: first commit hash
 * @commitB: second commit hash
 * Return: common ancestor commit hash 
 */


    string commonAncestor(const string& commitA, const string& commitB) {  
    unordered_set<string> ancestorsA;  
    queue<string> lineageA;  
    lineageA.push(commitA);  
  
    while (!lineageA.empty()) {  
        string commitID = lineageA.front(); lineageA.pop();  
        if (!ancestorsA.insert(commitID).second) continue;  
  
        istringstream parentStream(CommitData(commitID).prevCommit);  
        string parent;  
        while (getline(parentStream, parent, ','))  
            if (!parent.empty()) lineageA.push(parent);  
    }  
  
    queue<string> lineageB;  
    lineageB.push(commitB);  
  
    while (!lineageB.empty()) {  
        string commitID = lineageB.front(); lineageB.pop();  
        if (ancestorsA.count(commitID)) return commitID;  
  
        istringstream parentStream(CommitData(commitID).prevCommit);  
        string parent;  
        while (getline(parentStream, parent, ','))  
            if (!parent.empty()) lineageB.push(parent);  
    }  
  
    return "";  
}


public:

/**
 * MinGit - Constructor for the MinGit object
 * @path: the absolute path of the repository
 * @author: author name for master branch
 */
    MinGit(const string& path, const string& author) {
        repoPath = path + "/.minigit";
        repoName = repoPath.parent_path().filename().string();
        this->author = author;
    }

/**
 * init - Initializes .MiniGit repository if not already present
 */
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

/**
 * add - stages a file for commit by recording its blob hash
 * @filePath: path to the file to be added
 */
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

/**
 * commit - creates a new commit using the staged files
 * @comment: commit message to describe the changes
 */
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

/**
 * log - displays the commit chain history for the current branch
 */
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

/**
 * branch - Creates a new branch pointing to the current commit
 * @name: new branch name
 * @Author: author of the new branch
 */
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

/**
 * checkout - switches to a given commit or branch
 * @targetHash: branch name or commit ID 
 */
        void checkout(const string& targetHash) {
            string resolvedHash = findHash(targetHash);
            if (resolvedHash.empty()) return;
    
            ofstream hStream(repoPath / "HEAD", ios::trunc);
            hStream << targetHash;
            hStream.close();
    
            if (exists(repoPath / "refs" / targetHash)) {
                cout << "Branch switched to " << targetHash << "\n";
            } else {
                cout << "went to commit @" << targetHash << "\n";
            }
    
            recover(resolvedHash);
        }


/**
 * merge - merges another branch into the current branch
 * @branch: name of the branch to merge into the current branch
 */

    void merge(const string& branch) {  
        string currentBranch = activeBranch();  
  
        if (branch == currentBranch) {  
            cerr << "You can't merge a branch onto itself\n";  
            return;  
        }  
  
        path branchPath = repoPath / "refs" / branch;  
        if (!exists(branchPath)) {  
            cerr << "Branch " << branch << " doesn't exist\n";  
            return;  
        }  
  
        string currentCommit = latestCommit();  
        string branchCommit  = latestCommit(branch);  
        string ancestorCommit = commonAncestor(currentCommit, branchCommit);  
  
        if (ancestorCommit.empty()) {  
            cerr << branch << " and " << currentBranch << " have no common ancestor\n";  
            return;  
        }  
  
        auto ancestorMap = stageOf(ancestorCommit);  
        auto currentMap  = stageOf(currentCommit);  
        auto branchMap   = stageOf(branchCommit);  
  
        unordered_set<string> fileCollection;  
        for (const auto* fileMap : { &ancestorMap, &currentMap, &branchMap })  
            for (const auto& [fileName, _] : *fileMap)  
                fileCollection.insert(fileName);  
  
        unordered_map<string, string> mergedStage;  
        vector<string> conflictList;  
  
        for (const string& fileName : fileCollection) {  
            string resolved = manageConflict(ancestorMap[fileName], currentMap[fileName], branchMap[fileName]);  
            if (resolved.empty())  
                conflictList.push_back(fileName);  
            else  
                mergedStage[fileName] = resolved;  
        }  
  
        if (!conflictList.empty()) {  
            cerr << "Conflict occurred in merge - ";  
            for (size_t i = 0; i < conflictList.size(); ++i) {  
                cerr << conflictList[i];  
                if (i < conflictList.size() - 1) cerr << ", ";  
            }  
            cerr << "\n";  
        }  
  
        string stagePath = mergeStage(mergedStage);  
        string stageBlob = blob(stagePath);  
        remove(stagePath);  
  
        CommitNode node;  
        node.stageSnap  = stageBlob;  
        node.prevCommit = currentCommit;  
        node.timestamp  = time();  
        node.author     = Author(currentBranch);  
        node.comment    = "Merged from " + branch;  
  
        string commitText = commitString(node);  
        string commitID   = hashOf(commitText);  
  
        ofstream(repoPath / "commits" / commitID) << commitText;  
        ofstream(repoPath / "refs" / currentBranch) << commitID;  
  
        cout << "Merge Commit @" << commitID << " - " << node.comment << "\n";  
}
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


