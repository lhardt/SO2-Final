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

  std::vector<char> readFile(const std::string &file_name);
  void writeFile(const std::string &file_name, const std::vector<char> &data); // sempre adiciona no final do arquivo
  void writeFileTo(const std::string &file_path, const std::vector<char> &data);
  void createFile(const std::string &file_name);
  void clearFile(const std::string &file_name);
  void printFile(const std::string &file_name);
  void deleteFile(const std::string &file_name);
  bool isFileExists(const std::string &file_name);
};
