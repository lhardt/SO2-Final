#include "FileManager.hpp"
#include "logger.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>

const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
const uint64_t FNV_PRIME = 1099511628211ULL;
namespace fs = std::filesystem;

FileManager::FileManager(std::string base_directory_path) : base_directory(base_directory_path) {
  struct stat info;

  if (stat(base_directory.c_str(), &info) != 0) {
    createDirectory(base_directory);
  }else if (info.st_mode & S_IFDIR) {
    log_info("Diretório base já existe: %s", base_directory.c_str());
  }else {
    throw std::runtime_error("Base directory is not a directory");
  }
}



void FileManager::createDirectory(const std::string &path) {
  if (mkdir(path.c_str(), 0777) != 0) {
    throw std::runtime_error("Falha ao criar diretório " + path);
  }
}



void FileManager::deleteDirectory(const std::string &path) {
  if (rmdir(path.c_str()) != 0) {
    throw std::runtime_error("Falha ao deletar diretório: " + path);
  }
}



std::string FileManager::getBaseDirectory() { return base_directory; }

std::vector<char> FileManager::readFile(const std::string &file_name) {
  std::string file_path = base_directory + "/" + file_name;
  log_info("Lendo arquivo: %s", file_path.c_str());
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Falha ao abrir o arquivo: " + file_path);
  }

  std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
  return buffer;
}



void FileManager::createFile(const std::string &file_name) {
  std::string file_path = base_directory + "/" + file_name;
  log_info("Criando arquivo: %s", file_path.c_str());
  std::ofstream MyFile(file_path);
  if (!MyFile) {
    throw std::runtime_error("Falha ao criar arquivo: " + file_path);
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
  log_info("Escrevendo arquivo: %s", file_path.c_str());
  std::ofstream file(file_path, std::ios::binary | std::ios::app);
  if (!file) {
    throw std::runtime_error("Falha ao abrir arquivo: " + file_path);
  }
  file.write(data.data(), data.size());
}

//Para debug
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
  log_info("Deletando arquivo: %s", file_path.c_str());
  if (remove(file_path.c_str()) != 0) {
    throw std::runtime_error("Falha ao deletar arquivo: " + file_path);
  }
}

bool FileManager::isFileExists(const std::string &file_name) {
  std::string file_path = base_directory + "/" + file_name;
  std::ifstream file(file_path);
  return file.good();
}

bool FileManager::checkFileHashchanged(std::string &file_name_original,std::string &file_name_copy){
  log_info("Comparando Hash dos arquivos: %s %s", file_name_original.c_str(), file_name_copy.c_str());

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

std::string FileManager::getFiles() {
  std::ostringstream oss;

  try {
    for (const auto &entry : fs::directory_iterator(this->base_directory)) {
      if (entry.is_regular_file()) {
        oss << entry.path().filename().string() << " ";
      }
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "Erro: " << e.what() << std::endl;
  }

  return oss.str();
}

void FileManager::renameFile(const std::string &file_name,const std::string &new_name){
  std::filesystem::rename(file_name, new_name);
}
