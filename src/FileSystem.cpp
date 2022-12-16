
#include "FileSystem.hpp"
#include "assert.h"
#include <stack>
#include <queue>
#include <algorithm>
#include <sstream>
#include <string>

#include "Core.hpp"
#include "Diff.hpp"

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

        auto const& old_node = (*possible_old_child).second;
        auto const new_node = std::make_shared<FileTree>(new_file_id, old_node->GetParent(), new_version);
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

    FileSystem::FileSystem()
        : _files()
    { 
        _current_version = 0; // initial version
        _next_available_version = 1; // next possible version
        _versions.push_back(std::make_shared<FileTree>(0, nullptr, _current_version)); // create an original version
        _working_dir = _versions[_current_version]; // set working dir as root of only version available
        _files.emplace_back("/", 0); // root dir is /
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

    STATUS FileSystem::RemoveFile(const std::string& filename, std::string& out_error_msg)
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

    STATUS FileSystem::MergeVersions(Version src, Version dst, std::string &out_error_msg)
    {
        if (src == dst)
        {
            out_error_msg = "Versions to merge must differ.";
            return ERROR;
        }

        // Set tracking of version to maximum. By choice
        auto tracking_version = src > dst ? src : dst; 

        // Create new root depending on empty changebox accordingly
        if ( _versions[ tracking_version ]->UseChangeBox(tracking_version))
            _versions.emplace_back(_versions[ tracking_version ]);
        else {
            auto root_copy = std::make_shared<FileTree>(_versions[ tracking_version]->GetFileID(), nullptr, tracking_version);
            _versions.push_back( root_copy );
        }

        // Update working dir to new tracking
        _working_dir = _versions[ tracking_version ] ;

        typedef struct TMP {
            std::shared_ptr<FileTree> l_ver, r_ver, new_ver;
        } version_record;
        // Queue to move over common directories in a bredth first traversal
        std::queue<version_record> next_visit;
        
        next_visit.push( version_record{_versions[src], _versions[dst], _working_dir} );
        while (!next_visit.empty()) 
        {
            const auto &current_comp_dir = next_visit.front();
            next_visit.pop();

            // Set working dir according to newly created dirs
            _working_dir = current_comp_dir.new_ver;

            // Consider files from directory (adjacents)
            auto l_files = current_comp_dir.l_ver->ContainedFiles(src);
            auto r_files = current_comp_dir.r_ver->ContainedFiles(dst);

            // Here sort adyacents of both directories by name alphanumerically
            sort(l_files.begin(), l_files.end(), [&](const auto &a, const auto &b){
                const auto &l_content = _files[ a->GetFileID(src) ];
                const auto &r_content = _files[ b->GetFileID(dst) ];
                
                // If types are the same, order alphanumerically
                if (l_content.GetFileType() == r_content.GetFileType())
                    return l_content.GetName() < r_content.GetName();

                // Otherwise, regular files go first    
                return l_content.GetFileType() == FileType::DOCUMENT;
            });

            sort(r_files.begin(), r_files.end(), [&](const auto &a, const auto &b) {
                const auto &l_content = _files[ a->GetFileID(src) ];
                const auto &r_content = _files[ b->GetFileID(dst) ];
                
                // If types are the same, order alphanumerically
                if (l_content.GetFileType() == r_content.GetFileType())
                    return l_content.GetName() < r_content.GetName();

                // Otherwise, regular files go first    
                return l_content.GetFileType() == FileType::DOCUMENT;
            });

            // Perform "merge" logic
            auto l_it = l_files.begin();
            auto r_it = r_files.begin();
            while (l_it != l_files.end() || r_it != r_files.end()) 
            {
                const auto &l_content  = _files[ (*l_it)->GetFileID(src)];
                const auto &r_content  = _files[ (*r_it)->GetFileID(dst) ];

                // There are still files on both directories to compare 
                if (l_it != l_files.end() && r_it != r_files.end())
                {
                    if (l_content.GetName().compare(r_content.GetName()) == 0)
                    {
                        if (l_content.GetFileType() == r_content.GetFileType())
                        {
                            // If versions are equal then do nothing since one of src, dest represents merge version
                            if ((*l_it)->GetVersion() != (*r_it)->GetVersion())
                            {
                                // If directories, create new and push them to further check files
                                if (l_content.GetFileType() == FileType::DIRECTORY)
                                {
                                    // * DIR creation logic 
                                    CreateFile(l_content.GetName(), FileType::DIRECTORY, out_error_msg);

                                    // Take id from just created directory
                                    auto dir_id = _files[ _files.size() - 1].GetId(); // ! OJO risky

                                    printf("Aqui\n");
                                    next_visit.push( version_record{*l_it, *r_it, _working_dir->GetChilds(tracking_version).at(dir_id)} );

                                }
                                else
                                {
                                    // * Reg creation logic with content merge
                                    // If files create new with edist
                                    std::string A(l_content.GetContent()), B(r_content.GetContent());
                                    DIFF tmp(A, B);
                                    std::string diffed_content = tmp.compute_diff();

                                    CreateFile(l_content.GetName(), FileType::DOCUMENT, out_error_msg);
                                    WriteFile(l_content.GetName(), diffed_content, out_error_msg);
                                }
                            }
                            // Move both
                            ++l_it; ++r_it;
                        }
                        else
                        {
                            // ! OJO
                            // Create file first, move iterator related to file
                            if (l_content.GetFileType() == FileType::DOCUMENT)
                            {
                                // * Regfile creation logic with l_content
                                CreateFile(l_content.GetName(), FileType::DOCUMENT, out_error_msg);
                                WriteFile(l_content.GetName(), l_content.GetContent(), out_error_msg);
                                ++l_it;
                            }
                            else {
                                // * Regfile creation logic with r_content
                                CreateFile(r_content.GetName(), FileType::DOCUMENT, out_error_msg);
                                WriteFile(r_content.GetName(), r_content.GetContent(), out_error_msg);
                                ++r_it;
                            }
                        }
                    }
                    else
                    {
                        // Create smaller one (by name) no matter type, move it related to smaller one
                        if (l_content.GetName().compare(r_content.GetName()) < 0)
                        {
                            // * either DIR or REG creation logic
                            if (l_content.GetFileType() == FileType::DOCUMENT)
                            {
                                CreateFile(l_content.GetName(), FileType::DOCUMENT, out_error_msg);
                                WriteFile(l_content.GetName(), l_content.GetContent(), out_error_msg);
                            }
                            else 
                                CreateFile(l_content.GetName(), FileType::DIRECTORY, out_error_msg);
                            ++l_it;
                        }
                        else
                        {
                            // * either DIR or REG creation logic
                            if (r_content.GetFileType() == FileType::DOCUMENT)
                            {
                                CreateFile(r_content.GetName(), FileType::DOCUMENT, out_error_msg);
                                WriteFile(r_content.GetName(), r_content.GetContent(), out_error_msg);
                            }
                            else 
                                CreateFile(r_content.GetName(), FileType::DIRECTORY, out_error_msg);
                            ++r_it;
                        }
                    } 
                }
                // Only left directory holds files
                else if (l_it != l_files.end())
                {
                    // * either DIR or REG creation logic
                    if (l_content.GetFileType() == FileType::DOCUMENT)
                    {
                        CreateFile(l_content.GetName(), FileType::DOCUMENT, out_error_msg);
                        WriteFile(l_content.GetName(), l_content.GetContent(), out_error_msg);
                    }
                    else 
                        CreateFile(l_content.GetName(), FileType::DIRECTORY, out_error_msg);
                    ++l_it;
                }
                // Only right directory hodls files
                else if (r_it != r_files.end())
                {
                    // * either DIR or REG creation logic
                    if (r_content.GetFileType() == FileType::DOCUMENT)
                    {
                        CreateFile(r_content.GetName(), FileType::DOCUMENT, out_error_msg);
                        WriteFile(r_content.GetName(), r_content.GetContent(), out_error_msg);
                    }
                    else 
                        CreateFile(r_content.GetName(), FileType::DIRECTORY, out_error_msg);
                    ++r_it;
                }
                else break;
            }
        }

        //Register this action
        PushAction(Action
            { 
                ActionType::MERGE,
                {std::to_string(src) + "::" + std::to_string(dst)}, 
                tracking_version, 
                _next_available_version
            });

        return SUCCESS;
    }
}