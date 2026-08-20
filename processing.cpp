#include <future>
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
    
    std::string email = line.substr(0, colonPos);
    size_t atPos = email.find('@');
    size_t dotPos = email.rfind('.');
    if (atPos == std::string::npos || dotPos == std::string::npos || dotPos < atPos || atPos == 0 || dotPos == email.length() - 1 || dotPos == atPos + 1) return false;
    
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

void GetProtectedOutput(TaskContext* context, std::wstring& tempOutputFile, bool& needsReplace) {
    tempOutputFile = context->outputFile;
    needsReplace = false;
    for (const auto& f : context->inputFiles) {
        if (f == context->outputFile) {
            tempOutputFile += L".tmp";
            needsReplace = true;
            break;
        }
    }
}

void FinalizeProtectedOutput(const std::wstring& tempOutputFile, const std::wstring& outputFile, bool needsReplace) {
    if (needsReplace) {
        MoveFileExW(tempOutputFile.c_str(), outputFile.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
}

void CombineThread(TaskContext* context) {
    try {
        long long totalSize = 0;
        for (const auto& file : context->inputFiles) totalSize += GetFileSizeW(file);

        if (totalSize == 0) {
            PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
            delete context;
            return;
        }

        std::wstring tempOutputFile;
        bool needsReplace;
        GetProtectedOutput(context, tempOutputFile, needsReplace);

        std::ofstream out(tempOutputFile.c_str(), std::ios::binary);
        if (!out) {
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
            delete context;
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
                    DeleteFileW(tempOutputFile.c_str());
                    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                    delete context;
                    return;
                }
                processedSize += line.length() + 1;

                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                out << line << "\r\n";
                totalLines++;

                if (totalSize > 0) {
                    int percent = (int)((processedSize * 100) / totalSize);
                    if (percent != lastPercent) {
                        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                        lastPercent = percent;
                    }
                }
            }
        }
        
        out.close();
        FinalizeProtectedOutput(tempOutputFile, context->outputFile, needsReplace);
        
        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
        std::wstring* msg = new std::wstring(L"Combined successfully.\nTotal lines: " + std::to_wstring(totalLines));
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
        delete context;
    } catch (...) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
        delete context;
    }
}

void StartCombineTask(TaskContext* context) {
    std::thread t(CombineThread, context);
    t.detach();
}

void RemoveDuplicatesThread(TaskContext* context) {
    try {
        long long totalSize = 0;
        for (const auto& file : context->inputFiles) totalSize += GetFileSizeW(file);

        if (totalSize == 0) {
            PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
            delete context;
            return;
        }

        std::wstring tempOutputFile;
        bool needsReplace;
        GetProtectedOutput(context, tempOutputFile, needsReplace);

        std::ofstream out(tempOutputFile.c_str(), std::ios::binary);
        if (!out) {
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
            delete context;
            return;
        }

        std::unordered_set<std::string> seen;
        seen.reserve(totalSize / 30);

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
                    DeleteFileW(tempOutputFile.c_str());
                    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                    delete context;
                    return;
                }
                processedSize += line.length() + 1;
                originalLines++;

                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (seen.insert(line).second) {
                    out << line << "\r\n";
                } else {
                    duplicatesRemoved++;
                }

                if (totalSize > 0) {
                    int percent = (int)((processedSize * 100) / totalSize);
                    if (percent != lastPercent) {
                        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                        lastPercent = percent;
                    }
                }
            }
        }

        out.close();
        FinalizeProtectedOutput(tempOutputFile, context->outputFile, needsReplace);
        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
        std::wstring* msg = new std::wstring(L"Deduplication complete.\nOriginal lines: " + std::to_wstring(originalLines) + L"\nDuplicates removed: " + std::to_wstring(duplicatesRemoved) + L"\nFinal lines: " + std::to_wstring(originalLines - duplicatesRemoved));
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
        delete context;
    } catch (...) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
        delete context;
    }
}

void StartRemoveDuplicatesTask(TaskContext* context) {
    std::thread t(RemoveDuplicatesThread, context);
    t.detach();
}

void CleanThread(TaskContext* context) {
    try {
        long long totalSize = 0;
        for (const auto& file : context->inputFiles) totalSize += GetFileSizeW(file);

        if (totalSize == 0) {
            PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
            delete context;
            return;
        }

        std::wstring tempOutputFile;
        bool needsReplace;
        GetProtectedOutput(context, tempOutputFile, needsReplace);

        std::ofstream out(tempOutputFile.c_str(), std::ios::binary);
        if (!out) {
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
            delete context;
            return;
        }

        long long processedSize = 0;
        int lastPercent = -1;
        size_t originalLines = 0;
        size_t invalidLines = 0;

        const size_t BATCH_SIZE = 100000;
        std::vector<std::string> batch;
        batch.reserve(BATCH_SIZE);

        auto processBatch = [](const std::vector<std::string>& b) {
            std::vector<char> valid(b.size(), 0);
            for (size_t i = 0; i < b.size(); ++i) {
                if (IsValidEmailPass(b[i])) valid[i] = 1;
            }
            return valid;
        };

        for (const auto& file : context->inputFiles) {
            std::ifstream in(file.c_str(), std::ios::binary);
            if (!in) continue;
            
            std::vector<char> ioBuffer(1024 * 1024);
            in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());
            
            std::string line;
            while (true) {
                bool hasLine = (bool)std::getline(in, line);
                if (hasLine) {
                    processedSize += line.length() + 1;
                    originalLines++;
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    batch.push_back(std::move(line));
                }

                if (batch.size() >= BATCH_SIZE || (!hasLine && !batch.empty())) {
                    if (context->cancelRequested) {
                        out.close();
                        DeleteFileW(tempOutputFile.c_str());
                        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                        delete context;
                        return;
                    }

                    size_t numThreads = std::thread::hardware_concurrency();
                    if (numThreads == 0) numThreads = 4;
                    
                    std::vector<std::future<std::vector<char>>> futures;
                    size_t chunkSize = (batch.size() + numThreads - 1) / numThreads;
                    
                    for (size_t t = 0; t < numThreads; ++t) {
                        size_t startIdx = t * chunkSize;
                        size_t endIdx = std::min(startIdx + chunkSize, batch.size());
                        if (startIdx < endIdx) {
                            std::vector<std::string> subBatch(batch.begin() + startIdx, batch.begin() + endIdx);
                            futures.push_back(std::async(std::launch::async, processBatch, std::move(subBatch)));
                        }
                    }
                    
                    size_t currentIdx = 0;
                    for (auto& f : futures) {
                        std::vector<char> valid = f.get();
                        for (size_t i = 0; i < valid.size(); ++i) {
                            if (valid[i]) {
                                out << batch[currentIdx] << "\r\n";
                            } else {
                                invalidLines++;
                            }
                            currentIdx++;
                        }
                    }
                    batch.clear();
                }
                
                if (!hasLine) break;

                if (totalSize > 0) {
                    int percent = (int)((processedSize * 100) / totalSize);
                    if (percent != lastPercent) {
                        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                        lastPercent = percent;
                    }
                }
            }
        }

        out.close();
        FinalizeProtectedOutput(tempOutputFile, context->outputFile, needsReplace);
        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
        std::wstring* msg = new std::wstring(L"Clean complete.\nOriginal lines: " + std::to_wstring(originalLines) + L"\nInvalid removed: " + std::to_wstring(invalidLines) + L"\nFinal valid lines: " + std::to_wstring(originalLines - invalidLines));
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
        delete context;
    } catch (...) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
        delete context;
    }
}

void StartCleanTask(TaskContext* context) {
    std::thread t(CleanThread, context);
    t.detach();
}

void ExtractThread(TaskContext* context) {
    try {
        long long totalSize = 0;
        for (const auto& file : context->inputFiles) totalSize += GetFileSizeW(file);

        if (totalSize == 0) {
            PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
            delete context;
            return;
        }

        std::wstring tempOutputFile;
        bool needsReplace;
        GetProtectedOutput(context, tempOutputFile, needsReplace);

        std::ofstream out(tempOutputFile.c_str(), std::ios::binary);
        if (!out) {
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
            delete context;
            return;
        }

        long long processedSize = 0;
        int lastPercent = -1;
        size_t processedLines = 0;
        size_t matchesFound = 0;
        
        std::string targetDomain = context->filterDomain;
        std::transform(targetDomain.begin(), targetDomain.end(), targetDomain.begin(), ::tolower);
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
                    DeleteFileW(tempOutputFile.c_str());
                    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                    delete context;
                    return;
                }
                processedSize += line.length() + 1;
                processedLines++;

                if (!line.empty() && line.back() == '\r') line.pop_back();

                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos && IsValidEmailPass(line)) {
                    std::string email = line.substr(0, colonPos);
                    std::transform(email.begin(), email.end(), email.begin(), ::tolower);
                    if (email.find(targetDomain) != std::string::npos) {
                        out << line << "\r\n";
                        matchesFound++;
                    }
                }

                if (totalSize > 0) {
                    int percent = (int)((processedSize * 100) / totalSize);
                    if (percent != lastPercent) {
                        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                        lastPercent = percent;
                    }
                }
            }
        }

        out.close();
        FinalizeProtectedOutput(tempOutputFile, context->outputFile, needsReplace);
        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
        std::wstring* msg = new std::wstring(L"Extraction complete.\nLines processed: " + std::to_wstring(processedLines) + L"\nMatches found: " + std::to_wstring(matchesFound));
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
        delete context;
    } catch (...) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
        delete context;
    }
}

void StartExtractTask(TaskContext* context) {
    std::thread t(ExtractThread, context);
    t.detach();
}

void SplitThread(TaskContext* context) {
    try {
        long long totalSize = 0;
        for (const auto& file : context->inputFiles) totalSize += GetFileSizeW(file);

        if (totalSize == 0 || context->splitLines <= 0) {
            PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
            delete context;
            return;
        }

        long long processedSize = 0;
        int lastPercent = -1;
        size_t totalLinesWritten = 0;
        
        std::wstring basePath = context->outputFile;
        std::wstring ext = L"";
        size_t slashPos = basePath.find_last_of(L"/\\");
        size_t dotPos = basePath.rfind(L'.');
        if (dotPos != std::wstring::npos && (slashPos == std::wstring::npos || dotPos > slashPos)) {
            ext = basePath.substr(dotPos);
            basePath = basePath.substr(0, dotPos);
        }
        
        int partNumber = 1;
        long long currentCount = 0; // lines or bytes depending on mode
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
                    delete context;
                    return;
                }
                
                if (!out.is_open()) {
                    std::wstring partName = basePath + L"_" + std::to_wstring(partNumber) + ext;
                    out.open(partName.c_str(), std::ios::binary);
                }

                processedSize += line.length() + 1;
                
                if (!line.empty() && line.back() == '\r') line.pop_back();

                out << line << "\r\n";
                totalLinesWritten++;
                
                if (context->splitMode == 1) { // Split by MB
                    currentCount += line.length() + 2; // +2 for \r\n
                    if (currentCount >= context->splitLines * 1024 * 1024) {
                        out.close();
                        currentCount = 0;
                        partNumber++;
                    }
                } else { // Split by Lines
                    currentCount++;
                    if (currentCount >= context->splitLines) {
                        out.close();
                        currentCount = 0;
                        partNumber++;
                    }
                }

                if (totalSize > 0) {
                    int percent = (int)((processedSize * 100) / totalSize);
                    if (percent != lastPercent) {
                        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                        lastPercent = percent;
                    }
                }
            }
        }

        if (out.is_open()) out.close();
        
        int finalParts = (currentCount == 0) ? (partNumber - 1) : partNumber;
        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
        std::wstring* msg = new std::wstring(L"Split complete.\nTotal parts: " + std::to_wstring(finalParts) + L"\nTotal lines written: " + std::to_wstring(totalLinesWritten));
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
        delete context;
    } catch (...) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
        delete context;
    }
}

void StartSplitTask(TaskContext* context) {
    std::thread t(SplitThread, context);
    t.detach();
}

void SortThread(TaskContext* context) {
    try {
        long long totalSize = 0;
        for (const auto& file : context->inputFiles) totalSize += GetFileSizeW(file);

        if (totalSize == 0) {
            PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
            delete context;
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
                    delete context;
                    return;
                }
                processedSize += line.length() + 1;
                
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) {
                    lines.push_back(line);
                }
                
                if (totalSize > 0) {
                    int percent = (int)((processedSize * 50) / totalSize); 
                    if (percent != lastPercent) {
                        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                        lastPercent = percent;
                    }
                }
            }
        }

        if (context->sortMode == 0) {
            std::sort(lines.begin(), lines.end());
        } else if (context->sortMode == 1) {
            std::sort(lines.begin(), lines.end(), [](const std::string& a, const std::string& b) { return a > b; });
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

        std::wstring tempOutputFile;
        bool needsReplace;
        GetProtectedOutput(context, tempOutputFile, needsReplace);

        std::ofstream out(tempOutputFile.c_str(), std::ios::binary);
        if (!out) {
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
            delete context;
            return;
        }
        std::vector<char> ioBuffer(1024 * 1024);
        out.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());

        size_t totalLines = lines.size();
        for (size_t i = 0; i < totalLines; ++i) {
            if (context->cancelRequested) {
                out.close();
                DeleteFileW(tempOutputFile.c_str());
                PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                delete context;
                return;
            }
            out << lines[i] << "\r\n";
            
            if (i % 10000 == 0) {
                if (totalLines > 0) {
                    int percent = 75 + (int)((i * 25) / totalLines);
                    if (percent != lastPercent) {
                        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                        lastPercent = percent;
                    }
                }
            }
        }
        out.close();
        FinalizeProtectedOutput(tempOutputFile, context->outputFile, needsReplace);
        
        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
        std::wstring* msg = new std::wstring(L"Operation complete.\nTotal lines: " + std::to_wstring(totalLines));
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
        delete context;
    } catch (...) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
        delete context;
    }
}

void StartSortTask(TaskContext* context) {
    std::thread t(SortThread, context);
    t.detach();
}

void ScrapeThread(TaskContext* context) {
    try {
        long long totalSize = 0;
        for (const auto& file : context->inputFiles) totalSize += GetFileSizeW(file);
        
        if (totalSize == 0) {
            PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, 0);
            delete context;
            return;
        }

        std::wstring tempOutputFile;
        bool needsReplace;
        GetProtectedOutput(context, tempOutputFile, needsReplace);

        std::ofstream out(tempOutputFile.c_str(), std::ios::binary);
        if (!out) {
            PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
            delete context;
            return;
        }
        
        long long processedSize = 0;
        int lastPercent = -1;
        size_t matchesFound = 0;
        const std::string badChars = "|\"',;{}[]";

        for (const auto& file : context->inputFiles) {
            std::ifstream in(file.c_str(), std::ios::binary);
            if (!in) continue;
            std::vector<char> ioBuffer(1024 * 1024);
            in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());
            std::string line;
            while (std::getline(in, line)) {
                if (context->cancelRequested) {
                    out.close();
                    DeleteFileW(tempOutputFile.c_str());
                    PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
                    delete context;
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
                        while (emailStart > startPos && !isspace((unsigned char)line[emailStart - 1]) && badChars.find(line[emailStart - 1]) == std::string::npos) {
                            emailStart--;
                        }
                        size_t passEnd = colonPos + 1;
                        while (passEnd < line.length() && !isspace((unsigned char)line[passEnd]) && badChars.find(line[passEnd]) == std::string::npos) {
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

                if (totalSize > 0) {
                    int percent = (int)((processedSize * 100) / totalSize);
                    if (percent != lastPercent) {
                        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, percent, 0);
                        lastPercent = percent;
                    }
                }
            }
        }
        out.close();
        FinalizeProtectedOutput(tempOutputFile, context->outputFile, needsReplace);
        PostMessage(context->hwndMain, WM_WORKER_PROGRESS, 100, 0);
        std::wstring* msg = new std::wstring(L"Raw Scrape complete.\nCombos extracted: " + std::to_wstring(matchesFound));
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 1, (LPARAM)msg);
        delete context;
    } catch (...) {
        PostMessage(context->hwndMain, WM_WORKER_FINISHED, 0, 0);
        delete context;
    }
}

void StartScrapeTask(TaskContext* context) {
    std::thread t(ScrapeThread, context);
    t.detach();
}
