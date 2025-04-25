#include <sys/inotify.h>
#include <unistd.h>
#include <iostream>
#include <string>

class DirectoryWatcher {
    
private:
    static constexpr const char* sync_dir = "sync_dir";
    int inotifyFd;
    int watchDescriptor;

public:
    DirectoryWatcher()
        : inotifyFd(-1), watchDescriptor(-1) {}

    ~DirectoryWatcher() {
        if (watchDescriptor >= 0) inotify_rm_watch(inotifyFd, watchDescriptor);
        if (inotifyFd >= 0) close(inotifyFd);
    }

    void watch() {
        inotifyFd = inotify_init1(IN_NONBLOCK);
        if (inotifyFd < 0) {
            perror("inotify_init1");
            return;
        }

        watchDescriptor = inotify_add_watch(inotifyFd, sync_dir, IN_CREATE | IN_MODIFY | IN_DELETE);
        if (watchDescriptor < 0) {
            perror("inotify_add_watch");
            return;
        }

        constexpr size_t BUF_LEN = 1024 * (sizeof(inotify_event) + 16);
        char buffer[BUF_LEN];

        std::cout << "Monitorando a pasta: " << sync_dir << std::endl;

        while (true) {
            int length = read(inotifyFd, buffer, BUF_LEN);
            if (length < 0) {
                usleep(100000); // Evita busy waiting
                continue;
            }

            int i = 0;
            while (i < length) {
                struct inotify_event* event = (struct inotify_event*)&buffer[i];

                if (event->len > 0) {
                    std::string filename = "./" + sync_dir + "/" + event->name;

                    if (event->mask & IN_CREATE) {
                        on_file_added(filename);
                    }
                    if (event->mask & IN_MODIFY) {
                        on_file_modified(filename);
                    }
                    if (event->mask & IN_DELETE) {
                        on_file_deleted(filename);
                    }
                }

                i += sizeof(struct inotify_event) + event->len;
            }
        }
    }

private:
    template<typename T = void>
    void on_file_added(const std::string& filename) {
        std::cout << "[INFO] Arquivo adicionado: " << filename << std::endl;
    }

    template<typename T = void>
    void on_file_modified(const std::string& filename) {
        std::cout << "[INFO] Arquivo modificado: " << filename << std::endl;
    }

    template<typename T = void>
    void on_file_deleted(const std::string& filename) {
        std::cout << "[INFO] Arquivo removido: " << filename << std::endl;
    }
};
