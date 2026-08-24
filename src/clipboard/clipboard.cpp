#include "clipboard.hpp"
#include <iostream>
#include <cstdlib>
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace webclip {

class WindowsClipboard : public IClipboard {
public:
    std::string get_text() override {
        if (!OpenClipboard(nullptr)) {
            return "";
        }
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData == nullptr) {
            CloseClipboard();
            return "";
        }
        const wchar_t* pText = static_cast<const wchar_t*>(GlobalLock(hData));
        if (pText == nullptr) {
            CloseClipboard();
            return "";
        }

        std::string result;
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, pText, -1, nullptr, 0, nullptr, nullptr);
        if (size_needed > 1) {
            result.resize(size_needed - 1);
            WideCharToMultiByte(CP_UTF8, 0, pText, -1, &result[0], size_needed, nullptr, nullptr);
        }

        GlobalUnlock(hData);
        CloseClipboard();
        return result;
    }

    bool set_text(const std::string& text) override {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (wlen <= 0) return false;

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
        if (!hMem) return false;

        wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
        if (!pMem) {
            GlobalFree(hMem);
            return false;
        }
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pMem, wlen);
        GlobalUnlock(hMem);

        if (!OpenClipboard(nullptr)) {
            GlobalFree(hMem);
            return false;
        }

        EmptyClipboard();
        if (SetClipboardData(CF_UNICODETEXT, hMem) == nullptr) {
            GlobalFree(hMem);
            CloseClipboard();
            return false;
        }

        CloseClipboard();
        return true;
    }

    bool has_image() override {
        UINT pngFormat = RegisterClipboardFormatW(L"PNG");
        return IsClipboardFormatAvailable(pngFormat) || IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5);
    }

    ClipboardImage get_image() override {
        ClipboardImage img;
        if (!OpenClipboard(nullptr)) {
            return img;
        }

        UINT pngFormat = RegisterClipboardFormatW(L"PNG");
        if (IsClipboardFormatAvailable(pngFormat)) {
            HANDLE hData = GetClipboardData(pngFormat);
            if (hData) {
                size_t size = GlobalSize(hData);
                const uint8_t* pData = static_cast<const uint8_t*>(GlobalLock(hData));
                if (pData && size > 0) {
                    img.data.assign(pData, pData + size);
                    img.mime_type = "image/png";
                    img.valid = true;
                    GlobalUnlock(hData);
                }
            }
        }

        CloseClipboard();
        return img;
    }

    bool set_image(const std::vector<uint8_t>& data, const std::string& mime_type) override {
        if (data.empty()) return false;
        if (!OpenClipboard(nullptr)) return false;

        EmptyClipboard();
        UINT pngFormat = RegisterClipboardFormatW(L"PNG");

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, data.size());
        if (!hMem) {
            CloseClipboard();
            return false;
        }

        void* pMem = GlobalLock(hMem);
        if (!pMem) {
            GlobalFree(hMem);
            CloseClipboard();
            return false;
        }

        std::memcpy(pMem, data.data(), data.size());
        GlobalUnlock(hMem);

        if (SetClipboardData(pngFormat, hMem) == nullptr) {
            GlobalFree(hMem);
            CloseClipboard();
            return false;
        }

        CloseClipboard();
        return true;
    }

    std::string get_backend_name() const override {
        return "Windows Win32 (Unicode & PNG)";
    }
};

std::unique_ptr<IClipboard> create_clipboard() {
    return std::make_unique<WindowsClipboard>();
}

} // namespace webclip

#else // Linux / Unix

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

namespace webclip {

class LinuxClipboard : public IClipboard {
public:
    enum class Backend { Wayland, X11 };

    LinuxClipboard() {
        const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
        if (wayland_display != nullptr && wayland_display[0] != '\0') {
            backend_ = Backend::Wayland;
        } else {
            backend_ = Backend::X11;
        }
    }

    std::string get_backend_name() const override {
        return backend_ == Backend::Wayland ? "Linux (Wayland: wl-clipboard)" : "Linux (X11: xclip/xsel)";
    }

    std::string get_text() override {
        if (backend_ == Backend::Wayland) {
            std::string out;
            if (run_command_read({"wl-paste", "-n"}, out)) {
                return out;
            }
            return "";
        } else {
            std::string out;
            if (run_command_read({"xclip", "-selection", "clipboard", "-o"}, out)) {
                return out;
            }
            if (run_command_read({"xsel", "--clipboard", "--output"}, out)) {
                return out;
            }
            return "";
        }
    }

    bool set_text(const std::string& text) override {
        if (backend_ == Backend::Wayland) {
            return run_command_write({"wl-copy"}, text);
        } else {
            if (run_command_write({"xclip", "-selection", "clipboard"}, text)) {
                return true;
            }
            return run_command_write({"xsel", "--clipboard", "--input"}, text);
        }
    }

    bool has_image() override {
        if (backend_ == Backend::Wayland) {
            std::string types;
            if (run_command_read({"wl-paste", "--list-types"}, types)) {
                return types.find("image/") != std::string::npos;
            }
            return false;
        } else {
            std::string targets;
            if (run_command_read({"xclip", "-selection", "clipboard", "-t", "TARGETS", "-o"}, targets)) {
                return targets.find("image/") != std::string::npos || targets.find("PNG") != std::string::npos;
            }
            return false;
        }
    }

    ClipboardImage get_image() override {
        ClipboardImage img;
        std::vector<uint8_t> bytes;
        if (backend_ == Backend::Wayland) {
            std::vector<std::string> args = {"wl-paste", "--type", "image/png"};
            if (run_command_read_bytes(args, bytes) && !bytes.empty()) {
                img.data = std::move(bytes);
                img.mime_type = "image/png";
                img.valid = true;
                return img;
            }
        } else {
            std::vector<std::string> args = {"xclip", "-selection", "clipboard", "-t", "image/png", "-o"};
            if (run_command_read_bytes(args, bytes) && !bytes.empty()) {
                img.data = std::move(bytes);
                img.mime_type = "image/png";
                img.valid = true;
                return img;
            }
        }
        return img;
    }

    bool set_image(const std::vector<uint8_t>& data, const std::string& mime_type) override {
        if (data.empty()) return false;
        std::string mime = mime_type.empty() ? "image/png" : mime_type;
        if (backend_ == Backend::Wayland) {
            return run_command_write_bytes({"wl-copy", "--type", mime}, data);
        } else {
            return run_command_write_bytes({"xclip", "-selection", "clipboard", "-t", mime}, data);
        }
    }

private:
    Backend backend_;

    static bool run_command_read(const std::vector<std::string>& args, std::string& output) {
        if (args.empty()) return false;
        int pipefd[2];
        if (pipe(pipefd) != 0) return false;

        pid_t pid = fork();
        if (pid < 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            return false;
        }

        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
            close(pipefd[1]);

            std::vector<char*> c_args;
            for (const auto& a : args) c_args.push_back(const_cast<char*>(a.c_str()));
            c_args.push_back(nullptr);

            execvp(c_args[0], c_args.data());
            _exit(127);
        }

        close(pipefd[1]);
        output.clear();
        std::array<char, 4096> buffer;
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], buffer.data(), buffer.size())) > 0) {
            output.append(buffer.data(), bytes_read);
        }
        close(pipefd[0]);

        int status = 0;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    static bool run_command_read_bytes(const std::vector<std::string>& args, std::vector<uint8_t>& output) {
        if (args.empty()) return false;
        int pipefd[2];
        if (pipe(pipefd) != 0) return false;

        pid_t pid = fork();
        if (pid < 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            return false;
        }

        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
            close(pipefd[1]);

            std::vector<char*> c_args;
            for (const auto& a : args) c_args.push_back(const_cast<char*>(a.c_str()));
            c_args.push_back(nullptr);

            execvp(c_args[0], c_args.data());
            _exit(127);
        }

        close(pipefd[1]);
        output.clear();
        std::array<uint8_t, 8192> buffer;
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], buffer.data(), buffer.size())) > 0) {
            output.insert(output.end(), buffer.data(), buffer.data() + bytes_read);
        }
        close(pipefd[0]);

        int status = 0;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    static bool run_command_write(const std::vector<std::string>& args, const std::string& input) {
        if (args.empty()) return false;
        int pipefd[2];
        if (pipe(pipefd) != 0) return false;

        pid_t pid = fork();
        if (pid < 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            return false;
        }

        if (pid == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
            close(pipefd[0]);

            std::vector<char*> c_args;
            for (const auto& a : args) c_args.push_back(const_cast<char*>(a.c_str()));
            c_args.push_back(nullptr);

            execvp(c_args[0], c_args.data());
            _exit(127);
        }

        close(pipefd[0]);
        size_t total_written = 0;
        while (total_written < input.size()) {
            ssize_t written = write(pipefd[1], input.data() + total_written, input.size() - total_written);
            if (written <= 0) break;
            total_written += written;
        }
        close(pipefd[1]);

        int status = 0;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    static bool run_command_write_bytes(const std::vector<std::string>& args, const std::vector<uint8_t>& input) {
        if (args.empty()) return false;
        int pipefd[2];
        if (pipe(pipefd) != 0) return false;

        pid_t pid = fork();
        if (pid < 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            return false;
        }

        if (pid == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
            close(pipefd[0]);

            std::vector<char*> c_args;
            for (const auto& a : args) c_args.push_back(const_cast<char*>(a.c_str()));
            c_args.push_back(nullptr);

            execvp(c_args[0], c_args.data());
            _exit(127);
        }

        close(pipefd[0]);
        size_t total_written = 0;
        while (total_written < input.size()) {
            ssize_t written = write(pipefd[1], input.data() + total_written, input.size() - total_written);
            if (written <= 0) break;
            total_written += written;
        }
        close(pipefd[1]);

        int status = 0;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
};

std::unique_ptr<IClipboard> create_clipboard() {
    return std::make_unique<LinuxClipboard>();
}

} // namespace webclip

#endif
