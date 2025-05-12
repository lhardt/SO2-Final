#pragma once
#include <string>
#include <vector>

class FileManager {
private:
  std::string base_directory;

public:
  FileManager(std::string base_directory_path);

  void createDirectory(const std::string &path);
  void deleteDirectory(const std::string &path);
  std::string getBaseDirectory();

  std::vector<char> readFile(const std::string &file_path);
  void writeFile(const std::string &file_path, const std::vector<char> &data);
  void printFile(const std::string &file_path);
  void deleteFile(const std::string &file_path);
};
