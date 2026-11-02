#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../common/output.hpp"
#include "download.hpp"

using std::string;

// DEBI means download, extract, build, and install.

// Progress state
struct Progress {
    int last_percentage = -1;
    int last_width = -1;
    curl_off_t downloaded = 0;
    curl_off_t total = 0;
    bool visible = false;
    bool enabled = true;

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point last_update{};
};

// Get the terminal width so the bar doesn't take up the whole screen.
static int terminal_width() {
    struct winsize size{};

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0)
        return 0;

    return size.ws_col;
}

// Format an ETA as MM:SS, or HH:MM:SS if it's a long download.
static string format_time(long long seconds) {
    seconds = std::max(0LL, seconds);

    std::ostringstream text;
    text << std::setfill('0');

    if (seconds >= 3600)
        text << std::setw(2) << seconds / 3600 << ':';

    text << std::setw(2) << (seconds / 60) % 60 << ':' << std::setw(2) << seconds % 60;

    return text.str();
}

int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
                      curl_off_t ulnow) {
    auto *progress = static_cast<Progress *>(clientp);

    progress->downloaded = dlnow;
    progress->total = dltotal;

    // Don't draw progress bars when output isn't a terminal.
    if (!progress->enabled || !isatty(STDOUT_FILENO))
        return 0;

    int columns = terminal_width();

    if (columns <= 5)
        return 0;

    constexpr int max_width = 48;
    constexpr double mib = 1024.0 * 1024.0;

    // Some servers don't tell curl the total download size.
    bool has_total = dltotal > 0;

    int percentage = 0;

    if (has_total) {
        percentage =
            std::clamp(static_cast<int>(static_cast<double>(dlnow) / dltotal * 100), 0, 100);
    }

    // Avoid redrawing the same percentage unnecessarily.
    // Also redraw if the user resized the terminal.
    auto now = std::chrono::steady_clock::now();

    constexpr auto interval = std::chrono::milliseconds(80);

    bool resized = columns != progress->last_width;
    bool finished = has_total && percentage == 100 && progress->last_percentage != 100;

    if (progress->visible && !resized && !finished && now - progress->last_update < interval) {
        return 0;
    }

    progress->last_update = now;
    progress->last_percentage = percentage;
    progress->last_width = columns;

    // Estimate remaining time from the average download speed.
    const double elapsed = std::chrono::duration<double>(now - progress->start).count();
    const double speed = elapsed > 0.0 ? static_cast<double>(dlnow) / elapsed : 0.0;

    string eta = "--:--";

    if (has_total && dlnow >= dltotal) {
        eta = "00:00";
    } else if (has_total && speed > 0.0) {
        const auto seconds_left =
            static_cast<long long>(std::max(0.0, static_cast<double>(dltotal - dlnow) / speed));
        eta = format_time(seconds_left);
    }

    // Create the size text before calculating the bar width.
    std::ostringstream size_stream;

    size_stream << std::fixed << std::setprecision(1) << static_cast<double>(dlnow) / mib;

    if (has_total) {
        size_stream << " / " << static_cast<double>(dltotal) / mib;
    }

    size_stream << " MiB";

    string size_text = size_stream.str();
    string percentage_text = has_total ? std::to_string(percentage) + "%" : "--%";

    // Reserve space for:
    // ETA on the left, percentage on the right, and two spaces between parts.
    // Size info is optional: we'd rather keep a bigger trail.
    // 1 spare column to stop the terminal from wrapping.
    const int usable = columns - 1;
    const int base = static_cast<int>(eta.size() + percentage_text.size()) + 2;

    bool show_size = usable >= base + 2 + 16 + 2 + static_cast<int>(size_text.size());
    int size_space = show_size ? 2 + static_cast<int>(size_text.size()) : 0;

    // The trail can grow to 48 columns, but never forces it.
    int bar_width = std::clamp(usable - base - size_space - 2, 0, max_width);

    // If the terminal is too small, hide the MiB text first.
    if (bar_width < 8 && show_size) {
        show_size = false;
        bar_width = std::clamp(usable - base - 2, 0, max_width);
    }

    // If we don't have enough room, just show the ETA and percentage.
    if (bar_width < 4)
        bar_width = 0;

    // The arrow moves across the trail based on percentage.
    int position = bar_width > 0 ? percentage * (bar_width - 1) / 100 : 0;

    // Linux TTY fonts might not support the Unicode symbols.
    const char *term = std::getenv("TERM");
    bool ascii = term && std::string_view(term) == "linux";
    bool color = output::use_color(std::cout);

    constexpr std::string_view green = "\033[32m";
    constexpr std::string_view dim = "\033[90m";
    constexpr std::string_view reset = "\033[0m";

    std::ostringstream line;
    line << eta;

    if (bar_width > 0) {
        line << "  ";

        // Draw the completed part, then the arrow
        // Keep the trail green and the remaining track dim.
        if (color)
            line << green;

        for (int i = 0; i < bar_width; ++i) {
            if (i == position) {
                line << (ascii ? ">" : "➤");
            } else if (i < position) {
                line << (ascii ? "=" : "━");
            } else {
                if (color)
                    line << dim;
                line << (ascii ? "-" : "─");
                // The rest of the track stays dim.
                for (++i; i < bar_width; ++i)
                    line << (ascii ? "-" : "─");
                break;
            }
        }

        if (color)
            line << reset;
    }

    // Show how much we've downloaded, but only when there's room.
    if (show_size && bar_width > 0)
        line << "  " << size_text;

    // Keep the percentage on the far right of our progress line.
    line << "  " << percentage_text;

    // Move to the start, clear the old line, and redraw it.
    std::cout << "\r\033[2K" << line.str() << std::flush;

    progress->visible = true;
    return 0;
}

// Main libcurl function that writes our data to a file so we can DEBI
// peacefully.
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;

    auto *file = static_cast<std::ofstream *>(userdata);

    file->write(ptr, static_cast<std::streamsize>(total));

    if (!*file) {
        return 0;
    }

    return total;
}

string download_source(const string &source_url, const string &name, const string &version,
                       const Settings &settings) {
    // Initialize libcurl so we can download and write data
    // using its functions.
    CURL *curl = curl_easy_init();
    if (!curl) {
        output::error("Failed to initialize libcurl.");
        return "";
    }

    string extension;

    for (const std::string_view ext :
         {".tar.gz", ".tar.xz", ".tar.bz2", ".tar.zst", ".zip", ".tar.lz"}) {
        if (source_url.find(ext) != string::npos) {
            extension = ext;
            break;
        }
    }

    const std::filesystem::path download_dir = settings.cache / "tbdownload";
    string output_path = (download_dir / (name + "-" + version + extension)).string();

    // Create .cache/tarballdownload so it never fails to Download the tarballs.
    std::filesystem::create_directories(download_dir);

    std::ofstream file(output_path, std::ios::binary);
    if (!file) {
        output::error("Failed to open output file.");
        curl_easy_cleanup(curl);
        return "";
    }

    // Configure the source URL for the download.
    curl_easy_setopt(curl, CURLOPT_URL, source_url.c_str());

    // Register our callback to write incoming data.
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    // Pass the output file to our callback.
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

    // Follow HTTP redirects.
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // download HTTP error responses (download the tarballs as 404) as failures.
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    // Enable progress tracking
    Progress progress{};
    progress.enabled = settings.show_progress;
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    // Enable timeouts when a url hangs
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    // Require HTTPS and verify TLS certificates.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");

    // Perform the download and store it in result so we can check if it failed or not.
    CURLcode result = curl_easy_perform(curl);

    if (result == CURLE_OK && progress.total > 0 && progress.last_percentage < 100) {
        progress_callback(&progress, progress.total, progress.total, 0, 0);
    }

    // Keep the finished progress line in the terminal history.
    if (progress.visible)
        std::cout << '\n';

    // long http_code{0};
    // curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code
    // print a newline and make sure to exit if the download fails.
    file.close();

    if (result != CURLE_OK || !file) {
        if (result != CURLE_OK) {
            output::error(string("Download failed: ") + curl_easy_strerror(result));
        } else {
            output::error("Failed to write the downloaded file.");
        }

        // Delete the incomplete archive instead of leaving it in cache.
        std::filesystem::remove(output_path);
        curl_easy_cleanup(curl);
        return "";
    }

    // free resources so we dont memory leak
    curl_easy_cleanup(curl);
    return output_path;
}
