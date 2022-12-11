#include "Client.hpp"
#include <iostream>
#include <filesystem>
#include <stdio.h>
#include <fstream>
#include "Core.hpp"

namespace CELV
{
    Client::Client()
        : _running(false)
        , _filesystem()
    {

    }

    void Client::Run()
    {
        std::cout << "Consola CELV iniciada!" << std::endl;
        std::cout << "Escribe `ayuda` para la lista de comandos disponibles" << std::endl;
        std::cout << "Escribe `salir` para terminar esta sesión. Recuerda que los cambios serán descartados al salir" << std::endl;

        _running = true;
        // Parse first word of terminal, as a command
        while (_running)
        {   
            std::cout << "AELV [" << BLUE << _filesystem.GetCurrentWorkingDirectory() << RESET << "] >> ";
            ExecPrompt(std::cin);
            std::cout << std::endl;
        }
    }

    void Client::Run(const std::string& filepath)
    {
        // Check file existence
        if (std::filesystem::exists(filepath))
        {
            std::cerr << "File '" << filepath << "' does not exists" << std::endl;
            return; 
        }

        // Try to read file line by line
        std::fstream file(filepath);
        _running = true;
        while (!file.eof() && _running && (ExecPrompt(file) != ERROR));
        _running = false;
    }

    STATUS Client::ExecPrompt(std::istream&  user_prompt)
    {
        std::string line;
        // Read a single line
        getline(user_prompt, line);
        std::stringstream ss(line);

        std::string command; 
        ss >> command;
        
        if (command == "ayuda")
            Help();
        else if (command == "salir")
        {
            std::cout << "Saliendo del interpretador" << std::endl;
            _running = false;
        }
        else if (command == "crear_dir")
        {
            std::string name;
            if (ss >> name)
                CreateDir(name); 
            else 
                std::cerr << "Missing argument for command: " << command << std::endl;
        }
        else if (command == "crear_archivo")
        {
            std::string name;
            if (ss >> name)
                CreateFile(name); 
            else 
                std::cerr << "Missing argument for command: " << command << std::endl;
        }
        else if (command == "eliminar")
        {
            std::string name;
            if (ss >> name)
                Remove(name); 
            else 
                std::cerr << "Missing argument for command: " << command << std::endl;
        }
        else if (command == "leer")
        {
            std::string name;
            if (ss >> name)
                Read(name); 
            else 
                std::cerr << "Missing argument for command: " << command << std::endl;
        }
        else if (command == "escribir")
        {
            std::string name;
            std::string content;
            ss >> name >> std::ws;
            std::getline(ss, content);

            if (name != "")
                Write(name, content); 
            else 
                std::cerr << "Missing argument for command: " << command << std::endl;
        }
        else if (command == "ir")
        {
            std::string dir_to_go;
            if ((ss >> dir_to_go))
                Go(dir_to_go); 
            else 
                Go();
        }
        else if (command == "celv_importar")
        {
            std::string filepath;
            if (ss >> filepath)
                Import(filepath); 
            else 
                std::cerr << "Missing argument for command: " << command << std::endl;
        }
        else if (command == "celv_iniciar")
        {
            CELVInit(); 
        }
        else if (command == "celv_historia")
        {
            CELVHistory(); 
        }
        else if (command == "celv_vamos")
        {
            Version version;
            if (ss >> version) 
                CELVGo(version); 
            else 
                std::cerr << "Missing argument for command: " << command << std::endl;
        }
        else if (command == "celv_version")
        {
            auto version = _filesystem.GetVersion();
            std::cout << version << std::endl;
        }
        else if (command == "celv_fusion")
        {
            Version version1;
            Version version2;
            if ((ss >> version1) && (ss >> version2)) 
                CELVFusion(version1, version2); 
            else 
                std::cerr << "Missing argument for command: " << command << std::endl;
        }
        else if (command == "ls")
        {
            List();
        }
        else 
        {
            std::cerr << RED << "Invalid command: " << command << RESET << std::endl;
        }
        return SUCCESS;
    }

    void Client::CreateDir(const std::string& filename)
    {
        std::string error;
        if(_filesystem.CreateFile(filename, FileType::DIRECTORY, error) == ERROR)
            std::cerr << RED << error << RESET << std::endl;
    }

    void Client::CreateFile(const std::string& filename)
    {
        std::string error;
        if(_filesystem.CreateFile(filename, FileType::DOCUMENT, error) == ERROR)
            std::cerr << RED << error << RESET << std::endl;
    }

    void Client::Remove(const std::string& filename)
    {
        std::string error;
        if(_filesystem.RemoveFile(filename, error) == ERROR)
            std::cerr << RED << error << RESET << std::endl;
    }

    void Client::Read(const std::string& filename)
    {
        std::string error, content;

        if(_filesystem.ReadFile(filename, content, error) == ERROR)
        {
            std::cerr << RED << error << RESET << std::endl;;
            return;
        }
        
        std::cout << content << std::endl;
    }

    void Client::Write(const std::string& filename, const std::string& content)
    {
        std::string error;
        if(_filesystem.WriteFile(filename, content, error) == ERROR)
            std::cerr << RED << error << RESET << std::endl;
    }

    void Client::Go(const std::string& filename)
    {
        std::string error;
        if(_filesystem.ChangeDirectory(filename, error) == ERROR)
            std::cerr << RED << error << RESET << std::endl;
    }

    void Client::Go()
    {
        std::string error;
        if(_filesystem.ChangeDirectory(error) == ERROR)
            std::cerr << RED << error << RESET << std::endl;
    }

    void Client::List()
    {
        auto const &files = _filesystem.List();
        for (auto const& my_file : files)
        {
            std::cout << (my_file.GetFileType() == FileType::DIRECTORY ? BLUE : GREEN) << my_file.GetName()  << RESET << std::endl;
        }
    }

    void Client::Import(const std::string& local_filepath)
    {
        std::cout << "Function not yet implemented" << std::endl;
    }

    void Client::CELVInit()
    {
        std::cout << "Function not yet implemented" << std::endl;
    }

    void Client::CELVHistory()
    {
        std::cout << "Function not yet implemented" << std::endl;
    }

    void Client::CELVGo(const Version& version)
    {
        std::string error_msg;
        if (_filesystem.SetVersion(version, error_msg) == ERROR)
            std::cerr << RED << error_msg << RESET << std::endl;
    }

    void Client::CELVFusion(const Version& version1, const Version& version2)
    {
        std::cout << "Function not yet implemented" << std::endl;
    }

    void Client::Help()
    {
        std::cout << "Para correr un comando, usa: \n";
        std::cout << "\t<comando> [argumentos]\n";
        std::cout << "Los comandos disponibles son: \n";
        std::cout << "\t- salir : cierra esta terminal\n";
        std::cout << "\t- ayuda : imprime este mensaje\n";
        std::cout << "\t- crear_dir nombre_dir : Crea un directorio con el nombre especificado\n";
        std::cout << "\t- crear_archivo nombre_archivo : Crea un archivo vacío con el nombre especificado\n";
        std::cout << "\t- eliminar nombre_archivo : Elimina el archivo especificado por nombre_archivo. Si es un directorio, elimina recursivamente.\n";
        std::cout << "\t- leer nombre_archivo : Lee el contenido del archivo y lo imprime en la terminal.\n";
        std::cout << "\t- escribir nombre_archivo contenido : Lee el contenido del archivo y lo imprime en la terminal.\n";
        std::cout << "\t- ir nombre_archivo : navega al directorio llamado `nombre_archivo`\n";
        std::cout << "\t- ir : navega al directorio padre del nodo actual\n";
        std::cout << "\t- celv_iniciar : Inicializa control de versiones en el subarbol representado por el directorio actual\n";
        std::cout << "\t- celv_historia : Muestra el historial de cambios para el control de versiones actualmente activo\n";
        std::cout << "\t- celv_vamos version: cambia la version actual a la version especificada\n";
        std::cout << "\t- celv_fusion version1 version2: Trata de fusionar las dos versiones especificadas\n";
        std::cout << "\t- celv_importar camino_directorio: Imita la estructura de archivos del directorio especificado\n";
        std::cout << "\t- celv_version: Retorna la version actualmente activa en el control de versiones\n";
    }
}