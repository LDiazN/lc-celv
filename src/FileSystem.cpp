
#include "FileSystem.hpp"
#include "assert.h"

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

    FileTree::FileTree(FileID id, std::shared_ptr<FileTree> parent)
        : _contained_files()
        , _parent(parent)
        , _file_id(id)
    { }

    void FileTree::AddFile(std::shared_ptr<FileTree> file)
    {
        _contained_files[file->GetFileID()] = file;
    }

    void FileTree::RemoveFile(FileID file_id)
    {
        _contained_files.erase(file_id);
    }

    std::vector<std::shared_ptr<FileTree>> FileTree::ContainedFiles() const
    {
        std::vector<std::shared_ptr<FileTree>> files(_contained_files.size());
        size_t i = 0;
        for (auto const& [id,file_ptr] :_contained_files)
            files[i++] = file_ptr;
        
        return files;
    }

    bool FileTree::ContainsFile(FileID id)
    {
        return _contained_files.find(id) != _contained_files.end();
    }

    FileSystem::FileSystem()
        : _files()
    { 
        _file_tree = std::make_shared<FileTree>(0, nullptr);
        _working_dir = _file_tree;
        _files.emplace_back("/", 0);
    }

    const std::vector<File> FileSystem::List() const
    {
        assert(_working_dir != nullptr && "File tree is not initialized");
        auto const contained_files = _working_dir->ContainedFiles();
        std::vector<File> files;
        for (auto const& file_tree : contained_files)
        {
            auto const file_id = file_tree->GetFileID();
            files.emplace_back(_files[file_id]);
        }

        return files;
    }

    STATUS FileSystem::ChangeDirectory(const std::string& directory_name, std::string& out_error_msg)
    {
        auto const& files = _working_dir->ContainedFiles();

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
        for(auto const& file : _working_dir->ContainedFiles())
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

        _working_dir->AddFile(std::make_shared<FileTree>(new_file_id, _working_dir));
        
        return SUCCESS;
    }

    STATUS FileSystem::RemoveFile(const std::string& filename, std::string& out_error_msg)
    {
        for(auto const& file : _working_dir->ContainedFiles())
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
        for(auto const& file : _working_dir->ContainedFiles())
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

    STATUS FileSystem::WriteFile(const std::string& filename,const std::string& content, std::string& out_error_msg)
    {
        for(auto const& file : _working_dir->ContainedFiles())
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

}