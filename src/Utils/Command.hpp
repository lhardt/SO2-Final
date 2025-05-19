#pragma once
#include "FileManager.hpp"
#include "NetworkManager.hpp"

class Command {
public:
  virtual void execute() = 0;
  void undo();
};

class SendFileCommand : public Command {
private:
  NetworkManager *networkManager;
  FileManager *fileManager;
  std::string &file_path;

public:
  SendFileCommand(std::string &file_path, NetworkManager *networkManager, FileManager *fileManager);
  void undo();
  void execute();
};

class SendMessageCommand : public Command {
private:
  NetworkManager *networkManager;
  std::string msg;

public:
  SendMessageCommand(std::string msg, NetworkManager *networkManager);
  void undo();
  void execute();
};

class LocalDeleteCommand : public Command {
private:
  FileManager *fileManager;
  std::string &file_path;

public:
  LocalDeleteCommand(std::string &file_path, FileManager *fileManager);
  void undo();
  void execute();
};

class NetworkDeleteCommand : public Command {
private:
  FileManager *fileManager;
  NetworkManager *networkManager;
  std::string &file_path;

public:
  NetworkDeleteCommand(std::string &file_path, NetworkManager *networkManager, FileManager *fileManager);
  void undo();
  void execute();
};

class ReceiveFileCommand : public Command {
private:
  NetworkManager *networkManager;
  FileManager *fileManager;
  std::string &file_name;

public:
  ReceiveFileCommand(std::string &file_name, NetworkManager *networkManager, FileManager *fileManager);
  void undo();
  void execute();
};

class ReceiveMessageCommand {

private:
  NetworkManager *networkManager;
  std::string message;

public:
  ReceiveMessageCommand(NetworkManager *networkManager);
  std::string getMessage();
  void undo();
  void execute();
};
