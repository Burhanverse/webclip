#include "clipboard.hpp"
#include <iostream>
#include <cstdlib>
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <gdiplus.h>

namespace webclip {

namespace {

struct GdiPlusScope {
    ULONG_PTR token = 0;
    bool ok = false;

    GdiPlusScope() {
        Gdiplus::GdiplusStartupInput input;
        ok = Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok;
    }
};

GdiPlusScope& gdiplus_scope() {
    static GdiPlusScope scope;
    return scope;
}

size_t dib_pixel_offset(const BITMAPINFOHEADER& hdr) {
    size_t offset = hdr.biSize > 0 ? static_cast<size_t>(hdr.biSize) : sizeof(BITMAPINFOHEADER);
    DWORD colors = hdr.biClrUsed;
    if (colors == 0 && hdr.biBitCount <= 8) {
        colors = 1u << hdr.biBitCount;
    }
    offset += static_cast<size_t>(colors) * sizeof(RGBQUAD);
    if (hdr.biCompression == BI_BITFIELDS && hdr.biSize == sizeof(BITMAPINFOHEADER)) {
        offset += 3 * sizeof(DWORD);
    }
    return offset;
}

bool convert_dib_to_png(const uint8_t* dib, size_t dib_size, std::vector<uint8_t>& png_out) {
    png_out.clear();
    if (!gdiplus_scope().ok || dib == nullptr || dib_size <= sizeof(BITMAPINFOHEADER)) {
        return false;
    }

    const BITMAPINFOHEADER* hdr = reinterpret_cast<const BITMAPINFOHEADER*>(dib);
    if (hdr->biWidth <= 0 || hdr->biHeight == 0) return false;
    const LONG height = hdr->biHeight < 0 ? -hdr->biHeight : hdr->biHeight;
    const WORD bpp = hdr->biBitCount;
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) return false;

    const size_t stride = ((static_cast<size_t>(hdr->biWidth) * bpp + 31) / 32) * 4;
    const size_t offset = dib_pixel_offset(*hdr);
    const size_t pixel_bytes = stride * static_cast<size_t>(height);
    if (offset >= dib_size || pixel_bytes > dib_size - offset) return false;

    HDC hdc = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(hdc, reinterpret_cast<const BITMAPINFO*>(dib), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (bitmap == nullptr || bits == nullptr) {
        if (bitmap) DeleteObject(bitmap);
        return false;
    }
    std::memcpy(bits, dib + offset, pixel_bytes);

    Gdiplus::Bitmap* image = Gdiplus::Bitmap::FromHBITMAP(bitmap, nullptr);
    bool ok = false;
    CLSID png_clsid;
    IStream* stream = nullptr;

    if (image != nullptr &&
        CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}", &png_clsid) == S_OK &&
        CreateStreamOnHGlobal(nullptr, TRUE, &stream) == S_OK) {
        if (image->Save(stream, &png_clsid, nullptr) == Gdiplus::Ok) {
            STATSTG stat{};
            LARGE_INTEGER zero{};
            if (stream->Stat(&stat, STATFLAG_NONAME) == S_OK && stat.cbSize.QuadPart > 0 &&
                stream->Seek(zero, STREAM_SEEK_SET, nullptr) == S_OK) {
                png_out.resize(static_cast<size_t>(stat.cbSize.QuadPart));
                ULONG read_bytes = 0;
                if (stream->Read(png_out.data(), static_cast<ULONG>(png_out.size()), &read_bytes) == S_OK &&
                    read_bytes == png_out.size()) {
                    ok = true;
                }
            }
        }
        stream->Release();
    }

    delete image;
    DeleteObject(bitmap);

    if (!ok) png_out.clear();
    return ok;
}

HGLOBAL png_to_dib_global(const std::vector<uint8_t>& png) {
    if (!gdiplus_scope().ok || png.empty()) return nullptr;

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK) return nullptr;

    HGLOBAL result = nullptr;
    HBITMAP bitmap = nullptr;
    Gdiplus::Bitmap* image = nullptr;

    ULONG written = 0;
    LARGE_INTEGER zero{};
    if (stream->Write(png.data(), static_cast<ULONG>(png.size()), &written) != S_OK ||
        stream->Seek(zero, STREAM_SEEK_SET, nullptr) != S_OK) {
        goto cleanup;
    }

    image = Gdiplus::Bitmap::FromStream(stream);
    if (image == nullptr) goto cleanup;

    if (image->GetHBITMAP(Gdiplus::Color(255, 255, 255), &bitmap) != Gdiplus::Ok || bitmap == nullptr) {
        goto cleanup;
    }

    {
        BITMAP bm{};
        if (GetObject(bitmap, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 || bm.bmHeight == 0) {
            goto cleanup;
        }
        const LONG width = bm.bmWidth;
        const LONG height = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
        const size_t stride = ((static_cast<size_t>(width) * 24 + 31) / 32) * 4;
        const size_t buffer_size = sizeof(BITMAPINFOHEADER) + stride * static_cast<size_t>(height);

        result = GlobalAlloc(GMEM_MOVEABLE, buffer_size);
        if (result == nullptr) goto cleanup;

        void* mem = GlobalLock(result);
        if (mem == nullptr) {
            GlobalFree(result);
            result = nullptr;
            goto cleanup;
        }

        auto* header = static_cast<BITMAPINFOHEADER*>(mem);
        std::memset(header, 0, sizeof(BITMAPINFOHEADER));
        header->biSize = sizeof(BITMAPINFOHEADER);
        header->biWidth = width;
        header->biHeight = height;
        header->biPlanes = 1;
        header->biBitCount = 24;
        header->biCompression = BI_RGB;
        header->biSizeImage = static_cast<DWORD>(stride * static_cast<size_t>(height));

        HDC hdc = GetDC(nullptr);
        const int scan_lines = GetDIBits(
            hdc, bitmap, 0, static_cast<UINT>(height),
            static_cast<BYTE*>(mem) + sizeof(BITMAPINFOHEADER),
            reinterpret_cast<BITMAPINFO*>(header), DIB_RGB_COLORS);
        ReleaseDC(nullptr, hdc);

        GlobalUnlock(result);
        if (scan_lines == 0) {
            GlobalFree(result);
            result = nullptr;
        }
    }

cleanup:
    if (bitmap) DeleteObject(bitmap);
    delete image;
    stream->Release();
    return result;
}

} // namespace

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
        HANDLE hPngData = GetClipboardData(pngFormat);
        if (hPngData != nullptr) {
            SIZE_T size = GlobalSize(hPngData);
            const uint8_t* pData = static_cast<const uint8_t*>(GlobalLock(hPngData));
            if (pData && size > 0) {
                img.data.assign(pData, pData + size);
                img.mime_type = "image/png";
                img.valid = true;
            }
            if (pData) GlobalUnlock(hPngData);
            CloseClipboard();
            return img;
        }

        UINT dibFormat = IsClipboardFormatAvailable(CF_DIBV5) ? CF_DIBV5 : CF_DIB;
        HANDLE hDibData = GetClipboardData(dibFormat);
        if (hDibData != nullptr) {
            SIZE_T size = GlobalSize(hDibData);
            const uint8_t* pData = static_cast<const uint8_t*>(GlobalLock(hDibData));
            std::vector<uint8_t> png_bytes;
            if (pData && size > sizeof(BITMAPINFOHEADER)) {
                convert_dib_to_png(pData, static_cast<size_t>(size), png_bytes);
            }
            if (pData) GlobalUnlock(hDibData);
            if (!png_bytes.empty()) {
                img.data = std::move(png_bytes);
                img.mime_type = "image/png";
                img.valid = true;
            }
        }

        CloseClipboard();
        return img;
    }

    bool set_image(const std::vector<uint8_t>& data, const std::string& mime_type) override {
        if (data.empty()) return false;

        UINT pngFormat = RegisterClipboardFormatW(L"PNG");

        HGLOBAL hPngMem = GlobalAlloc(GMEM_MOVEABLE, data.size());
        if (!hPngMem) return false;

        void* pPngMem = GlobalLock(hPngMem);
        if (!pPngMem) {
            GlobalFree(hPngMem);
            return false;
        }
        std::memcpy(pPngMem, data.data(), data.size());
        GlobalUnlock(hPngMem);

        HGLOBAL hDibMem = png_to_dib_global(data);

        if (!OpenClipboard(nullptr)) {
            GlobalFree(hPngMem);
            if (hDibMem) GlobalFree(hDibMem);
            return false;
        }

        EmptyClipboard();

        bool ok = SetClipboardData(pngFormat, hPngMem) != nullptr;
        if (!ok) {
            GlobalFree(hPngMem);
        }
        if (SetClipboardData(CF_DIB, hDibMem) != nullptr) {
            ok = true;
        } else if (hDibMem) {
            GlobalFree(hDibMem);
        }

        CloseClipboard();
        return ok;
    }

    std::string get_backend_name() const override {
        return "Windows Win32 (Unicode & PNG)";
    }
};

std::unique_ptr<IClipboard> create_clipboard() {
    return std::make_unique<WindowsClipboard>();
}

}

#else

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <ctime>
#include <cerrno>
#include <algorithm>

namespace webclip {

class LinuxClipboard : public IClipboard {
public:
    enum class Backend { Wayland, X11 };

    static constexpr int HELPER_TIMEOUT_MS = 3000;

    static bool wait_pid_deadline(pid_t pid, int timeout_ms, int& exit_code) {
        auto deadline_ms = []() {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
        };

        const int64_t deadline = deadline_ms() + timeout_ms;
        int status = 0;
        for (;;) {
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid) {
                exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 127;
                return true;
            }
            if (r < 0) {
                exit_code = 127;
                return false;
            }
            if (deadline_ms() >= deadline) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                exit_code = 127;
                return false;
            }
            usleep(20 * 1000);
        }
    }

    static bool drain_pipe_deadline(int fd, std::vector<uint8_t>& output, int timeout_ms) {
        output.clear();
        std::array<char, 4096> buffer;
        auto deadline_ms = []() {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
        };
        const int64_t deadline = deadline_ms() + timeout_ms;

        for (;;) {
            int64_t remaining = deadline - deadline_ms();
            if (remaining <= 0) return false;

            struct pollfd pfd{fd, POLLIN, 0};
            int pr = poll(&pfd, 1, static_cast<int>(std::min<int64_t>(remaining, 200)));
            if (pr < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (pr == 0) continue;

            ssize_t n = read(fd, buffer.data(), buffer.size());
            if (n == 0) return true;
            if (n < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            output.insert(output.end(), buffer.data(), buffer.data() + n);

            if (deadline_ms() >= deadline) return false;
        }
    }

public:

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
        std::vector<uint8_t> raw;
        bool ok = drain_pipe_deadline(pipefd[0], raw, HELPER_TIMEOUT_MS);
        close(pipefd[0]);

        int exit_code = 127;
        wait_pid_deadline(pid, HELPER_TIMEOUT_MS, exit_code);
        output.assign(raw.begin(), raw.end());
        return ok && exit_code == 0;
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
        bool ok = drain_pipe_deadline(pipefd[0], output, HELPER_TIMEOUT_MS);
        close(pipefd[0]);

        int exit_code = 127;
        wait_pid_deadline(pid, HELPER_TIMEOUT_MS, exit_code);
        return ok && exit_code == 0;
    }

    static bool write_all_deadline(int fd, const uint8_t* data, size_t size, int timeout_ms) {
        auto deadline_ms = []() {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
        };
        const int64_t deadline = deadline_ms() + timeout_ms;
        size_t total_written = 0;

        while (total_written < size) {
            int64_t remaining = deadline - deadline_ms();
            if (remaining <= 0) return false;

            struct pollfd pfd{fd, POLLOUT, 0};
            int pr = poll(&pfd, 1, static_cast<int>(std::min<int64_t>(remaining, 200)));
            if (pr < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (pr == 0) continue;

            ssize_t written = write(fd, data + total_written, size - total_written);
            if (written < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;
                return false;
            }
            total_written += static_cast<size_t>(written);
        }
        return true;
    }

    template <typename Container>
    static bool run_command_write_impl(const std::vector<std::string>& args, const Container& input) {
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
        const uint8_t* data = reinterpret_cast<const uint8_t*>(input.data());
        bool ok = write_all_deadline(pipefd[1], data, input.size(), HELPER_TIMEOUT_MS);
        close(pipefd[1]);

        int exit_code = 127;
        wait_pid_deadline(pid, HELPER_TIMEOUT_MS, exit_code);
        return ok && exit_code == 0;
    }

    static bool run_command_write(const std::vector<std::string>& args, const std::string& input) {
        return run_command_write_impl(args, input);
    }

    static bool run_command_write_bytes(const std::vector<std::string>& args, const std::vector<uint8_t>& input) {
        return run_command_write_impl(args, input);
    }
};

std::unique_ptr<IClipboard> create_clipboard() {
    return std::make_unique<LinuxClipboard>();
}

}

#endif
