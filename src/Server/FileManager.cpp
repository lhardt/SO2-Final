#include "FileManager.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>

const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
const uint64_t FNV_PRIME = 1099511628211ULL;

FileManager::FileManager(std::string base_directory_path) : base_directory(base_directory_path) {
  struct stat info;

  if (stat(base_directory.c_str(), &info) != 0) {
    createDirectory(base_directory);
  }else if (info.st_mode & S_IFDIR) {
    std::cout << "Base directory already exists: " << base_directory
              << std::endl;
  }else {
    throw std::runtime_error("Base directory is not a directory");
  }
}



void FileManager::createDirectory(const std::string &path) {
  if (mkdir(path.c_str(), 0777) != 0) {
    throw std::runtime_error("Failed to create directory: " + path);
  }
}



void FileManager::deleteDirectory(const std::string &path) {
  if (rmdir(path.c_str()) != 0) {
    throw std::runtime_error("Failed to delete directory: " + path);
  }
}



std::string FileManager::getBaseDirectory() { return base_directory; }

std::vector<char> FileManager::readFile(const std::string &file_name) {
  std::string file_path = base_directory + "/" + file_name;
  std::cout << "Reading file: " << file_path << std::endl;
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open file: " + file_path);
  }

  std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
  return buffer;
}



void FileManager::createFile(const std::string &file_name) {
  std::string file_path = base_directory + "/" + file_name;
  std::cout << "Creating file: " << std::endl;
  std::ofstream MyFile(file_path);
  if (!MyFile) {
    throw std::runtime_error("Failed to create the file: " + file_path);
  }
}



void FileManager::clearFile(const std::string &file_name) {
  std::string file_path = base_directory + "/" + file_name;
  std::ofstream ofs;
  ofs.open(file_path, std::ofstream::out | std::ofstream::trunc);
  ofs.close();
}



void FileManager::writeFile(const std::string &file_name, const std::vector<char> &data) {
  std::string file_path = base_directory + "/" + file_name;
  std::cout << "Writing file: " << file_path << std::endl;
  std::ofstream file(file_path, std::ios::binary | std::ios::app);
  if (!file) {
    throw std::runtime_error("Failed to open file: " + file_path);
  }
  file.write(data.data(), data.size());
}

void FileManager::printFile(const std::string &file_name) {
  std::string file_path = base_directory + "/" + file_name;
  std::cout << "Printing file: " << file_path << std::endl;
  std::ifstream file(file_path);
  if (!file) {
    throw std::runtime_error("Failed to open file: " + file_path);
  }
  std::string line;
  while (std::getline(file, line)) {
    std::cout << line << std::endl;
  }
}



void FileManager::deleteFile(const std::string &file_name) {
  std::string file_path = base_directory + "/" + file_name;
  std::cout << "Deleting file: " << file_path << std::endl;
  if (remove(file_path.c_str()) != 0) {
    throw std::runtime_error("Failed to delete file: " + file_path);
  }
}



bool FileManager::isFileExists(const std::string &file_name) {
  std::string file_path = base_directory + "/" + file_name;
  std::ifstream file(file_path);
  return file.good();
}

//2INFO:
//O NOME DO ARQUIVO Q TÁ NO SERVER (NOME -> LE ARQUIVO -> FAZ HASH DO BYTESTREA)
//BYTESTREAM DE ARQUIVO Q RECEBEU DO CLIENTE 
bool FileManager::checkFileHashchanged(std::string &file_name_original,std::string &file_name_copy){
  std::cout<<"comparando arquivos: "<<file_name_original<<" "<<file_name_copy;


  if(hash_file_fnv1a(file_name_original) == hash_file_fnv1a(file_name_copy))
    return false;
  return true;
}

uint64_t FileManager::hash_file_fnv1a(const std::string& filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file) throw std::runtime_error("Erro ao abrir o arquivo: " + filename);

  uint64_t hash = FNV_OFFSET_BASIS;
  char buffer[8192]; // buffer de 8KB

  while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
      std::streamsize bytes = file.gcount();
      for (std::streamsize i = 0; i < bytes; ++i) {
          hash ^= static_cast<uint8_t>(buffer[i]);
          hash *= FNV_PRIME;
      }
  }

  return hash;
}

void FileManager::renameFile(const std::string &file_name,const std::string &new_name){
  std::filesystem::rename(file_name, new_name);
}
