#include "FileManager.hpp"
#include "logger.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
const uint64_t FNV_PRIME = 1099511628211ULL;
namespace fs = std::filesystem;

FileManager::FileManager(std::string base_directory_path) : base_directory(base_directory_path) {
  struct stat info;

  if (stat(base_directory.c_str(), &info) != 0) {
    createDirectory(base_directory);
  } else if (info.st_mode & S_IFDIR) {
    log_info("Diretório base já existe: %s", base_directory.c_str());
  } else {
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

// Para debug
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
  if (!isFileExists(file_name)) {
    return;
  }
  if (remove(file_path.c_str()) != 0) {
    throw std::runtime_error("Falha ao deletar arquivo: " + file_path);
  }
}

bool FileManager::isFileExists(const std::string &file_name) {
  std::string file_path = base_directory + "/" + file_name;
  std::ifstream file(file_path);
  return file.good();
}

bool FileManager::checkFileHashchanged(std::string &file_name_original, std::string &file_name_copy) {
  log_info("Comparando Hash dos arquivos: %s %s", file_name_original.c_str(), file_name_copy.c_str());

  if (hash_file_fnv1a(file_name_original) == hash_file_fnv1a(file_name_copy))
    return false;
  return true;
}

uint64_t FileManager::hash_file_fnv1a(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file)
    throw std::runtime_error("Erro ao abrir o arquivo: " + filename);

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
  std::ostringstream result;

  for (const auto &entry : fs::directory_iterator(base_directory)) {
    if (fs::is_regular_file(entry)) {
      auto path = entry.path();
      std::string file_name = path.filename().string();

      // Modification time (mtime)
      auto ftime = fs::last_write_time(entry);
      auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
          ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
      std::time_t mtime = std::chrono::system_clock::to_time_t(sctp);

      struct stat file_stat;
      if (stat(path.c_str(), &file_stat) == 0) {
        std::time_t atime = file_stat.st_atime; // Access time
        std::time_t ctime = file_stat.st_ctime; // Change time

        result << file_name
               << " (mtime: " << std::put_time(std::localtime(&mtime), "%Y-%m-%d %H:%M:%S")
               << ", atime: " << std::put_time(std::localtime(&atime), "%Y-%m-%d %H:%M:%S")
               << ", ctime: " << std::put_time(std::localtime(&ctime), "%Y-%m-%d %H:%M:%S")
               << ")\n";
      } else {
        result << file_name << " (erro ao obter stat)\n";
      }
    }
  }

  return result.str();
}

void FileManager::renameFile(const std::string &file_name, const std::string &new_name) {
  std::filesystem::rename(file_name, new_name);
}

std::string FileManager::getFileModificationTime(const std::string &file_name) {
  fs::path file_path = base_directory + "/" + file_name;
  if (!fs::exists(file_path)) {
    return "File does not exist";
  }

  auto ftime = fs::last_write_time(file_path);
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
  std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);

  std::ostringstream oss;
  oss << std::put_time(std::localtime(&cftime), "%Y-%m-%d %H:%M:%S");
  return oss.str();
}
