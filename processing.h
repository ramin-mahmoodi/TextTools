#include <atomic>
#pragma once

#include <windows.h>
#include <vector>
#include <string>

// Custom window messages for threading
#define WM_WORKER_PROGRESS (WM_APP + 1)
#define WM_WORKER_FINISHED (WM_APP + 2)

struct TaskContext {
    HWND hwndMain;
    std::vector<std::wstring> inputFiles;
    std::wstring outputFile;
    std::atomic<bool> cancelRequested;
    std::string filterDomain;
    long long splitLines;
    int sortMode;
    int splitMode;

    TaskContext() : hwndMain(NULL), cancelRequested(false), splitLines(10000), sortMode(0), splitMode(0) {}
};

// Start a thread to combine files
void StartCombineTask(TaskContext* context);

// Start a thread to remove duplicates from files
void StartRemoveDuplicatesTask(TaskContext* context);

// Start a thread to clean invalid email:pass lines
void StartCleanTask(TaskContext* context);

// Start a thread to extract specific domain
void StartExtractTask(TaskContext* context);

// Start a thread to split files by line count
void StartSplitTask(TaskContext* context);

// Start a thread to sort files
void StartSortTask(TaskContext* context);

// Start a thread to scrape from dump
void StartScrapeTask(TaskContext* context);



