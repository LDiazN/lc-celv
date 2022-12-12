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
    using Version = size_t;
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

        /// @brief Set content to the specified new content
        /// @param new_content content to add
        void SetContent(const std::string& new_content);

        private:
        std::string _name;
        std::string _content; // Empty when file type is directory
        FileType _type;
        FileID _id;
    };

    class FileTree
    {
        public:
        // typedef ChildMap = ...
        using ChildMap = std::map<FileID, std::shared_ptr<FileTree>>;

        public:
        /// @brief Create a new FileTree
        /// @param id id of this file
        /// @param parent parent file
        FileTree(FileID id, std::shared_ptr<FileTree> parent, Version version = 0);

        /// @brief Get parent of this tree
        /// @return pointer to parent
        std::shared_ptr<FileTree> GetParent() const { return _parent; }

        /// @brief Set new parent of this node
        /// @param new_parent new parent to set
        void SetParent(std::shared_ptr<FileTree> new_parent) { _parent = new_parent; }

        /// @brief Try to add this file as child of this file tree. Note that this function doesn't checks if file is dir or doc, 
        /// you have to ensure it yourself
        /// @param file file to add as child
        /// @param current_version Current version to use, used to query which data to use next
        /// @param new_version New version to mark in any newly modified node
        /// @param out_new_possible_parent New version parent if new root was created. Null if no new root is created.
        /// @return new node if one was created during the update
        std::shared_ptr<FileTree> AddFile(std::shared_ptr<FileTree> file, Version current_version, Version new_version, std::shared_ptr<FileTree>& out_possible_new_parent);

        /// @brief Delete specified file from this node
        /// @param file_id id of file to delete
        void RemoveFile(FileID file_id);
        
        /// @brief Delete specified file from this node
        /// @param file_id id of file to delete
        /// @param current_version currently active version
        /// @param new_version new version to mark for new nodes
        /// @param out_new_possible_parent New version parent if new root was created. Null if no new root is created.
        /// @return new node if one was created during the update
        std::shared_ptr<FileTree> RemoveFile(FileID file_id, Version current_version, Version new_version, std::shared_ptr<FileTree>& out_possible_new_parent);

        /// @brief replace a file with id `old_file_id` with a new file with id `new_file_id`
        /// @param old_file_id id of old file to be replaced
        /// @param new_file_id new id replacing old id
        /// @param current_version currently active version
        /// @param new_version new version to mark in each newly created node
        /// @param out_possible_new_parent possible new root parent if one was created
        /// @return possible new version of this node if one was created
        std::shared_ptr<FileTree> ReplaceFileId(FileID old_file_id, FileID new_file_id, Version current_version, Version new_version, std::shared_ptr<FileTree>& out_possible_new_parent);

        /// @brief Return list of contained files
        /// @return files contained by this node
        std::vector<std::shared_ptr<FileTree>> ContainedFiles(Version version) const;

        /// @brief Checks if this file node contains the file specified by `id`
        /// @param id id of file to check if exists
        /// @return if this file tree contains the required file
        bool ContainsFile(FileID id);

        /// @brief Get if of file refered by this node
        /// @return id of file refered by this node
        FileID GetFileID() const { return _file_id; }

        /// @brief Get if of file refered by this node
        /// @param version Query version
        /// @return id of file refered by this node
        FileID GetFileID(Version version) const { return UseChangeBox(version) ? _change_box->GetFileID() : _file_id; }

        /// @brief Get how many children has this folder of the file tree
        /// @return amount of childs in first level of this file
        size_t GetNChilds() const { return _contained_files.size(); }

        /// @brief Get version of this node
        /// @return version of this node
        Version GetVersion() const { return _version; }

        /// @brief Set new childs of this file
        /// @param childs new childs to updatre
        void SetNewChilds(const ChildMap& childs) { _contained_files = childs; }

        /// @brief Get reference to childs of this node
        /// @return childs contained by this node
        const ChildMap& GetChilds(Version version ) const 
        { return UseChangeBox(version) ? _change_box->GetChilds(version) : _contained_files ; }

        /// @brief If this node is a root node
        /// @return true if this node is root, false otherwise
        bool IsRoot() const { return _parent == nullptr; }

        /// @brief Utility function to check if should use changebox data instead of regular data
        /// @param version active version
        /// @return true if should use change box, false otherwise
        bool UseChangeBox(Version version) const { return _change_box != nullptr && _change_box->GetVersion() <= version; }

        private:
        /// @brief Update file_id of this node. Return new node if new was created
        /// @param new_file_id updated file id
        /// @param current_version Current version to use, used to query which data to use next
        /// @param new_version New version to mark in any newly modified node
        /// @return nullptr if no new node was created, ptr to newly created node otherwise
        std::shared_ptr<FileTree> UpdateNode(FileID new_file_id, Version current_version, Version new_version, std::shared_ptr<FileTree>& out_new_version_parent); // escribir

        /// @brief Update list of files of this node. Return new node if new was created
        /// @param new_contained_files new list of files for this node
        /// @param current_version Current version to use, used to query which data to use next
        /// @param new_version New version to mark in any newly modified node
        /// @return nullptr if no new node was created, ptr to newly created node otherwise
        std::shared_ptr<FileTree> UpdateNode(const ChildMap& new_contained_files,Version current_version, Version new_version, std::shared_ptr<FileTree>& out_new_version_parent); // operacion de directorio

        private:
        ChildMap _contained_files;
        std::shared_ptr<FileTree> _parent;
        std::shared_ptr<FileTree> _change_box;
        FileID _file_id; // id of file containing actual data
        Version _version;
        
    };

    class FileSystem
    {
        public:
        FileSystem();

        /// @brief List files in current directory
        /// @return List of files in current directory
        const std::vector<File> List() const;

        std::string GetCurrentWorkingDirectory() const;

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

        /// @brief Try to change version to the specified version
        /// @param version Version to change to
        /// @param out_error_msg error message in case of error
        /// @return Success status
        STATUS SetVersion(Version version, std::string& out_error_msg);

        /// @brief Get currently active version
        /// @return currently active version
        Version GetVersion() const { return _current_version; }

        private:
        std::vector<File> _files;
        std::shared_ptr<FileTree> _working_dir;
        // Array of version roots
        std::vector<std::shared_ptr<FileTree>> _versions;
        Version _current_version;
        Version _next_available_version;
    };
}

#endif