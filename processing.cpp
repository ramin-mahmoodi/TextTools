#include "processing.h"
#include <fstream>
#include <unordered_set>
#include <thread>
#include <algorithm>
#include <random>

long long GetFileSizeW(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
        return 0;
    LARGE_INTEGER size;
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    return size.QuadPart;
}

bool IsValidEmailPass(const std::string& line) {
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos || colonPos == 0 || colonPos == line.length() - 1) return false;
    
    // Check if email has @ and .
    std::string email = line.substr(0, colonPos);
    if (email.find('@') == std::string::npos || email.find('.') == std::string::npos) return false;
    
    // Check if password has at least one non-whitespace character
    std::string pass = line.substr(colonPos + 1);
    bool hasChar = false;
    for (char c : pass) {
        if (!isspace((unsigned char)c)) {
            hasChar = true;
            break;
        }
    }
    return hasChar;
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

    long long processedSize = 0;
    int lastPercent = -1;
    size_t totalLines = 0;

    for (const auto& file : context->inputFiles) {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) continue;
        
        std::vector<char> ioBuffer(1024 * 1024);
        in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());
        
        std::string line;
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
            out << line << "\r\n";
            totalLines++;

            int percent = (int)((processedSize * 100) / totalSize);
            if (percent != lastPercent) {
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                lastPercent = percent;
            }
        }
    }
    
    out.close();
    std::wstring* msg = new std::wstring(L"Combined successfully.\nTotal lines: " + std::to_wstring(totalLines));
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
}

void StartCombineTask(TaskContext* context) {
    std::thread t(CombineThread, context);
    t.detach();
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

    std::unordered_set<std::string> seen;
    seen.reserve(totalSize / 30); // Approximate lines based on average 30 chars

    long long processedSize = 0;
    int lastPercent = -1;
    size_t originalLines = 0;
    size_t duplicatesRemoved = 0;

    for (const auto& file : context->inputFiles) {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) continue;
        
        std::vector<char> ioBuffer(1024 * 1024);
        in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());
        
        std::string line;
        while (std::getline(in, line)) {
            if (context->cancelRequested) {
                out.close();
                DeleteFileW(context->outputFile.c_str());
                PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                return;
            }
            processedSize += line.length() + 1;
            originalLines++;

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (seen.insert(line).second) {
                out << line << "\r\n";
            } else {
                duplicatesRemoved++;
            }

            int percent = (int)((processedSize * 100) / totalSize);
            if (percent != lastPercent) {
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                lastPercent = percent;
            }
        }
    }

    out.close();
    std::wstring* msg = new std::wstring(L"Deduplication complete.\nOriginal lines: " + std::to_wstring(originalLines) + L"\nDuplicates removed: " + std::to_wstring(duplicatesRemoved) + L"\nFinal lines: " + std::to_wstring(originalLines - duplicatesRemoved));
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
}

void StartRemoveDuplicatesTask(TaskContext* context) {
    std::thread t(RemoveDuplicatesThread, context);
    t.detach();
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
    size_t originalLines = 0;
    size_t invalidLines = 0;

    for (const auto& file : context->inputFiles) {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) continue;
        
        std::vector<char> ioBuffer(1024 * 1024);
        in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());
        
        std::string line;
        while (std::getline(in, line)) {
            if (context->cancelRequested) {
                out.close();
                DeleteFileW(context->outputFile.c_str());
                PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                return;
            }
            processedSize += line.length() + 1;
            originalLines++;

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (IsValidEmailPass(line)) {
                out << line << "\r\n";
            } else {
                invalidLines++;
            }

            int percent = (int)((processedSize * 100) / totalSize);
            if (percent != lastPercent) {
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                lastPercent = percent;
            }
        }
    }

    out.close();
    std::wstring* msg = new std::wstring(L"Clean complete.\nOriginal lines: " + std::to_wstring(originalLines) + L"\nInvalid removed: " + std::to_wstring(invalidLines) + L"\nFinal valid lines: " + std::to_wstring(originalLines - invalidLines));
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
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
    size_t processedLines = 0;
    size_t matchesFound = 0;
    
    std::string targetDomain = context->filterDomain;
    if (!targetDomain.empty() && targetDomain[0] != '@') {
        targetDomain = "@" + targetDomain;
    }

    for (const auto& file : context->inputFiles) {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) continue;
        
        std::vector<char> ioBuffer(1024 * 1024);
        in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());

        std::string line;
        while (std::getline(in, line)) {
            if (context->cancelRequested) {
                out.close();
                DeleteFileW(context->outputFile.c_str());
                PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                return;
            }
            processedSize += line.length() + 1;
            processedLines++;

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.find(targetDomain) != std::string::npos && IsValidEmailPass(line)) {
                out << line << "\r\n";
                matchesFound++;
            }

            int percent = (int)((processedSize * 100) / totalSize);
            if (percent != lastPercent) {
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                lastPercent = percent;
            }
        }
    }

    out.close();
    std::wstring* msg = new std::wstring(L"Extraction complete.\nLines processed: " + std::to_wstring(processedLines) + L"\nMatches found: " + std::to_wstring(matchesFound));
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
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
    size_t totalLinesWritten = 0;
    
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

        std::string line;
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
            totalLinesWritten++;

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
    
    int finalParts = (currentLineCount == 0) ? (partNumber - 1) : partNumber;
    std::wstring* msg = new std::wstring(L"Split complete.\nTotal parts: " + std::to_wstring(finalParts) + L"\nTotal lines written: " + std::to_wstring(totalLinesWritten));
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
}

void StartSplitTask(TaskContext* context) {
    std::thread t(SplitThread, context);
    t.detach();
}

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
    lines.reserve(totalSize / 30);
    
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
            
            int percent = (int)((processedSize * 50) / totalSize); 
            if (percent != lastPercent) {
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                lastPercent = percent;
            }
        }
    }

    if (context->sortMode == 0) {
        std::sort(lines.begin(), lines.end());
    } else if (context->sortMode == 1) {
        std::sort(lines.begin(), lines.end(), [](const std::string& a, const std::string& b) {
            return a > b;
        });
    } else if (context->sortMode == 2) {
        std::sort(lines.begin(), lines.end(), [](const std::string& a, const std::string& b) {
            if (a.length() != b.length()) return a.length() < b.length();
            return a < b;
        });
    } else if (context->sortMode == 3) {
        std::sort(lines.begin(), lines.end(), [](const std::string& a, const std::string& b) {
            if (a.length() != b.length()) return a.length() > b.length();
            return a < b;
        });
    } else if (context->sortMode == 4) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(lines.begin(), lines.end(), g);
    }

    PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 75, 0);

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

    std::wstring* msg = new std::wstring(L"Operation complete.\nTotal lines: " + std::to_wstring(totalLines));
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
}

void StartSortTask(TaskContext* context) {
    std::thread t(SortThread, context);
    t.detach();
}

void ScrapeThread(TaskContext* context) {
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
    size_t matchesFound = 0;
    for (const auto& file : context->inputFiles) {
        std::ifstream in(file.c_str(), std::ios::binary);
        if (!in) continue;
        std::vector<char> ioBuffer(1024 * 1024);
        in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());
        std::string line;
        while (std::getline(in, line)) {
            if (context->cancelRequested) {
                out.close();
                DeleteFileW(context->outputFile.c_str());
                PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                return;
            }
            processedSize += line.length() + 1;
            
            size_t startPos = 0;
            while (startPos < line.length()) {
                size_t colonPos = line.find(':', startPos);
                if (colonPos == std::string::npos) break;
                
                size_t atPos = line.rfind('@', colonPos);
                if (atPos != std::string::npos && atPos >= startPos && atPos < colonPos - 1) {
                    size_t emailStart = atPos;
                    while (emailStart > startPos && !isspace((unsigned char)line[emailStart - 1]) && line[emailStart - 1] != '|' && line[emailStart - 1] != '"' && line[emailStart - 1] != '\'') {
                        emailStart--;
                    }
                    size_t passEnd = colonPos + 1;
                    while (passEnd < line.length() && !isspace((unsigned char)line[passEnd]) && line[passEnd] != '|' && line[passEnd] != '"' && line[passEnd] != '\'') {
                        passEnd++;
                    }
                    
                    std::string combo = line.substr(emailStart, passEnd - emailStart);
                    if (IsValidEmailPass(combo)) {
                        out << combo << "\r\n";
                        matchesFound++;
                    }
                }
                startPos = colonPos + 1;
            }

            int percent = (int)((processedSize * 100) / totalSize);
            if (percent != lastPercent) {
                PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                lastPercent = percent;
            }
        }
    }
    out.close();
    std::wstring* msg = new std::wstring(L"Raw Scrape complete.\nCombos extracted: " + std::to_wstring(matchesFound));
    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
}

void StartScrapeTask(TaskContext* context) {
    std::thread t(ScrapeThread, context);
    t.detach();
}
