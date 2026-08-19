#include "processing.h"
#include <thread>
#include <fstream>
#include <unordered_set>


// Helper to get file size
long long GetFileSizeW(const std::wstring& filename) {
    HANDLE hFile = CreateFileW(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size)) size.QuadPart = 0;
    CloseHandle(hFile);
    return size.QuadPart;
}

void CombineThread(TaskContext* context) {
    long long totalSize = 0;
    for (const auto& file : context->inputFiles) {
        totalSize += GetFileSizeW(file);
    }

    if (totalSize == 0) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
        return;
    }

    std::ofstream out(context->outputFile.c_str(), std::ios::binary);
    if (!out) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
        return;
    }

    const size_t bufferSize = 1024 * 1024; // 1MB buffer
    std::vector<char> buffer(bufferSize);
    long long processedSize = 0;
    int lastPercent = -1;

    for (const auto& file : context->inputFiles) {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) continue;

        while (in.read(buffer.data(), bufferSize) || in.gcount() > 0) {
            if (context->cancelRequested) {
                out.close();
                DeleteFileW(context->outputFile.c_str());
                PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                return;
            }
            size_t bytes = in.gcount();
            out.write(buffer.data(), bytes);
            processedSize += bytes;

            int percent = static_cast<int>((processedSize * 100) / totalSize);
            if (percent != lastPercent) {
                lastPercent = percent;
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
            }
        }
    }
    
    out.close();
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
}

// 64-bit FNV-1a hash for memory efficient deduplication
uint64_t HashString64(const std::string& str) {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void RemoveDuplicatesThread(TaskContext* context) {
    long long totalSize = 0;
    for (const auto& file : context->inputFiles) {
        totalSize += GetFileSizeW(file);
    }

    if (totalSize == 0) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
        return;
    }

    std::ofstream out(context->outputFile.c_str(), std::ios::binary);
    if (!out) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
        return;
    }

    // Using uint64_t hash set to save massive amounts of RAM for multi-GB files.
    std::unordered_set<uint64_t> seenHashes;
    long long processedSize = 0;
    int lastPercent = -1;
    std::string line;

    for (const auto& file : context->inputFiles) {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) continue;
        
        // Use a larger buffer for ifstream
        std::vector<char> ioBuffer(1024 * 1024);
        in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());

        while (std::getline(in, line)) {
            if (context->cancelRequested) {
                out.close();
                DeleteFileW(context->outputFile.c_str());
                PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                return;
            }
            processedSize += line.length() + 1; // approximation for progress

            // Trim carriage return if present
            std::string actualLine = line;
            if (!actualLine.empty() && actualLine.back() == '\r') {
                actualLine.pop_back();
            }

            uint64_t hash = HashString64(actualLine);
            if (seenHashes.find(hash) == seenHashes.end()) {
                seenHashes.insert(hash);
                out << line << "\n";
            }

            int percent = static_cast<int>((processedSize * 100) / totalSize);
            if (percent > 100) percent = 100;
            if (percent != lastPercent) {
                lastPercent = percent;
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
            }
        }
    }

    out.close();
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
}

void StartCombineTask(TaskContext* context) {
    std::thread t(CombineThread, context);
    t.detach();
}

void StartRemoveDuplicatesTask(TaskContext* context) {
    std::thread t(RemoveDuplicatesThread, context);
    t.detach();
}

bool IsValidEmailPass(const std::string& line) {
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos || colonPos == 0 || colonPos == line.length() - 1) return false;
    
    size_t atPos = line.find('@');
    if (atPos == std::string::npos || atPos >= colonPos || atPos == 0 || atPos == colonPos - 1) return false;
    
    size_t dotPos = line.rfind('.', colonPos);
    if (dotPos == std::string::npos || dotPos < atPos || dotPos == colonPos - 1 || dotPos == atPos + 1) return false;

    bool hasPassword = false;
    for (size_t i = colonPos + 1; i < line.length(); ++i) {
        if (line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n') {
            hasPassword = true;
            break;
        }
    }
    
    return hasPassword;
}

void CleanThread(TaskContext* context) {
    long long totalSize = 0;
    for (const auto& file : context->inputFiles) {
        totalSize += GetFileSizeW(file);
    }

    if (totalSize == 0) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
        return;
    }

    std::ofstream out(context->outputFile.c_str(), std::ios::binary);
    if (!out) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
        return;
    }

    long long processedSize = 0;
    int lastPercent = -1;
    std::string line;

    for (const auto& file : context->inputFiles) {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) continue;
        
        std::vector<char> ioBuffer(1024 * 1024);
        in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());

        while (std::getline(in, line)) {
            if (context->cancelRequested) {
                out.close();
                DeleteFileW(context->outputFile.c_str());
                PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                return;
            }
            processedSize += line.length() + 1;

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (IsValidEmailPass(line)) {
                out << line << "\r\n";
            }

            int percent = (int)((processedSize * 100) / totalSize);
            if (percent != lastPercent) {
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                lastPercent = percent;
            }
        }
    }

    out.close();
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
}

void StartCleanTask(TaskContext* context) {
    std::thread t(CleanThread, context);
    t.detach();
}

void ExtractThread(TaskContext* context) {
    long long totalSize = 0;
    for (const auto& file : context->inputFiles) {
        totalSize += GetFileSizeW(file);
    }

    if (totalSize == 0) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
        return;
    }

    std::ofstream out(context->outputFile.c_str(), std::ios::binary);
    if (!out) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
        return;
    }

    long long processedSize = 0;
    int lastPercent = -1;
    std::string line;
    
    std::string targetDomain = context->filterDomain;
    if (!targetDomain.empty() && targetDomain[0] != '@') {
        targetDomain = "@" + targetDomain;
    }

    for (const auto& file : context->inputFiles) {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) continue;
        
        std::vector<char> ioBuffer(1024 * 1024);
        in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());

        while (std::getline(in, line)) {
            if (context->cancelRequested) {
                out.close();
                DeleteFileW(context->outputFile.c_str());
                PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                return;
            }
            processedSize += line.length() + 1;

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.find(targetDomain) != std::string::npos && IsValidEmailPass(line)) {
                out << line << "\r\n";
            }

            int percent = (int)((processedSize * 100) / totalSize);
            if (percent != lastPercent) {
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                lastPercent = percent;
            }
        }
    }

    out.close();
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
}

void StartExtractTask(TaskContext* context) {
    std::thread t(ExtractThread, context);
    t.detach();
}

void SplitThread(TaskContext* context) {
    long long totalSize = 0;
    for (const auto& file : context->inputFiles) {
        totalSize += GetFileSizeW(file);
    }

    if (totalSize == 0 || context->splitLines <= 0) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
        return;
    }

    long long processedSize = 0;
    int lastPercent = -1;
    std::string line;
    
    // Find base name and extension of outputFile
    std::wstring basePath = context->outputFile;
    std::wstring ext = L"";
    size_t dotPos = basePath.rfind(L'.');
    if (dotPos != std::wstring::npos) {
        ext = basePath.substr(dotPos);
        basePath = basePath.substr(0, dotPos);
    }
    
    int partNumber = 1;
    long long currentLineCount = 0;
    std::ofstream out;

    for (const auto& file : context->inputFiles) {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) continue;
        
        std::vector<char> ioBuffer(1024 * 1024);
        in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());

        while (std::getline(in, line)) {
            if (context->cancelRequested) {
                if (out.is_open()) out.close();
                PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                return;
            }
            
            if (!out.is_open()) {
                std::wstring partName = basePath + L"_" + std::to_wstring(partNumber) + ext;
                out.open(partName.c_str(), std::ios::binary);
            }

            processedSize += line.length() + 1;
            
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            out << line << "\r\n";
            currentLineCount++;

            if (currentLineCount >= context->splitLines) {
                out.close();
                currentLineCount = 0;
                partNumber++;
            }

            int percent = (int)((processedSize * 100) / totalSize);
            if (percent != lastPercent) {
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                lastPercent = percent;
            }
        }
    }

    if (out.is_open()) out.close();
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
}

void StartSplitTask(TaskContext* context) {
    std::thread t(SplitThread, context);
    t.detach();
}

#include <algorithm>

void SortThread(TaskContext* context) {
    long long totalSize = 0;
    for (const auto& file : context->inputFiles) {
        totalSize += GetFileSizeW(file);
    }

    if (totalSize == 0) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
        return;
    }

    std::vector<std::string> lines;
    // Pre-allocate to save time, assuming ~30 bytes per line
    lines.reserve(totalSize / 30);
    
    long long processedSize = 0;
    int lastPercent = -1;
    std::string line;

    // Step 1: Read all files
    for (const auto& file : context->inputFiles) {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) continue;
        
        std::vector<char> ioBuffer(1024 * 1024);
        in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());

        while (std::getline(in, line)) {
            if (context->cancelRequested) {
                PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                return;
            }
            processedSize += line.length() + 1;
            
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                lines.push_back(line);
            }
            
            int percent = (int)((processedSize * 50) / totalSize); // First 50% is reading
            if (percent != lastPercent) {
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                lastPercent = percent;
            }
        }
    }

    // Step 2: Sort
    if (context->sortMode == 0) {
        // Alphabetical A-Z
        std::sort(lines.begin(), lines.end());
    } else if (context->sortMode == 1) {
        // Alphabetical Z-A
        std::sort(lines.begin(), lines.end(), [](const std::string& a, const std::string& b) {
            return a > b;
        });
    } else if (context->sortMode == 2) {
        // Length Shortest to Longest
        std::sort(lines.begin(), lines.end(), [](const std::string& a, const std::string& b) {
            if (a.length() != b.length()) return a.length() < b.length();
            return a < b; // Fallback to alphabetical if same length
        });
    } else if (context->sortMode == 3) {
        // Length Longest to Shortest
        std::sort(lines.begin(), lines.end(), [](const std::string& a, const std::string& b) {
            if (a.length() != b.length()) return a.length() > b.length();
            return a < b;
        });
    }

    // Sort is 75% point
    PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 75, 0);

    // Step 3: Write out
    std::ofstream out(context->outputFile.c_str(), std::ios::binary);
    std::vector<char> ioBuffer(1024 * 1024);
    out.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());

    size_t totalLines = lines.size();
    for (size_t i = 0; i < totalLines; ++i) {
        if (context->cancelRequested) {
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
            return;
        }
        out << lines[i] << "\r\n";
        
        if (i % 10000 == 0) {
            int percent = 75 + (int)((i * 25) / totalLines);
            if (percent != lastPercent) {
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                lastPercent = percent;
            }
        }
    }

    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
}

void StartSortTask(TaskContext* context) {
    std::thread t(SortThread, context);
    t.detach();
}
