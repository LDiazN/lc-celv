/*
 * 
 */

#include<stdio.h>
#include<iostream> 
#include<vector>
#include<stack> 
#include<queue> 
#include<algorithm> 

#include<memory>
#include "Colors.hpp"
#include<random>
#include<chrono>
#include<sstream>
#include<filesystem>
#include<fstream>

using namespace std; 

namespace fs = filesystem;

//GC

auto seed = std::chrono::system_clock::now().time_since_epoch().count();
mt19937 gen(seed);

#define PROMPT "SIM-P>"

#define INDENT "  "

#define MAXDEPTH 3

#define STATUS int
#define ERR 0
#define SUCC 1

using Version = size_t;

enum FileType { DIR, REG };

struct File {
public:
    // create reg file
    File(FileType type, const string & name, const string & cont) : _name(name), _type(type) {
        switch (type) {
            case DIR:
                break;
            case REG:
                setContent(cont);
        }
    };

    STATUS setContent(const string &cont) 
    { 
        if ( getFType() == DIR )
        {
            cout<<RED<<"Cannot set directory contents.\n"<<RESET;
            return ERR;
        }
        _content = cont; 

        return SUCC;
    }

    string getContent() const { 
        if ( getFType() == DIR )
        {
            cerr<<RED<<"Cannot read from dir.\n"<<RESET;
            return "";
        }
        cout<<"Reading "<<BOLDCYAN<<getFName()<<RESET<<"..\n";
        return _content; 
    }
    string getFName() const { return _name; }
    FileType getFType() const { return _type; }

    void displayFName() const 
    { 
        switch (getFType()) 
        {
            case DIR:
                cout<<BOLDBLUE<<getFName()<<RESET;
            break;
            case REG:
                cout<<getFName();
            break;
        }
    }
private:
    string _content, _name;
    FileType _type;
};

class FSNode {
public:
    FSNode(shared_ptr<FSNode> parent, FileType type, const string &name, const string &content) 
        : _parent(parent)
        , _F(type, name, content)
    {
        _version = gen() % 42;
    };



    shared_ptr<FSNode> getParent() { return _parent; }
    File getFile() const { return _F; }
    vector<shared_ptr<FSNode>> getAdyacents() const { return _adyacents; }
    void addAdyacent(shared_ptr<FSNode> parent, FileType type, const string &name, const string &content) {
        _adyacents.push_back(make_shared<FSNode>(parent, type, name, content));
    }
    Version getVersion() const { return _version; }

    // call display from file
    void displayNode() const {
        getFile().displayFName();
        cout<<"."<<BOLDMAGENTA<<getVersion()<<RESET;
    }; 
private:
    
    shared_ptr<FSNode> _parent;
    vector<shared_ptr<FSNode>> _adyacents;
    File _F; 
    Version _version;
};

class FS {
public:
    FS () : _root( make_shared<FSNode>(nullptr, DIR, "/", "") ) 
    {
        _CWD = _root;
    }

    // cd dirname
    STATUS simpleMove(const string &dest) {

        for (const auto & el : getCWD()->getAdyacents())
            if (el->getFile().getFName() == dest && el->getFile().getFType() == DIR ) 
            {
                setCWD(el);
                return SUCC;
            }


        cerr<<RED<<"There's no directory "<<dest<<" within "<<getCWD()->getFile().getFName()<<RESET<<'\n';
        return ERR;
    }

    // cd ..  
    STATUS simpleMove() {
        auto tmp = getCWD()->getParent();
        if ( tmp != nullptr) 
        {
            setCWD(tmp);
            return SUCC;
        }

        cerr<<RED<<"Already on root."<<RESET<<'\n';
        return ERR;
    }

    // Only if directory
    STATUS addNode(FileType type, const string &filename, const string &content) {
        switch ( getCWD()->getFile().getFType() ) 
        {
            case DIR:
                for (const auto &el : getCWD()->getAdyacents())
                    if (el->getFile().getFName().compare(filename) == 0 && el->getFile().getFType() == type)
                    {
                        cerr<<RED<<"File already exists"<<RESET<<'\n';
                        return ERR;
                    }
                getCWD()->addAdyacent( getCWD(), type, filename, content );
                break;
            case REG:
                cerr<<RED<<"Cannot add file to regular file"<<RESET<<'\n';
                return ERR;
                break;
        }

        return SUCC;
    };

    STATUS mkdir(const string &name) 
    {
        return addNode(DIR, name, ""); 
    }
    STATUS mkreg(const string &name, const string &content)
    {
        return addNode(REG, name, content);
    }

    shared_ptr<FSNode> getRoot() const { return _root ; }
    shared_ptr<FSNode> getCWD() const { return _CWD ;}
    void setCWD(shared_ptr<FSNode> new_CWD) { _CWD = new_CWD; }

    void ls () 
    {
        for (const auto & adyacent : getCWD()->getAdyacents())
        {
            adyacent->displayNode() ;
            cout<<INDENT;
        }
        cout<<'\n';

    }

    STATUS tree(shared_ptr<FSNode> curr_dir , int depth = 0) 
    {

        indent(depth);
        curr_dir->displayNode(); // ! OJO
        printf("\n");

        for (const auto adyacent : curr_dir->getAdyacents()) // ! OJO
        {
            STATUS tmp;
                tmp = tree(adyacent, depth + 1);

            if (tmp = ERR)
                return ERR;
        }

        return SUCC;
    }

    void indent(int repetitions) const {
        for (int i=0 ; i<repetitions; ++i)
            printf("%s",INDENT);
    }

    STATUS fromLocalFilesystem(const string src_path )
    {
        fs::path p(src_path);

        // Guarantee that path exists
        if (!fs::exists(p) || !fs::is_directory(p))
        {
            cout<<RED<<"Path to a directory '"<<src_path<<"' does not exists"<<RESET<<'\n';
            return ERR;
        }

        // Iterate through whole filesystem subtree rooted at path
        auto it = fs::recursive_directory_iterator(p);
        while (it != end(it))
        {
            
            //Check permissions. Need a way to check I have ownership 
            auto perms = it->status().permissions();
            if ( ( (perms & fs::perms::owner_read) != fs::perms::none 
                    && (perms & fs::perms::owner_write) != fs::perms::none
                 ) || (
                    (perms & fs::perms::others_read) != fs::perms::none 
                    && (perms & fs::perms::others_write) != fs::perms::none 
                 )
                )
            {
                if (fs::is_directory(it->path()) || fs::is_regular_file(it->path()))
                    createPath(getCWD(), it->path()); // ! OJO
                else 
                    cerr<<" Ignoring '"<<it->path().string()<<"'. Not regular file nor directory\n";
            }
            else 
                cerr<<" Ignoring '"<<it->path().string()<<"'. Not enough permissions\n";

            //Move to next directory entry
            ++it;
        } 
        return SUCC;

    }

    void createPath(shared_ptr<FSNode> root, fs::path p) {

        auto curr_node = root;
        // P is a relative path in the unix sense
        auto pp = p.parent_path();
        auto it = pp.begin();
        // remove root
        it++;

        // Create each directory on path
        while (it != pp.end())
        {
            mkdir(*it); // !OJO
            // Move to it
            simpleMove(*it); // ! OJO
            it++;
        }
        // Creation logic
        if (fs::is_directory(p))
            mkdir(p.filename()); // ! OJO
        else 
            {
                cout<<"Creating file: "<<p.filename()<<'\n';
                // Create regular file on current directory
                string filename(p.filename());
                ifstream ifs(filename, ifstream::in);

                // Read file char by char
                string filecontent;
                while (ifs.good())
                    filecontent.push_back( ifs.get());

                // Create file
                mkreg(filename, filecontent); // ! OJO
                ifs.close();
            }
        
        // Reset CWD
        setCWD(root); // ! OJO
    }

private: 
    shared_ptr<FSNode> _root;
    shared_ptr<FSNode> _CWD;
};

int main() { 
    FS *globalFS = new FS();

    cout << "Greetings, fool around with";
    cout << "\n\tcd";
    cout << "\n\tcd arg";
    cout << "\n\tmkdir arg";
    cout << "\n\ttouch arg1 arg2";
    cout << "\n\ttree" ;
    cout << "\n\tls" ;
    cout << "\n\textend arg" ;
    cout << "\n\texit" << endl;

    // Pseudo client
    while (true) 
    {
        cout<<PROMPT<<" ";
        string line;
        getline(cin, line);

        stringstream ss(line);
        string words;
        getline(ss,words,' ');

        if (words.compare("ls") == 0)
            globalFS->ls();
        else if (words.compare("cd") == 0)
        {
            words.clear();
            getline(ss, words, ' ');
            if (words.empty())
                globalFS->simpleMove();
            else   
                globalFS->simpleMove(words);

        }
        else if (words.compare("mkdir") == 0)
        {
            string name;
            ss>>name;

            if (name.empty())
                cerr<<RED<<"Error, directory name required"<<RESET<<'\n';
            else
                globalFS->mkdir(name);

        }
        else if (words.compare("touch") == 0)
        {
            string name, content;
            ss>>name>>content;
            if (name.empty() || content.empty())
                cerr<<RED<<"Error, filename and content required"<<RESET<<'\n';
            else
                globalFS->mkreg(name,content);

        }
        else if (words.compare("tree") == 0)
        {
            globalFS->tree(globalFS->getRoot());
        }
        else if (words.compare("extend") == 0)
        {
            words.clear();
            getline(ss, words, ' ');
            globalFS->fromLocalFilesystem(words);
        }
        else if (words.compare("exit") == 0)
            break;
        else
        {
            cerr<<RED<<words<<RESET<<" is not a valid known command.\n";
        }

    }

    return SUCC;
}

// TODO cd ..,ls, cd arg, mkdir arg, touch arg1 arg2, tree