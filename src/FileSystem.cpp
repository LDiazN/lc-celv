
#include "FileSystem.hpp"
#include "assert.h"
#include <stack>
#include <sstream>
#include <string>
#include "Core.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>

namespace CELV
{

    File::File(const std::string& name, FileID id, const std::string& content)
        : _name(name)
        , _content(content)
        , _type(FileType::DOCUMENT)
        , _id(id)
    { }

    File::File(const std::string& name, FileID id)
        : _name(name)
        , _content("")
        , _type(FileType::DIRECTORY)
        , _id(id)
    { }

    std::string File::GetContent() const
    {
        // Return conent only if type is document
        assert(_type == FileType::DOCUMENT && "Can't get content of folder");

        return _content;
    }

    void File::SetContent(const std::string& new_content)
    {
        assert(_type == FileType::DOCUMENT && "Can't set content of directory");
        _content = new_content;
    }

    std::vector<File> FileTree::_files;

    FileTree::FileTree(FileID id, std::shared_ptr<FileTree> parent,  Version version, std::shared_ptr<CELV> _version_control)
        : _contained_files()
        , _parent(parent)
        , _change_box(nullptr)
        , _file_id(id)
        , _version(version)
        , _celv(_version_control)
    { }

    std::shared_ptr<FileTree> FileTree::MakeRootFileTree()
    {
        assert(_files.empty() && "Can't create filesystem root, already exists");
        _files.push_back(File{"/", 0});

        return std::make_shared<FileTree>(0, nullptr, 0, nullptr);
    }

    File FileTree::GetFileData() const
    {
        if (CELVActive())
            return _celv->GetFiles()[_file_id];

        return _files[_file_id];
    }

    STATUS FileTree::FromLocalFileSystem(const std::string& src_path, std::shared_ptr<FileTree>& out_tree, std::string& out_error_msg, std::vector<File>& files, Version version, std::shared_ptr<CELV> celv)
    {
        
        std::filesystem::path p(src_path);

        // Guarantee that path exists
        if (!std::filesystem::exists(p) || !std::filesystem::is_directory(p))
        {
            std::stringstream ss;
            ss <<"Path to a directory '"<<src_path<<"' does not exists\n";
            out_error_msg = ss.str();
            return ERROR;
        }

        // Root of the entire filesystem
        auto root_file_id = files.size();
        files.emplace_back(p.filename().string(), root_file_id);
        auto overall_root = std::make_shared<FileTree>(root_file_id, nullptr, version, celv);

        // Iterate through whole filesystem subtree rooted at path
        auto it = std::filesystem::directory_iterator(p);
        while (it != end(it))
        {
            
            //Check permissions. Need a way to check I have ownership 
            auto perms = it->status().permissions();
            if ( ( (perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none 
                    && (perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none
                 ) || (
                    (perms & std::filesystem::perms::others_read) != std::filesystem::perms::none 
                    && (perms & std::filesystem::perms::others_write) != std::filesystem::perms::none 
                 )
                )
            {
                if (std::filesystem::is_directory(it->path()))
                {
                    FileID new_id = files.size();
                    files.push_back(File(it->path().filename().string(), new_id));

                    std::shared_ptr<FileTree> child_dir;
                    if (FromLocalFileSystem(it->path().string(), child_dir, out_error_msg, files, version, celv) == ERROR)
                        return ERROR;
                    
                    overall_root->AddFile(child_dir);
                    child_dir->SetParent(child_dir);
                }
                else if (std::filesystem::is_regular_file(it->path()))
                {
                    std::ifstream  input_str(it->path().string());
                    std::stringstream buff;
                    buff << input_str.rdbuf();

                    // Create actual node
                    FileID new_id = files.size();
                    files.push_back(File(it->path().filename().string(), new_id, buff.str()));

                    auto child = std::make_shared<FileTree>(new_id, overall_root, version, celv);
                    overall_root->AddFile(child);
                }
                else 
                    std::cerr<<" Ignoring '"<<it->path().string()<<"'. Not regular file nor directory\n";
            }
            else 
                std::cerr<<" Ignoring '"<<it->path().string()<<"'. Not enough permissions\n";

            //Move to next directory entry
            ++it;
        } 
        out_tree = overall_root;
        return SUCCESS;
    }

    const std::vector<File> FileTree::List() const
    {
        if (CELVActive())
            return _celv->List();
        
        std::vector<File> results;
        for(auto const& [file_id, file_ref] : _contained_files)
            results.emplace_back(_files[file_id]);
        
        return results;
    }

    STATUS FileTree::ChangeDirectory(const std::string& directory_name, std::shared_ptr<FileTree>& out_new_dir, std::string& out_error_msg)
    {
        if (CELVActive())
        {
            auto status = _celv->ChangeDirectory(directory_name, out_error_msg);

            if (status == ERROR)
                return status;

            out_new_dir = _celv->GetCurrentWorkingDirectoryRef();
            return SUCCESS;
        }
        auto const& files = _contained_files;

        // Iterate over files searching for a file matching this name
        for(auto const& [file_id, file] : files)
        {
            const bool name_match = _files[file_id].GetName() == directory_name;

            // If match and is directory...
            if ( name_match && _files[file_id].GetFileType() == FileType::DIRECTORY)
            {
                out_new_dir = file;
                return SUCCESS;
            }
            else if(name_match) // if match but not dir...
            {
                out_error_msg = "Specified file is not a directory";
                return ERROR;
            }
        }

        out_error_msg = "No such file or directory";
        return ERROR;
    }

    STATUS FileTree::ChangeDirectory(std::shared_ptr<FileTree>& out_new_dir, std::string& out_error_msg)
    {
        // Redirect to celv if active
        if (CELVActive() && _celv->GetCurrentWorkingDirectoryRef()->IsRoot())
        {
            assert(_celv->GetParentDir() != nullptr && "parent of celv can't be null");
            auto possible_new_parent = _celv->GetParentDir()->GetParent();
            if (possible_new_parent == nullptr)
            {
                out_error_msg = "Can't go up from root dir";
                return ERROR;
            }

            out_new_dir = possible_new_parent;
            return SUCCESS;
        }
        else if (CELVActive())
        {
            auto status = _celv->ChangeDirectory(out_error_msg);
            if (status == ERROR)
                return status;
            
            out_new_dir = _celv->GetCurrentWorkingDirectoryRef();
            return status;
        }

        // Check if this file is root
        if (IsRoot())
        {
            out_error_msg = "Can't go up from root dir";
            return ERROR;
        }

        out_new_dir = _parent;
        return SUCCESS;
    }

    STATUS FileTree::CreateFile(const std::string& filename, FileType type, std::string& out_error_msg, std::shared_ptr<FileTree> new_parent)
    {
        // Check if CELV if necessary
        if (CELVActive())
            return _celv->CreateFile(filename, type, out_error_msg);

        // Check if any files has this name already
        for(auto const& [file_id, file] : _contained_files)
        {
            if (_files[file_id].GetName() == filename)
            {
                out_error_msg = "File already exists";
                return ERROR;
            }
        }

        // Create new file now that we know we can
        auto const new_file_id = _files.size();
        switch (type)
        {
        case FileType::DOCUMENT:
            _files.emplace_back(filename, new_file_id, "");
            break;
        case FileType::DIRECTORY:
            _files.emplace_back(filename, new_file_id);
            break;
        default:
            break;
        }

        _contained_files[new_file_id] = std::make_shared<FileTree>(new_file_id, new_parent);
        return SUCCESS;
    }

    STATUS FileTree::RemoveFile(const std::string& filename, std::string& out_error_msg)
    {
        if(CELVActive())
            return _celv->RemoveFile(filename, out_error_msg);
        
        for (auto const& [file_id, file_ref] : _contained_files)
        {
            auto const name_match = filename == _files[file_id].GetName();
            if (name_match && _files[file_id].GetFileType() == FileType::DOCUMENT)
            {
                _contained_files.erase(file_id);
                return SUCCESS;
            }
            else if (name_match)
            {
                file_ref->Destroy();
                _contained_files.erase(file_id);
                return SUCCESS;
            }

        }

        out_error_msg = "No such file or directory";
        return ERROR;
    }

    STATUS FileTree::ReadFile(const std::string& filename, std::string& out_content, std::string& out_error_msg) const
    {
        if(CELVActive())
            return _celv->ReadFile(filename, out_content, out_error_msg);
        
        for (auto const& [file_id, file_ref] : _contained_files)
        {
            auto const name_match = filename == _files[file_id].GetName();
            if (name_match && _files[file_id].GetFileType() == FileType::DOCUMENT)
            {
                out_content = _files[file_id].GetContent();
                return SUCCESS;
            }
            else if (name_match)
            {
                out_error_msg = "Can't read content from directory";
                return ERROR;
            }
        }

        out_error_msg = "No such file or directory";
        return ERROR;
    }

    STATUS FileTree::WriteFile(const std::string& filename,const std::string& content, std::string& out_error_msg)
    {
        if (CELVActive())
            return _celv->WriteFile(filename, content, out_error_msg);
        
        for (auto const& [file_id, file_ref] : _contained_files)
        {
            auto const name_match = filename == _files[file_id].GetName();
            if (name_match && _files[file_id].GetFileType() == FileType::DOCUMENT)
            {
                _files[file_id].SetContent(content);
                return SUCCESS;
            }
            else if (name_match)
            {
                out_error_msg = "Can't write content to directory";
                return ERROR;
            }
        }

        out_error_msg = "No such file or directory";
        return ERROR;
    }

    STATUS FileTree::SetVersion(Version version, std::string& out_error_msg)
    {
        if (CELVActive())
            return _celv->SetVersion(version, out_error_msg);
        
        out_error_msg = "CELV not initialized, can't change any version";
        return ERROR;
    }

    STATUS FileTree::GetVersion(Version& out_version, std::string& out_error_msg)
    {
        if (CELVActive())
        {
            out_version = _celv->GetVersion();
            return SUCCESS;
        }
        
        out_error_msg = "CELV not initialized, can't get any version";
        return ERROR;
    }

    STATUS  FileTree::GetHistory(std::vector<Action>& out_history, std::string& out_error_msg)
    {
        if (CELVActive())
        {
            out_history = _celv->GetHistory();
            return SUCCESS;
        }

        out_error_msg = "CELV not initialized, can't retrieve any history";
        return ERROR;
    }

    STATUS FileTree::InitCELV(std::string& out_error_msg, std::shared_ptr<FileTree> celv_parent)
    {
        if (IsCelvInitInSubtree())
        {
            out_error_msg = "Can't init celv in this directory. Already initialized in subdirectory.";            return ERROR;
        }
        
        auto cloned_tree = CloneTree();
        auto const& new_celv = CELV::FromTree(cloned_tree);
        _celv = new_celv;
        _celv->SetParentDir(celv_parent);
        _file_id = _celv->GetCurrentWorkingDirectoryRef()->GetFileID();
        return SUCCESS;
    }

    STATUS FileTree::ImportLocalPath(const std::string& path, std::string& out_error_msg, std::shared_ptr<FileTree> parent)
    {
        if (CELVActive())
        {
            return _celv->ImportLocalPath(path, out_error_msg, _celv);
        }

        std::filesystem::path p(path);
        auto filename = p.filename().string();

        for (auto const& [file_id, file_ref] : _contained_files)
        {
            if (filename == _files[file_id].GetName())
            {
                out_error_msg = "File already exists";
                return ERROR;
            }
        }

        std::shared_ptr<FileTree> new_child;
        if (FromLocalFileSystem(path, new_child, out_error_msg, _files) == ERROR)
            return ERROR;
        new_child->SetParent(parent);
        AddFile(new_child);
        return SUCCESS;
    }

    std::shared_ptr<FileTree> FileTree::AddFile(std::shared_ptr<FileTree> file, Version current_version, Version new_version, std::shared_ptr<FileTree>& out_possible_new_parent)
    {
        ChildMap new_contained(GetChilds(current_version));
        new_contained[file->GetFileID()] = file;
        return UpdateNode(new_contained, current_version, new_version, out_possible_new_parent);
    }

    void FileTree::RemoveFile(FileID file_id)
    {
        _contained_files.erase(file_id);
    }

    std::shared_ptr<FileTree> FileTree::RemoveFile(FileID file_id, Version current_version, Version new_version, std::shared_ptr<FileTree>& out_possible_new_parent)
    {
        // Create a new map with corresponding childs
        auto const& old_childs = GetChilds(current_version);
        if (old_childs.find(file_id) == old_childs.end())
            return nullptr; // If node does not contains the specified file, do nothing

        ChildMap new_childs(old_childs);

        // Erase corresponding node
        new_childs.erase(file_id);

        // Update node by deleting this
        return UpdateNode(new_childs, current_version, new_version, out_possible_new_parent);
    }

    std::shared_ptr<FileTree> FileTree::ReplaceFileId(FileID old_file_id, FileID new_file_id, Version current_version, Version new_version, std::shared_ptr<FileTree>& out_possible_new_parent)
    {
        auto const& old_childs = GetChilds(current_version);
        auto const& possible_old_child = old_childs.find(old_file_id);

        if (old_childs.find(old_file_id) == old_childs.end())
            return nullptr; // Nothing to do if nothing to replace

        ChildMap new_childs(old_childs);
        new_childs.erase(old_file_id);

        auto const& old_node = possible_old_child->second;
        auto const new_node = std::make_shared<FileTree>(new_file_id, old_node->GetParent(), new_version, _celv);
        new_childs[new_file_id] = new_node;

        return UpdateNode(new_childs, current_version, new_version, out_possible_new_parent);
    }

    std::vector<std::shared_ptr<FileTree>> FileTree::ContainedFiles(Version version) const
    {
        auto const& contained_files = UseChangeBox(version) ? _change_box->_contained_files :  _contained_files;

        std::vector<std::shared_ptr<FileTree>> files(contained_files.size());
        size_t i = 0;
        for (auto const& [id, file_ptr] : contained_files)
            files[i++] = file_ptr;
        
        return files;
    }

    bool FileTree::ContainsFile(FileID id)
    {
        return _contained_files.find(id) != _contained_files.end();
    }

    void FileTree::Destroy()
    {
        // Set every pointer to null recursively
        _parent = nullptr;
        for (auto &[k, child] : _contained_files)
        {
            child->Destroy();
            _contained_files[k] = nullptr; // break all references to this child
        }

        if (_change_box != nullptr)
        {
            _change_box->Destroy();
            _change_box = nullptr; // break reference tu change_box
        }
    }

    std::shared_ptr<FileTree> FileTree::UpdateNode(FileID new_file_id, Version current_version, Version new_version, std::shared_ptr<FileTree>& out_new_version_parent)
    {
        // If changebox is empty, update it and and return nothing
        if (_change_box == nullptr)
        {
            _change_box = std::make_shared<FileTree>(new_file_id, _parent, new_version, _celv);
            _change_box->SetNewChilds(_contained_files);
            return nullptr;
        }

        // If changebox if full, we need to create a new node
        auto new_node = std::make_shared<FileTree>(new_file_id, nullptr, new_version, _celv);
        // Update parent for this node
        if (_parent == nullptr)
        {   
            // If this node is root, then new created node is this versions's root
            out_new_version_parent = new_node;
            return new_node;
        }

        auto parent_childs(_parent->GetChilds(current_version));
        parent_childs.erase(_file_id);
        parent_childs[new_file_id] = new_node;
        auto possible_new_parent = _parent->UpdateNode(parent_childs, current_version,new_version, out_new_version_parent);

        if (possible_new_parent == nullptr)
            new_node->SetParent(_parent);
        else 
            new_node->SetParent(possible_new_parent);

        // Update childs of this node as childs of change box, the newest version 
        new_node->SetNewChilds(_change_box->GetChilds(current_version));

        return new_node;
    }
        
    std::shared_ptr<FileTree> FileTree::UpdateNode(const ChildMap& new_contained_files, Version current_version, Version new_version, std::shared_ptr<FileTree>& out_new_version_parent)
    {
        // If changebox is empty, update it and and return nothing
        if (_change_box == nullptr)
        {
            _change_box = std::make_shared<FileTree>(_file_id, _parent, new_version, _celv);
            _change_box->SetNewChilds(new_contained_files);
            return nullptr;
        }

        // If changebox if full, we need to create a new node
        auto new_node = std::make_shared<FileTree>(_file_id, nullptr, new_version, _celv);
        new_node->SetNewChilds(new_contained_files);

        // Update parent for this node
        if (IsRoot())
        {   
            // If this node is root, then new created node is this versions's root
            out_new_version_parent = new_node;
            return new_node;
        }

        auto parent_childs(_parent->GetChilds(current_version));
        parent_childs[_file_id] = new_node;
        auto possible_new_parent = _parent->UpdateNode(parent_childs, current_version, new_version, out_new_version_parent);

        if (possible_new_parent == nullptr)
            new_node->SetParent(_parent);
        else 
            new_node->SetParent(possible_new_parent);

        return new_node;
    }

    bool FileTree::IsCelvInitInSubtree() const
    {
        if (CELVActive())
            return true;

        for (auto const& [file_id, file_ref] : _contained_files)
            if (file_ref->IsCelvInitInSubtree())
                return true;
        
        return false;
    }

    std::shared_ptr<FileTree> FileTree::CloneTree() const
    {
        auto const new_root = std::make_shared<FileTree>(_file_id, nullptr, _version, _celv);
        ChildMap new_child_map;

        for (auto const& [file_id, file_ref] : _contained_files)
        {
            auto const new_tree = file_ref->CloneTree();
            new_child_map[file_id] = new_tree;
            new_tree->SetParent(new_root);
        }

        new_root->SetNewChilds(new_child_map);
        return new_root;
    }

    std::string Action::Str() const
    {
        std::stringstream ss;
        ss  << MAGENTA << "[ " << origin_version << BLUE << " -> " << MAGENTA << new_version << " ]" << std::endl;
        ss << "\t" << YELLOW;
        switch (type)
        {
        case ActionType::WRITE :
            ss << "escribir ";
            break;
        case ActionType::CREATE_DIR: 
            ss << "crear_dir";
            break;
        case ActionType::CREATE_DOC:
            ss << "crear_archivo";
            break;
        case ActionType::REMOVE:
            ss << "eliminar";
            break;
        case ActionType::MERGE:
            ss << "celv_fusion";
            break;
        case ActionType::IMPORT:
            ss << "celv_importar";
        default:
            break;
        }

        ss << RESET << " ";

        for(auto const& arg : args)
        {
            if (arg.size() <= 23)
            {
                ss << arg << " ";
                continue;
            }

                ss << arg.substr(0, 10);
                ss << "...";
                ss << arg.substr(arg.size() - 10);
        }
        return ss.str();
    }

    CELV::CELV()
        : _files()
    { 
        _current_version = 0; // initial version
        _next_available_version = 1; // next possible version
        _versions.push_back(std::make_shared<FileTree>(0, nullptr, _current_version)); // create an original version
        _working_dir = _versions[_current_version]; // set working dir as root of only version available
        _files.emplace_back("/", 0); // root dir is /
    }

    std::shared_ptr<CELV> CELV::FromTree(std::shared_ptr<FileTree> file_tree)
    { 
        auto celv = std::make_shared<CELV>();

        file_tree->SetParent(nullptr);
        celv->_current_version = 0; // initial version
        celv->_next_available_version = 1; // next possible version
        celv->_versions = {file_tree}; // create an original version
        celv->_working_dir = celv->_versions[celv->_current_version]; // set working dir as root of only version available
        
        // Traverse given tree updating their values
        std::stack<std::shared_ptr<FileTree>> file_trees;
        std::map<FileID, FileID> old_to_new;
        file_trees.emplace(celv->_working_dir);

        while(!file_trees.empty())
        {
            auto next_tree = file_trees.top();
            file_trees.pop();
            next_tree->_version = 0;
            next_tree->_celv = celv; // oh no

            auto const old_id = next_tree->GetFileID();
            FileID new_id;
            auto const possible_find = old_to_new.find(old_id);
            if (possible_find == old_to_new.end())
            {
                new_id = celv->_files.size();
                celv->_files.emplace_back(FileTree::_files[old_id]);
                old_to_new[old_id] = new_id;
            }
            else
                new_id = possible_find->second;

            next_tree->_file_id = new_id;
            // Traverse over its children
            for(auto [file_id, file_ref] : next_tree->_contained_files)
                file_trees.push(file_ref);
        }

        // Now update left side of ChildMaps
        file_trees.push(celv->_working_dir);
        while(!file_trees.empty())
        {
            auto next_tree = file_trees.top();
            file_trees.pop();
            FileTree::ChildMap new_contained;
            for (auto const&[old_file_id, file_ref] : next_tree->_contained_files)
            {
                auto new_id = old_to_new[old_file_id];
                new_contained[new_id] = file_ref;
                file_trees.push(file_ref);
            }
            next_tree->SetNewChilds(new_contained);
        }

        return celv;
    }

    const std::vector<File> CELV::List() const
    {
        assert(_working_dir != nullptr && "File tree is not initialized");
        auto const contained_files = _working_dir->ContainedFiles(_current_version);
        std::vector<File> files;
        for (auto const& file_tree : contained_files)
        {
            auto const file_id = file_tree->GetFileID();
            files.emplace_back(_files[file_id]);
        }

        return files;
    }

    std::string CELV::GetCurrentWorkingDirectory() const
    {
        assert(_working_dir != nullptr && "File tree is not initialized");
        auto dir_id =  _working_dir->GetFileID(_current_version);
        return _files[dir_id].GetName();
    }

    STATUS CELV::ChangeDirectory(const std::string& directory_name, std::string& out_error_msg)
    {
        auto const& files = _working_dir->ContainedFiles(_current_version);

        // Iterate over files searching for a file matching this name
        for(auto const& file : files)
        {
            auto const& file_id = file->GetFileID();
            const bool name_match = _files[file_id].GetName() == directory_name;

            // If match and is directory...
            if ( name_match && _files[file_id].GetFileType() == FileType::DIRECTORY)
            {
                _working_dir = file;
                return SUCCESS;
            }
            else if(name_match) // if match but not dir...
            {
                out_error_msg = "Specified file is not a directory";
                return ERROR;
            }
        }

        // if couldn't find dir...
        out_error_msg = "No such file or directory";
        return ERROR;
    }

    STATUS CELV::ChangeDirectory(std::string& out_error_msg)
    {
        if (_working_dir->GetParent() == nullptr)
        {
            out_error_msg = "Can't go up from filesystem root";
            return ERROR;
        }

        std::string s;
        assert(SetVersion(_current_version, s, 1) == SUCCESS);
        return SUCCESS;
    }

    STATUS CELV::CreateFile(const std::string& filename, FileType type, std::string& out_error_msg)
    {

        // Check if any files has this name already
        for(auto const& file : _working_dir->ContainedFiles(_current_version))
        {
            auto const file_id = file->GetFileID();
            if (_files[file_id].GetName() == filename)
            {
                out_error_msg = "File already exists";
                return ERROR;
            }
        }

        // Add file according to type
        auto const new_file_id = _files.size();
        switch (type)
        {
        case FileType::DOCUMENT:
            _files.emplace_back(filename, new_file_id, "");
            break;
        case FileType::DIRECTORY:
            _files.emplace_back(filename, new_file_id);
            break;
        default:
            assert(false && "Invalid type of file");
            break;
        }

        // Add file to current directory
        // Note that adding a new file means that the version root might be new 
        // and that a new version of current working dir could be created
        std::shared_ptr<FileTree> possible_new_version_parent = nullptr;
        auto possible_new_node = _working_dir->AddFile(std::make_shared<FileTree>(new_file_id, _working_dir, _next_available_version, _working_dir->_celv), _current_version, _next_available_version, possible_new_version_parent);

        // Update version root
        if (possible_new_version_parent != nullptr) // if a new root is created, added to version control
            _versions.push_back(possible_new_version_parent);
        else // if no new version of root is created, repeat root
            _versions.push_back(_versions[_current_version]);

        // If created a new node version for our working directory, update current working directory
        if (possible_new_node != nullptr)
            _working_dir = possible_new_node;

        
        //Register this action
        PushAction(Action
            { 
                type == FileType::DOCUMENT ? ActionType::CREATE_DOC : ActionType::CREATE_DIR, 
                {filename}, 
                _current_version, 
                _next_available_version
            });

        _current_version = _next_available_version++;
        
        return SUCCESS;
    }

    STATUS CELV::RemoveFile(const std::string& filename, std::string& out_error_msg)
    {
        for(auto const& file : _working_dir->ContainedFiles(_current_version))
        {
            auto const file_id = file->GetFileID();
            if (_files[file_id].GetName() == filename)
            {

                std::shared_ptr<FileTree> possible_new_version_parent = nullptr;
                auto possible_new_node = _working_dir->RemoveFile(file_id, _current_version, _next_available_version, possible_new_version_parent);

                if (possible_new_version_parent != nullptr)
                    _versions.push_back(possible_new_version_parent);
                else 
                    _versions.push_back(_versions[_current_version]);

                if (possible_new_node != nullptr)
                    _working_dir = possible_new_node;

                //Register this action
                PushAction(Action{ActionType::REMOVE, {filename}, _current_version, _next_available_version});

                _current_version = _next_available_version++;
                return SUCCESS;
            }
        }

        out_error_msg = "No such file or directory";
        return ERROR;
    }

    STATUS CELV::ReadFile(const std::string& filename, std::string& out_content, std::string& out_error_msg) const
    {
        for(auto const& file : _working_dir->ContainedFiles(_current_version))
        {
            auto const file_id = file->GetFileID();
            bool name_match = _files[file_id].GetName() == filename;
            if (name_match && _files[file_id].GetFileType() == FileType::DOCUMENT)
            {
                out_content = _files[file_id].GetContent();
                return SUCCESS;
            }
            else if (name_match)
            {
                out_error_msg = "File is not a document, can't read directories";
                return ERROR;
            }
        }

        out_error_msg = "No such file or directory";
        return ERROR;
    }

    STATUS CELV::WriteFile(const std::string& filename, const std::string& content, std::string& out_error_msg)
    {
        for(auto const& file : _working_dir->ContainedFiles(_current_version))
        {
            auto const file_id = file->GetFileID();
            bool name_match = _files[file_id].GetName() == filename;
            if (name_match && _files[file_id].GetFileType() == FileType::DOCUMENT)
            {
                auto const& file = _files[file_id];
                auto const new_file_id = _files.size();
                _files.emplace_back(file.GetName(), new_file_id, content);

                std::shared_ptr<FileTree> possible_new_parent = nullptr;
                auto const possible_new_cwd = _working_dir->ReplaceFileId(file_id, new_file_id, _current_version, _next_available_version, possible_new_parent);

                if (possible_new_parent != nullptr)
                    _versions.push_back(possible_new_parent);
                else
                    _versions.push_back(_versions[_current_version]);
                
                if (possible_new_cwd != nullptr)
                    _working_dir = possible_new_cwd;

                //Register this action
                PushAction(Action{ActionType::WRITE, {filename, content}, _current_version, _next_available_version});

                _current_version = _next_available_version++;

                return SUCCESS;
            }
            else if (name_match)
            {
                out_error_msg = "File is not a document, can't write on directories";
                return ERROR;
            }
        }

        out_error_msg = "No such file or directory";
        return ERROR;
    }

    STATUS CELV::ImportLocalPath(const std::string& path, std::string& out_error_msg, std::shared_ptr<CELV> celv)
    {
        std::filesystem::path p(path);
        auto filename = p.filename().string();

        for(auto const&[file_id, file_ref] : _working_dir->GetChilds(_current_version))
        {
            auto child_name = _files[file_id].GetName();
            if (filename == child_name)
            {
                out_error_msg = "File already exists";
                return ERROR;
            }
        }

        std::shared_ptr<FileTree> new_node;
        if (FileTree::FromLocalFileSystem(path, new_node, out_error_msg, _files, _next_available_version, celv) == ERROR)
            return ERROR;
        
        new_node ->SetParent(_working_dir);

        // Add file to current directory
        // Note that adding a new file means that the version root might be new 
        // and that a new version of current working dir could be created
        std::shared_ptr<FileTree> possible_new_version_parent = nullptr;
        auto possible_new_node = _working_dir->AddFile(new_node, _current_version, _next_available_version, possible_new_version_parent);

        // Update version root
        if (possible_new_version_parent != nullptr) // if a new root is created, added to version control
            _versions.push_back(possible_new_version_parent);
        else // if no new version of root is created, repeat root
            _versions.push_back(_versions[_current_version]);

        // If created a new node version for our working directory, update current working directory
        if (possible_new_node != nullptr)
            _working_dir = possible_new_node;

        
        //Register this action
        PushAction(Action
            { 
                ActionType::IMPORT, 
                {path}, 
                _current_version, 
                _next_available_version
            });

        _current_version = _next_available_version++;
        
        return SUCCESS;
    }

    STATUS CELV::SetVersion(Version version, std::string& out_error_msg, size_t skip_in_stack)
    {
        if (version >= _next_available_version) // raise error if requesting a version too high
        {
            out_error_msg = "Invalid version";
            return ERROR;
        }

        // When changing versions, we first need to check if the working directory is one that 
        // exists in that version.
        // We traverse the filesystem tree up to the root to get the path required to go down again.
        std::stack<FileID> path_to_cwd;
        auto next_dir = _working_dir;
        while (!next_dir->IsRoot())
        {
            path_to_cwd.push(next_dir->GetFileID());
            next_dir = next_dir->GetParent();
        }

        _current_version = version;
        next_dir = _versions[version];
        while(path_to_cwd.size() > skip_in_stack)
        {
            // Get id of next folder to advance
            auto next_dir_id = path_to_cwd.top();
            path_to_cwd.pop();
            
            auto const &childs = next_dir->GetChilds(version);
            // If this folder does not contains the directory we're looking for, 
            // we end this
            auto possible_dir = childs.find(next_dir_id);
            if (possible_dir == childs.end())
                break;
            
            // If we found it, set the next dir as this dir
            next_dir = (*possible_dir).second;
        }

        _working_dir = next_dir;
        return SUCCESS;
    }

    void CELV::Destroy()
    {
        for (auto &version : _versions)
            version->Destroy();
        _versions.clear();
        _files.clear();
        _history.clear();
        _working_dir = nullptr;
    }

    FileSystem::FileSystem()
    {
        _file_tree = FileTree::MakeRootFileTree();
        _working_directory = _file_tree;
    }

    std::vector<File> FileSystem::List() const
    {
        return _working_directory->List();
    }

    std::string  FileSystem::GetCurrentWorkingDirectory() const
    {
        return _working_directory->GetFileData().GetName();
    }

    STATUS FileSystem::ChangeDirectory(const std::string& directory_name, std::string& out_error_msg)
    {
        std::shared_ptr<FileTree> new_cwd;
        auto status = _working_directory->ChangeDirectory(directory_name, new_cwd, out_error_msg);
        if (status == ERROR)
            return status;

        _working_directory = new_cwd;
        return SUCCESS;
    }

    STATUS FileSystem::ChangeDirectory(std::string& out_error_msg)
    {
        std::shared_ptr<FileTree> new_cwd;
        auto status = _working_directory->ChangeDirectory(new_cwd, out_error_msg);
        if (status == ERROR)
            return status;

        _working_directory = new_cwd;
        return SUCCESS;
    }

    STATUS FileSystem::CreateFile(const std::string& filename, FileType type, std::string& out_error_msg)
    {
        return _working_directory->CreateFile(filename, type, out_error_msg, _working_directory);
    }

    STATUS FileSystem::RemoveFile(const std::string& filename, std::string& out_error_msg)
    {
        return _working_directory->RemoveFile(filename, out_error_msg);
    }

    STATUS FileSystem::ReadFile(const std::string& filename, std::string& out_content, std::string& out_error_msg) const
    {
        return _working_directory->ReadFile(filename, out_content, out_error_msg);
    }

    STATUS FileSystem::WriteFile(const std::string& filename,const std::string& content, std::string& out_error_msg)
    {
        return _working_directory->WriteFile(filename, content, out_error_msg);
    }

    STATUS FileSystem::SetVersion(Version version, std::string& out_error_msg)
    {
        return _working_directory->SetVersion(version, out_error_msg);
    }

    STATUS FileSystem::GetVersion(Version& out_version, std::string& out_error_msg) const
    {
        return _working_directory->GetVersion(out_version, out_error_msg);
    }

    STATUS FileSystem::GetHistory(std::vector<Action>& out_history, std::string& error_msg)
    {
        return _working_directory->GetHistory(out_history, error_msg);
    }
    void FileSystem::Destroy()
    {
        _working_directory = nullptr;
        _file_tree->Destroy();
        _file_tree = nullptr;
    }
}