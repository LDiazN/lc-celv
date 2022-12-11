
#include "FileSystem.hpp"
#include "assert.h"
#include <stack>

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

    FileTree::FileTree(FileID id, std::shared_ptr<FileTree> parent,  Version version)
        : _contained_files()
        , _parent(parent)
        , _change_box(nullptr)
        , _file_id(id)
        , _version(version)
    { }

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

    std::shared_ptr<FileTree> FileTree::UpdateNode(FileID new_file_id, Version current_version, Version new_version, std::shared_ptr<FileTree>& out_new_version_parent)
    {
        // If changebox is empty, update it and and return nothing
        if (_change_box == nullptr)
        {
            _change_box = std::make_shared<FileTree>(new_file_id, _parent, new_version);
            _change_box->SetNewChilds(_contained_files);
            return nullptr;
        }

        // If changebox if full, we need to create a new node
        auto new_node = std::make_shared<FileTree>(new_file_id, nullptr, new_version);
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
            _change_box = std::make_shared<FileTree>(_file_id, _parent, new_version);
            _change_box->SetNewChilds(new_contained_files);
            return nullptr;
        }

        // If changebox if full, we need to create a new node
        auto new_node = std::make_shared<FileTree>(_file_id, nullptr, new_version);
        new_node->SetNewChilds(new_contained_files);

        // Update parent for this node
        if (_parent == nullptr)
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

    FileSystem::FileSystem()
        : _files()
    { 
        _current_version = 0;
        _next_available_version = 1;
        _versions.push_back(std::make_shared<FileTree>(0, nullptr, _current_version));
        _working_dir = _versions[_current_version];
        _files.emplace_back("/", 0);
    }

    const std::vector<File> FileSystem::List() const
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

    std::string FileSystem::GetCurrentWorkingDirectory() const
    {
        assert(_working_dir != nullptr && "File tree is not initialized");
        auto dir_id =  _working_dir->GetFileID(_current_version);
        return _files[dir_id].GetName();
    }

    STATUS FileSystem::ChangeDirectory(const std::string& directory_name, std::string& out_error_msg)
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

    STATUS FileSystem::ChangeDirectory(std::string& out_error_msg)
    {
        if (_working_dir->GetParent() == nullptr)
        {
            out_error_msg = "Can't go up from filesystem root";
            return ERROR;
        }

        _working_dir = _working_dir->GetParent();
        return SUCCESS;
    }

    STATUS FileSystem::CreateFile(const std::string& filename, FileType type, std::string& out_error_msg)
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
        auto possible_new_node = _working_dir->AddFile(std::make_shared<FileTree>(new_file_id, _working_dir), _current_version, _next_available_version, possible_new_version_parent);

        // Update version root
        if (possible_new_version_parent != nullptr) // if a new root is created, added to version control
            _versions.push_back(possible_new_version_parent);
        else // if no new version of root is created, repeat root
            _versions.push_back(_versions[_current_version]);

        // If created a new node version for our working directory, update current working directory
        if (possible_new_node != nullptr)
            _working_dir = possible_new_node;

        _current_version = _next_available_version++;
        
        return SUCCESS;
    }

    STATUS FileSystem::RemoveFile(const std::string& filename, std::string& out_error_msg)
    {
        for(auto const& file : _working_dir->ContainedFiles(_current_version))
        {
            auto const file_id = file->GetFileID();
            if (_files[file_id].GetName() == filename)
            {
                _working_dir->RemoveFile(file_id);
                return SUCCESS;
            }
        }

        out_error_msg = "No such file or directory";
        return ERROR;
    }

    STATUS FileSystem::ReadFile(const std::string& filename, std::string& out_content, std::string& out_error_msg) const
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

    STATUS FileSystem::WriteFile(const std::string& filename, const std::string& content, std::string& out_error_msg)
    {
        for(auto const& file : _working_dir->ContainedFiles(_current_version))
        {
            auto const file_id = file->GetFileID();
            bool name_match = _files[file_id].GetName() == filename;
            if (name_match && _files[file_id].GetFileType() == FileType::DOCUMENT)
            {
                _files[file_id].SetContent(content);
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

    STATUS FileSystem::SetVersion(Version version, std::string& out_error_msg)
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
        while(!path_to_cwd.empty())
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
}