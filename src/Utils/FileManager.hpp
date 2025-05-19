#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace fs = std::filesystem;
class FileManager {
private:
  std::string base_directory;
  uint64_t hash_file_fnv1a(const std::string &filename);

public:
  FileManager(std::string base_directory_path);

  void createDirectory(const std::string &path);
  void deleteDirectory(const std::string &path);
  std::string getBaseDirectory();

  std::vector<char> readFile(const std::string &file_name);
  void writeFile(const std::string &file_name, const std::vector<char> &data); // sempre adiciona no final do arquivo
  void createFile(const std::string &file_name);
  void clearFile(const std::string &file_name);
  void printFile(const std::string &file_name);
  void deleteFile(const std::string &file_name);
  bool isFileExists(const std::string &file_name);
  bool checkFileHashchanged(std::string &file_name_original, std::string &file_name_copy);
  void renameFile(const std::string &file_name, const std::string &new_name);
  std::string getFileModificationTime(const std::string &file_name);
  std::string getFiles();
  size_t getFileSize(const std::string &file_name);
};
