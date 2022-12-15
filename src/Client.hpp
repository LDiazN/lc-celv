#ifndef CLIENT_HPP
#define CLIENT_HPP
#include <string>
#include <fstream>
#include "Core.hpp"
#include "FileSystem.hpp"

namespace CELV
{
    class Client
    {
        public:
            Client();

            ~Client() { _filesystem.Destroy(); }

            // -- < Filesystem API > ------------------------------------------------------------------------------------------

            /// @brief Try to create a directory and report error if not possible
            /// @param filename  name of directory to create
            void CreateDir(const std::string& filename);

            /// @brief Try to create an empty file with the specified name. Report error if not possible
            /// @param filename name of file to create
            void CreateFile(const std::string& filename);

            /// @brief Try to delete file specified by `filename`. Report error if not possible
            /// @param filename name of file to delete
            void Remove(const std::string& filename);

            /// @brief Try to read content of file specified by name, and print it to terminal. Report error if not possible.
            /// @param filename name of file to read
            void Read(const std::string& filename);

            /// @brief Try to write the specified `content` to the name specified by `filename`. Report error if not possible.
            /// @param filename 
            /// @param content 
            void Write(const std::string& filename, const std::string& content);

            /// @brief try to go to the directory specified by `filename`. Report error if not possible.
            /// @param filename name of dir to go
            void Go(const std::string& filename);

            /// @brief Try to go to the parent directory of the current directory. Report error if not possible.
            void Go();

            /// @brief List content of current working directory
            void List();

            /// @brief Import the directory structure specified by `local_filepath` and mirror it in memory, only considers dirs and files. 
            /// Report error if not possible.
            /// @param local_filepath file path in the actual disk to mirror
            void Import(const std::string& local_filepath);

            // -- < CELV Version control API > ---------------------------------------------------------------------------------------------
            
            /// @brief Try to init a version control system in the current node.  Report error if not possible.
            void CELVInit();

            /// @brief Try to print history of modifications of some control version system if any.
            void CELVHistory();

            /// @brief Modify the filesystem to go to the specified version. Report error if version does not exists.
            /// @param version Version to go to
            void CELVGo(const Version& version);

            /// @brief Try to fuse together two versions. Print conflicts if not possible
            /// @param version1 Version 1 to fuse
            /// @param version2 Version 2 to fuse
            void CELVFusion(const Version& version1, const Version& version2);

            void CELVVersion() const;

            // -- < Client logic > ---------------------------------------------------------------------------------------------------------
            
            /// @brief Execute main loop
            void Run();

            /// @brief Execute commands from a file specified by `filepath`
            /// @param filepath name of file to open to read commands from
            void Run(const std::string& filepath);

        private:

            /// @brief Print available commands
            static void Help();

            /// @brief Parse and execute the command specified by the user. Report errors if necessary
            /// @param user_prompt command provided by user, from terminal or from file
            STATUS ExecPrompt(std::istream& user_prompt);

        private:
            bool _running;
            FileSystem _filesystem;


    };
}

#endif