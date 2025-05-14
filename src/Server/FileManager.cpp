#include "FileManager.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

FileManager::FileManager(std::string base_directory_path)
    : base_directory(base_directory_path) {
  struct stat info;
  if (stat(base_directory.c_str(), &info) != 0) {
    createDirectory(base_directory);
  } else if (info.st_mode & S_IFDIR) {
    std::cout << "Base directory already exists: " << base_directory
              << std::endl;
  } else {
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
void FileManager::writeFileTo(const std::string &file_path, const std::vector<char> &data) {
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
