#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP
#include <vector>
#include <string>
#include <memory>
#include "Core.hpp"
#include <map>

namespace CELV
{

    /// @brief Possible file types. Could be a document, a text only file, or a directory,
    /// a group of files
    enum class FileType
    {
        DOCUMENT,
        DIRECTORY
    };

    using FileID = size_t;
    class File
    {
        public:

        /// @brief Create a document with the specified content
        /// @param name name of file
        /// @param FileId if for this file
        /// @param content content to write into the document
        File(const std::string& name, FileID id, const std::string& content);

        /// @brief Create a folder with the specified name
        /// @param name name of new folder
        /// @param id id for this folder
        File(const std::string& name, FileID id);

        std::string GetName() const { return _name; }
        FileType GetFileType() const { return _type; }
        FileID GetId() const { return _id; }

        /// @brief Try to get content of this file. Raise an error if type is dir
        /// @return Content of file as string
        std::string GetContent() const;

        private:
        std::string _name;
        std::string _content; // Empty when file type is directory
        FileType _type;
        FileID _id;
    };

    class FileTree
    {
        public:

        /// @brief Create a new FileTree
        /// @param id id of this file
        /// @param parent parent file
        FileTree(FileID id, std::shared_ptr<FileTree> parent);

        /// @brief Get parent of this tree
        /// @return pointer to parent
        std::shared_ptr<FileTree> GetParent() const { return _parent; }

        /// @brief Try to add this file as child of this file tree. Note that this function doesn't checks if file is dir or doc, 
        /// you have to ensure it yourself
        /// @param file file to add as child
        void AddFile(std::shared_ptr<FileTree> file);

        /// @brief Return list of contained files
        /// @return files contained by this node
        std::vector<std::shared_ptr<FileTree>> ContainedFiles() const;

        /// @brief Checks if this file node contains the file specified by `id`
        /// @param id id of file to check if exists
        /// @return if this file tree contains the required file
        bool ContainsFile(FileID id);

        private:
        std::map<FileID, std::shared_ptr<FileTree>> _contained_files;
        std::shared_ptr<FileTree> _parent;
        FileID _file_id; // id of file containing actual data
    };

    class FileSystem
    {
        public:
        FileSystem();

        /// @brief List files in current directory
        /// @return List of files in current directory
        const std::vector<File>& List() const;

        /// @brief Try to change directory to a directory named `directory_name`. If not such directory, return error  
        /// @param directory_name Name of directory to change to
        /// @param out_error_msg Error message
        /// @return Success Status
        STATUS ChangeDirectory(const std::string& directory_name, std::string& out_error_msg);

        /// @brief Try to change directory to parent directory. Raise error if already in root directory. 
        /// @param out_error_msg error message if some error happened
        /// @return Success Status
        STATUS ChangeDirectory(std::string& out_error_msg);

        /// @brief Try to create a directory named `directory_name`. Report error if not possible and store message in `out_error_msg`
        /// @param filename Name of new directory to create
        /// @param type If directory or document
        /// @param out_error_msg Resulting error message when not possible
        /// @return Success Status
        STATUS CreateFile(const std::string& filename, FileType type, std::string& out_error_msg);

        /// @brief Try to remove specified file. If directory, perform recursive delete
        /// @param filename name of file to delete
        /// @param out_error_msg error message if not possible
        /// @return Success status
        STATUS RemoveFile(const std::string& filename, std::string& out_error_msg);

        /// @brief Try to read content of file `filename` to string `out_content`. Return error if not possible 
        /// @param filename name of file to read in current directory
        /// @param out_content where to return content of file
        /// @param out_error_msg error msg if not possible to read
        /// @return Status success
        STATUS ReadFile(const std::string& filename, std::string& out_content, std::string& out_error_msg) const;

        /// @brief Try to write `content` into a file named `filename`. Return error if not possible
        /// @param filename name of file to write
        /// @param content content to write into the file
        /// @param out_error_msg error msg if not possible
        /// @return Success status
        STATUS WriteFile(const std::string& filename,const std::string& content, std::string& out_error_msg);

        private:
        std::vector<File> _files;
        FileTree _file_tree;
        std::shared_ptr<FileTree> _working_dir;
    };
}

#endif