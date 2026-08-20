#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <algorithm>
#include "processing.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

// UI Element IDs
#define ID_LISTVIEW 101
#define ID_BTN_ADD 102
#define ID_BTN_REMOVE 103
#define ID_BTN_COMBINE 104
#define ID_BTN_DEDUPE 105
#define ID_BTN_CLEAN 108
#define ID_PROGRESS 106
#define ID_CHK_SELECT_ALL 107

#define ID_BTN_EXTRACT 109
#define ID_EDIT_DOMAIN 110
#define ID_BTN_SPLIT 111
#define ID_EDIT_SPLIT 112
#define ID_BTN_SORT 113
#define ID_COMBO_SORT 114
#define ID_COMBO_SPLIT_MODE 116
#define ID_BTN_SCRAPE 115

// Global Variables
HWND hMainWnd;
HWND hListView;
HWND hBtnAdd, hBtnRemove, hBtnCombine, hBtnDedupe, hBtnClean, hBtnScrape, hBtnExtract, hBtnSplit, hBtnSort;
HWND hEditDomain, hEditSplit, hComboSort, hComboSplitMode;
HWND hProgressBar;
HWND hChkSelectAll;


TaskContext* currentTask = nullptr;
int g_nProgress = 0;

LRESULT CALLBACK ListViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_NOTIFY) {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        if (nmhdr->code == NM_CUSTOMDRAW) {
            LPNMCUSTOMDRAW nmcd = (LPNMCUSTOMDRAW)lParam;
            if (nmhdr->hwndFrom == (HWND)SendMessage(hWnd, LVM_GETHEADER, 0, 0)) {
                if (nmcd->dwDrawStage == CDDS_PREPAINT) {
                    HDC hdc = nmcd->hdc;
                    RECT rc;
                    GetClientRect(nmhdr->hwndFrom, &rc);
                    
                    HBRUSH hBrush = CreateSolidBrush(RGB(36, 36, 36));
                    FillRect(hdc, &rc, hBrush);
                    DeleteObject(hBrush);
                    
                    int count = SendMessage(nmhdr->hwndFrom, HDM_GETITEMCOUNT, 0, 0);
                    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
                    HGDIOBJ hOldPen = SelectObject(hdc, hPen);
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, RGB(240, 240, 240));
                    
                    for (int i = 0; i < count; i++) {
                        RECT itemRc;
                        SendMessage(nmhdr->hwndFrom, HDM_GETITEMRECT, i, (LPARAM)&itemRc);
                        
                        if (i > 0) {
                            MoveToEx(hdc, itemRc.right - 1, itemRc.top + 4, NULL);
                            LineTo(hdc, itemRc.right - 1, itemRc.bottom - 4);
                        }
                        
                        wchar_t text[256] = {0};
                        HDITEMW hdi = {0};
                        hdi.mask = HDI_TEXT;
                        hdi.pszText = text;
                        hdi.cchTextMax = 256;
                        SendMessage(nmhdr->hwndFrom, HDM_GETITEMW, i, (LPARAM)&hdi);
                        
                        RECT textRc = itemRc;
                        textRc.left += 6;
                        DrawTextW(hdc, text, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                    }
                    SelectObject(hdc, hOldPen);
                    DeleteObject(hPen);
                    
                    return CDRF_SKIPDEFAULT;
                }
            }
        }
    } else if (uMsg == WM_NCDESTROY) {
        RemoveWindowSubclass(hWnd, ListViewSubclassProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_NCPAINT:
            return 0; // Disable standard border
        case WM_PAINT: {
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            HDC hdc = GetWindowDC(hWnd);
            RECT rc;
            GetWindowRect(hWnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
            HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
            
            Rectangle(hdc, 0, 0, w, h);
            
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPen);
            ReleaseDC(hWnd, hdc);
            return res;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, EditSubclassProc, uIdSubclass);
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ComboSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_PAINT) {
        LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        HDC hdc = GetWindowDC(hWnd);
        RECT rc;
        GetWindowRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        // Outer border (1px)
        HBRUSH hBorderBrush = CreateSolidBrush(RGB(60, 60, 60));
        RECT borderRect = {0, 0, w, h};
        FrameRect(hdc, &borderRect, hBorderBrush);
        DeleteObject(hBorderBrush);

        // Erase classic 3D inner borders using background color
        HBRUSH hBgBrush = CreateSolidBrush(RGB(45, 45, 48));
        InflateRect(&borderRect, -1, -1);
        FrameRect(hdc, &borderRect, hBgBrush);
        InflateRect(&borderRect, -1, -1);
        FrameRect(hdc, &borderRect, hBgBrush);
        DeleteObject(hBgBrush);

        // Draw arrow button
        COMBOBOXINFO cbi = { sizeof(COMBOBOXINFO) };
        if (GetComboBoxInfo(hWnd, &cbi)) {
            RECT rcBtn = cbi.rcButton;
            
            // Adjust button to avoid painting over border
            rcBtn.top = 1; rcBtn.bottom = h - 1; rcBtn.right = w - 1;
            
            HBRUSH btnBrush = CreateSolidBrush(RGB(45, 45, 48));
            FillRect(hdc, &rcBtn, btnBrush);
            DeleteObject(btnBrush);
            
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            MoveToEx(hdc, rcBtn.left, rcBtn.top, NULL);
            LineTo(hdc, rcBtn.left, rcBtn.bottom);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            int centerX = rcBtn.left + (rcBtn.right - rcBtn.left) / 2;
            int centerY = rcBtn.top + (rcBtn.bottom - rcBtn.top) / 2;
            
            HBRUSH arrowBrush = CreateSolidBrush(RGB(200, 200, 200));
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, arrowBrush);
            HPEN arrowPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
            hOldPen = (HPEN)SelectObject(hdc, arrowPen);
            
            POINT pts[3];
            pts[0].x = centerX - 4; pts[0].y = centerY - 2;
            pts[1].x = centerX + 4; pts[1].y = centerY - 2;
            pts[2].x = centerX;     pts[2].y = centerY + 3;
            Polygon(hdc, pts, 3);
            
            SelectObject(hdc, hOldBrush);
            SelectObject(hdc, hOldPen);
            DeleteObject(arrowBrush);
            DeleteObject(arrowPen);
        }
        
        ReleaseDC(hWnd, hdc);
        return res;
    }
    else if (uMsg == WM_NCDESTROY) {
        RemoveWindowSubclass(hWnd, ComboSubclassProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_MOUSEMOVE: {
            if (!GetPropW(hWnd, L"Hovered")) {
                SetPropW(hWnd, L"Hovered", (HANDLE)1);
                TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0};
                TrackMouseEvent(&tme);
                InvalidateRect(hWnd, NULL, TRUE);
            }
            break;
        }
        case WM_MOUSELEAVE: {
            RemovePropW(hWnd, L"Hovered");
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        case WM_NCDESTROY: {
            RemovePropW(hWnd, L"Hovered");
            RemoveWindowSubclass(hWnd, ButtonSubclassProc, uIdSubclass);
            break;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

RECT g_rcListPanel;
RECT g_rcControlPanel;
RECT g_rcExtractPanel;

// Helper to disable/enable UI during processing
void SetUIState(bool enabled) {
    EnableWindow(hListView, enabled);
    EnableWindow(hBtnAdd, enabled);
    EnableWindow(hBtnRemove, enabled);
    EnableWindow(hBtnCombine, enabled);
    EnableWindow(hBtnDedupe, enabled);
    EnableWindow(hBtnClean, enabled);
    EnableWindow(hBtnScrape, enabled);
    EnableWindow(hBtnExtract, enabled);
    EnableWindow(hEditDomain, enabled);
    EnableWindow(hBtnSplit, enabled);
    EnableWindow(hEditSplit, enabled);
    EnableWindow(hBtnSort, enabled);
    EnableWindow(hComboSort, enabled);
    EnableWindow(hComboSplitMode, enabled);
    EnableWindow(hChkSelectAll, enabled);
}

// Add files to ListView
#include <stdio.h>
std::wstring FormatCommas(size_t value) {
    std::wstring s = std::to_wstring(value);
    int insertPosition = s.length() - 3;
    while (insertPosition > 0) {
        s.insert(insertPosition, L",");
        insertPosition -= 3;
    }
    return s;
}

std::wstring GetFileSizeStr(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return L"0 B";
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    CloseHandle(hFile);
    double mb = (double)size.QuadPart / (1024.0 * 1024.0);
    if (mb < 1.0) {
        double kb = (double)size.QuadPart / 1024.0;
        return std::to_wstring((int)kb) + L" KB";
    }
    wchar_t buf[64];
    swprintf(buf, 64, L"%.2f MB", mb);
    return std::wstring(buf);
}

std::wstring GetFileLinesStr(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return L"0";
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    if (size.QuadPart == 0) { CloseHandle(hFile); return L"0"; }
    HANDLE hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) { CloseHandle(hFile); return L"0"; }
    const char* view = (const char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!view) { CloseHandle(hMap); CloseHandle(hFile); return L"?"; }
    
    size_t lines = 0;
    for (long long i = 0; i < size.QuadPart; ++i) {
        if (view[i] == '\n') lines++;
    }
    if (size.QuadPart > 0 && view[size.QuadPart - 1] != '\n') lines++;
    
    UnmapViewOfFile(view);
    CloseHandle(hMap);
    CloseHandle(hFile);
    
    return FormatCommas(lines);
}

void AddFilesToList(const std::vector<std::wstring>& files) {
    for (const auto& file : files) {
        LVITEMW lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = ListView_GetItemCount(hListView);
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)L""; // Empty for checkbox column
        int idx = ListView_InsertItem(hListView, &lvi);
        ListView_SetCheckState(hListView, idx, TRUE);
        ListView_SetItemText(hListView, idx, 1, const_cast<LPWSTR>(file.c_str()));
        
        std::wstring sizeStr = GetFileSizeStr(file);
        ListView_SetItemText(hListView, idx, 2, const_cast<LPWSTR>(sizeStr.c_str()));
        
        std::wstring linesStr = GetFileLinesStr(file);
        ListView_SetItemText(hListView, idx, 3, const_cast<LPWSTR>(linesStr.c_str()));
    }
}

// Open File Dialog
void OnAddFiles() {
    OPENFILENAMEW ofn = {0};
    wchar_t szFile[65536] = {0}; // Large buffer for multiple files

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hMainWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Supported Files\0*.txt;*.csv;*.log;*.sql;*.json\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn) == TRUE) {
        std::vector<std::wstring> files;
        wchar_t* p = ofn.lpstrFile;
        std::wstring dir = p;
        p += dir.length() + 1;
        if (*p == 0) {
            // Single file selected
            files.push_back(dir);
        } else {
            // Multiple files selected
            while (*p) {
                std::wstring filename = p;
                files.push_back(dir + L"\\" + filename);
                p += filename.length() + 1;
            }
        }
        AddFilesToList(files);
    }
}

// Remove checked files
void OnRemoveFiles() {
    int count = ListView_GetItemCount(hListView);
    for (int i = count - 1; i >= 0; --i) {
        if (ListView_GetCheckState(hListView, i)) {
            ListView_DeleteItem(hListView, i);
        }
    }
}

// Get checked files from list
std::vector<std::wstring> GetSelectedFiles() {
    std::vector<std::wstring> files;
    int count = ListView_GetItemCount(hListView);
    for (int i = 0; i < count; ++i) {
        if (ListView_GetCheckState(hListView, i)) {
            wchar_t buffer[MAX_PATH];
            ListView_GetItemText(hListView, i, 1, buffer, MAX_PATH);
            files.push_back(buffer);
        }
    }
    return files;
}

// Process task
void ExecuteTask(int mode) {
    std::vector<std::wstring> files = GetSelectedFiles();
    if (files.empty()) {
        MessageBoxW(hMainWnd, L"Please select at least one file from the list.", L"Notice", MB_OK | MB_ICONINFORMATION);
        return;
    }

    OPENFILENAMEW ofn = {0};
    wchar_t szFile[MAX_PATH] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hMainWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn) == TRUE) {
        currentTask = new TaskContext();
        currentTask->hwndMain = hMainWnd;
        currentTask->inputFiles = files;
        currentTask->outputFile = szFile;

        if (mode == 3) {
            wchar_t domainBuf[256];
            GetWindowTextW(hEditDomain, domainBuf, 256);
            if (wcslen(domainBuf) == 0) {
                MessageBoxW(hMainWnd, L"Please enter a domain to extract.", L"Notice", MB_OK | MB_ICONWARNING);
                delete currentTask;
                currentTask = nullptr;
                return;
            }
            char narrowBuf[256];
            WideCharToMultiByte(CP_UTF8, 0, domainBuf, -1, narrowBuf, 256, NULL, NULL);
            currentTask->filterDomain = narrowBuf;
        } else if (mode == 4) {
            wchar_t splitBuf[256];
            GetWindowTextW(hEditSplit, splitBuf, 256);
            long long lines = 0;
            try {
                lines = std::stoll(splitBuf);
            } catch (...) {
                lines = 0;
            }
            
            if (lines <= 0) {
                MessageBoxW(hMainWnd, L"Please enter a valid positive number for lines.", L"Notice", MB_OK | MB_ICONWARNING);
                delete currentTask;
                currentTask = nullptr;
                return;
            }
            currentTask->splitLines = lines;
        } else if (mode == 4) {
            currentTask->splitMode = SendMessageW(hComboSplitMode, CB_GETCURSEL, 0, 0);
        } else if (mode == 5) {
            currentTask->sortMode = SendMessageW(hComboSort, CB_GETCURSEL, 0, 0);
            if (currentTask->sortMode == CB_ERR) currentTask->sortMode = 0;
        }

        SetUIState(false);
        SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

        if (mode == 0) {
            StartCombineTask(currentTask);
        } else if (mode == 1) {
            StartRemoveDuplicatesTask(currentTask);
        } else if (mode == 2) {
            StartCleanTask(currentTask);
        } else if (mode == 3) {
            StartExtractTask(currentTask);
        } else if (mode == 4) {
            StartSplitTask(currentTask);
        } else if (mode == 4) {
            currentTask->splitMode = SendMessageW(hComboSplitMode, CB_GETCURSEL, 0, 0);
        } else if (mode == 5) {
            StartSortTask(currentTask);
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_ERASEBKGND:
            return 1;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Background
            HBRUSH bgBrush = CreateSolidBrush(RGB(24, 24, 24));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);
            
            // Panels
            HBRUSH panelBrush = CreateSolidBrush(RGB(36, 36, 36));
            HPEN panelPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 50));
            HGDIOBJ oldBrush = SelectObject(hdc, panelBrush);
            HGDIOBJ oldPen = SelectObject(hdc, panelPen);
            
            RoundRect(hdc, g_rcListPanel.left, g_rcListPanel.top, g_rcListPanel.right, g_rcListPanel.bottom, 10, 10);
            RoundRect(hdc, g_rcControlPanel.left, g_rcControlPanel.top, g_rcControlPanel.right, g_rcControlPanel.bottom, 10, 10);
            
            // Draw separator line in control panel
            if (hEditDomain) { // if created
                int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
                int margin = MulDiv(10, dpi, 96);
                
                RECT rcEdit;
                GetWindowRect(hEditDomain, &rcEdit);
                POINT pt = {rcEdit.left, rcEdit.top};
                ScreenToClient(hwnd, &pt);
                int yLine1 = pt.y - MulDiv(5, dpi, 96);
                MoveToEx(hdc, g_rcControlPanel.left + margin, yLine1, NULL);
                LineTo(hdc, g_rcControlPanel.right - margin, yLine1);

                GetWindowRect(hEditSplit, &rcEdit);
                pt = {rcEdit.left, rcEdit.top};
                ScreenToClient(hwnd, &pt);
                int yLine2 = pt.y - MulDiv(5, dpi, 96);
                MoveToEx(hdc, g_rcControlPanel.left + margin, yLine2, NULL);
                LineTo(hdc, g_rcControlPanel.right - margin, yLine2);

                if (hComboSort) {
                    GetWindowRect(hComboSort, &rcEdit);
                    pt = {rcEdit.left, rcEdit.top};
                    ScreenToClient(hwnd, &pt);
                    int yLine3 = pt.y - MulDiv(5, dpi, 96);
                    MoveToEx(hdc, g_rcControlPanel.left + margin, yLine3, NULL);
                    LineTo(hdc, g_rcControlPanel.right - margin, yLine3);
                }

                if (hProgressBar) {
                    GetWindowRect(hProgressBar, &rcEdit);
                    pt = {rcEdit.left, rcEdit.top};
                    ScreenToClient(hwnd, &pt);
                    int yLine4 = pt.y - MulDiv(5, dpi, 96);
                    MoveToEx(hdc, g_rcControlPanel.left + margin, yLine4, NULL);
                    LineTo(hdc, g_rcControlPanel.right - margin, yLine4);
                }
            }

            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(panelBrush);
            DeleteObject(panelPen);
            
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            HDC hdc = GetDC(NULL);
            int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(NULL, hdc);
            
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = MulDiv(450, dpi, 96);
            mmi->ptMinTrackSize.y = MulDiv(550, dpi, 96);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            static HBRUSH hbrStatic = CreateSolidBrush(RGB(36, 36, 36));
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(240, 240, 240));
            SetBkColor(hdcStatic, RGB(36, 36, 36));
            return (INT_PTR)hbrStatic;
        }
        case WM_CTLCOLORLISTBOX: {
            static HBRUSH hbrListbox = CreateSolidBrush(RGB(45, 45, 48));
            HDC hdcListbox = (HDC)wParam;
            SetTextColor(hdcListbox, RGB(240, 240, 240));
            SetBkColor(hdcListbox, RGB(45, 45, 48));
            return (INT_PTR)hbrListbox;
        }
        case WM_CTLCOLOREDIT: {
            static HBRUSH hbrEdit = CreateSolidBrush(RGB(45, 45, 48));
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, RGB(240, 240, 240));
            SetBkColor(hdcEdit, RGB(45, 45, 48));
            return (INT_PTR)hbrEdit;
        }
        case WM_CREATE: {
            // Dark mode titlebar
            BOOL value = TRUE;
            DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &value, sizeof(value));

            // Setup UI Layout
            NONCLIENTMETRICSW ncm = { sizeof(NONCLIENTMETRICSW) };
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
            
            // Adjust font size for DPI (fallback if GetDpiForSystem is not used directly)
            HDC hdc = GetDC(hwnd);
            int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hwnd, hdc);
            
            // Force a slightly larger font for better readability
            ncm.lfMessageFont.lfHeight = -MulDiv(10, dpiY, 72); // 10pt font
            HFONT hFont = CreateFontIndirectW(&ncm.lfMessageFont);

            hListView = CreateWindowExW(0, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
                0, 0, 0, 0, hwnd, (HMENU)ID_LISTVIEW, GetModuleHandle(NULL), NULL);
            SendMessage(hListView, WM_SETFONT, (WPARAM)hFont, TRUE);
            ListView_SetExtendedListViewStyle(hListView, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            SetWindowTheme(hListView, L"DarkMode_Explorer", NULL);
            // Removed to prevent theme interfering with custom draw
            ListView_SetBkColor(hListView, RGB(36, 36, 36));
            ListView_SetTextBkColor(hListView, RGB(36, 36, 36));
            ListView_SetTextColor(hListView, RGB(240, 240, 240));
            SetWindowSubclass(hListView, ListViewSubclassProc, 3, 0);

            hChkSelectAll = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                0, 0, 0, 0, ListView_GetHeader(hListView), (HMENU)ID_CHK_SELECT_ALL, GetModuleHandle(NULL), NULL);

            LVCOLUMNW lvc = {0};
            lvc.mask = LVCF_WIDTH | LVCF_SUBITEM | LVCF_TEXT;
            
            // Column 0: Checkbox
            lvc.cx = 30;
            lvc.pszText = (LPWSTR)L"";
            ListView_InsertColumn(hListView, 0, &lvc);
            
            // Column 1: File Path
            lvc.cx = 500;
            lvc.pszText = (LPWSTR)L"File Path";
            ListView_InsertColumn(hListView, 1, &lvc);

            // Column 2: Size
            lvc.cx = 100;
            lvc.pszText = (LPWSTR)L"Size";
            ListView_InsertColumn(hListView, 2, &lvc);
            
            // Column 3: Lines
            lvc.cx = 100;
            lvc.pszText = (LPWSTR)L"Lines";
            ListView_InsertColumn(hListView, 3, &lvc);

            hBtnAdd = CreateWindowW(L"BUTTON", L"Add Files", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                10, 320, 100, 30, hwnd, (HMENU)ID_BTN_ADD, GetModuleHandle(NULL), NULL);
            SendMessage(hBtnAdd, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBtnRemove = CreateWindowW(L"BUTTON", L"Remove Selected", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                120, 320, 120, 30, hwnd, (HMENU)ID_BTN_REMOVE, GetModuleHandle(NULL), NULL);
            SendMessage(hBtnRemove, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBtnCombine = CreateWindowW(L"BUTTON", L"Combine Selected", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                250, 320, 120, 30, hwnd, (HMENU)ID_BTN_COMBINE, GetModuleHandle(NULL), NULL);
            SendMessage(hBtnCombine, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBtnDedupe = CreateWindowW(L"BUTTON", L"Remove Duplicates", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, (HMENU)ID_BTN_DEDUPE, GetModuleHandle(NULL), NULL);
            SendMessage(hBtnDedupe, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBtnClean = CreateWindowW(L"BUTTON", L"Clean Invalid", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, (HMENU)ID_BTN_CLEAN, GetModuleHandle(NULL), NULL);
            SendMessage(hBtnClean, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBtnScrape = CreateWindowW(L"BUTTON", L"Raw Scrape", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, (HMENU)ID_BTN_SCRAPE, GetModuleHandle(NULL), NULL);
            SendMessage(hBtnScrape, WM_SETFONT, (WPARAM)hFont, TRUE);

            hEditDomain = CreateWindowExW(0, L"EDIT", L"gmail.com", WS_TABSTOP | WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_MULTILINE,
                0, 0, 0, 0, hwnd, (HMENU)ID_EDIT_DOMAIN, GetModuleHandle(NULL), NULL);
            SendMessage(hEditDomain, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBtnExtract = CreateWindowW(L"BUTTON", L"Extract Domain", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, (HMENU)ID_BTN_EXTRACT, GetModuleHandle(NULL), NULL);
            SendMessage(hBtnExtract, WM_SETFONT, (WPARAM)hFont, TRUE);

            hEditSplit = CreateWindowExW(0, L"EDIT", L"10000", WS_TABSTOP | WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_MULTILINE | ES_NUMBER,
                0, 0, 0, 0, hwnd, (HMENU)ID_EDIT_SPLIT, GetModuleHandle(NULL), NULL);
            SendMessage(hEditSplit, WM_SETFONT, (WPARAM)hFont, TRUE);

            hComboSplitMode = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                0, 0, 0, 0, hwnd, (HMENU)ID_COMBO_SPLIT_MODE, GetModuleHandle(NULL), NULL);
            SendMessage(hComboSplitMode, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(hComboSplitMode, CB_ADDSTRING, 0, (LPARAM)L"Lines");
            SendMessageW(hComboSplitMode, CB_ADDSTRING, 0, (LPARAM)L"MB");
            SendMessageW(hComboSplitMode, CB_SETCURSEL, 0, 0);
            hBtnSplit = CreateWindowW(L"BUTTON", L"Split File", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, (HMENU)ID_BTN_SPLIT, GetModuleHandle(NULL), NULL);
            SendMessage(hBtnSplit, WM_SETFONT, (WPARAM)hFont, TRUE);

            hComboSort = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                0, 0, 0, 0, hwnd, (HMENU)ID_COMBO_SORT, GetModuleHandle(NULL), NULL);
            SendMessage(hComboSort, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(hComboSort, CB_ADDSTRING, 0, (LPARAM)L"Alphabetical (A-Z)");
            SendMessageW(hComboSort, CB_ADDSTRING, 0, (LPARAM)L"Alphabetical (Z-A)");
            SendMessageW(hComboSort, CB_ADDSTRING, 0, (LPARAM)L"Length (Shortest to Longest)");
            SendMessageW(hComboSort, CB_ADDSTRING, 0, (LPARAM)L"Length (Longest to Shortest)");
            SendMessageW(hComboSort, CB_ADDSTRING, 0, (LPARAM)L"Randomize (Shuffle)");
            SendMessageW(hComboSort, CB_SETCURSEL, 0, 0);
            SetWindowTheme(hComboSort, L"", L"");
            SetWindowTheme(hComboSplitMode, L"", L"");

            hBtnSort = CreateWindowW(L"BUTTON", L"Sort Files", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, (HMENU)ID_BTN_SORT, GetModuleHandle(NULL), NULL);
            SendMessage(hBtnSort, WM_SETFONT, (WPARAM)hFont, TRUE);

            SetWindowSubclass(hBtnAdd, ButtonSubclassProc, 1, 0);
            SetWindowSubclass(hBtnRemove, ButtonSubclassProc, 2, 0);
            SetWindowSubclass(hBtnCombine, ButtonSubclassProc, 3, 0);
            SetWindowSubclass(hBtnDedupe, ButtonSubclassProc, 4, 0);
            SetWindowSubclass(hBtnClean, ButtonSubclassProc, 5, 0);
            SetWindowSubclass(hBtnScrape, ButtonSubclassProc, 12, 0);
            SetWindowSubclass(hBtnExtract, ButtonSubclassProc, 6, 0);
            SetWindowSubclass(hEditDomain, EditSubclassProc, 7, 0);
            SetWindowSubclass(hEditSplit, EditSubclassProc, 8, 0);
            SetWindowSubclass(hBtnSplit, ButtonSubclassProc, 9, 0);
            SetWindowSubclass(hBtnSort, ButtonSubclassProc, 10, 0);
            SetWindowSubclass(hComboSort, ComboSubclassProc, 11, 0);
            SetWindowSubclass(hComboSplitMode, ComboSubclassProc, 13, 0);

            hProgressBar = CreateWindowExW(0, WC_STATICW, NULL,
                WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                10, 360, 560, 20, hwnd, (HMENU)ID_PROGRESS, GetModuleHandle(NULL), NULL);

            // Enable Drag and Drop
            DragAcceptFiles(hwnd, TRUE);
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_BTN_ADD: OnAddFiles(); break;
                case ID_BTN_REMOVE: OnRemoveFiles(); break;
                case ID_BTN_COMBINE: ExecuteTask(0); break;
                case ID_BTN_DEDUPE: ExecuteTask(1); break;
                case ID_BTN_CLEAN: ExecuteTask(2); break;
                case ID_BTN_EXTRACT: ExecuteTask(3); break;
                case ID_BTN_SPLIT: ExecuteTask(4); break;
                case ID_BTN_SORT: ExecuteTask(5); break;
                case ID_CHK_SELECT_ALL: {
                    bool isChecked = SendMessage(hChkSelectAll, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    int count = ListView_GetItemCount(hListView);
                    for (int i = 0; i < count; ++i) ListView_SetCheckState(hListView, i, isChecked);
                    break;
                }
            }
            return 0;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            UINT numFiles = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
            std::vector<std::wstring> files;
            for (UINT i = 0; i < numFiles; ++i) {
                wchar_t buffer[MAX_PATH];
                if (DragQueryFileW(hDrop, i, buffer, MAX_PATH)) {
                    files.push_back(buffer);
                }
            }
            AddFilesToList(files);
            DragFinish(hDrop);
            return 0;
        }

        case WM_WORKER_PROGRESS: {
            g_nProgress = (int)wParam;
            InvalidateRect(hProgressBar, NULL, TRUE);
            return 0;
        }

        case WM_WORKER_FINISHED: {
            SetUIState(true);
            std::wstring* msg = (std::wstring*)lParam;
            if (wParam == 1) {
                if (msg) {
                    MessageBoxW(hwnd, msg->c_str(), L"Success", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxW(hwnd, L"Task completed successfully!", L"Success", MB_OK | MB_ICONINFORMATION);
                }
            } else {
                MessageBoxW(hwnd, L"Task failed or was cancelled.", L"Error", MB_OK | MB_ICONERROR);
            }
            if (msg) delete msg;
            g_nProgress = 0;
            InvalidateRect(hProgressBar, NULL, TRUE);
            delete currentTask;
            currentTask = nullptr;
            return 0;
        }

        case WM_DESTROY: {
            if (currentTask) {
                currentTask->cancelRequested = true;
                // Give it a moment to cancel if it was running, memory might leak if closed mid-task, but OS cleans it up.
            }
            PostQuitMessage(0);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                HDC hdc = dis->hDC;
                RECT rect = dis->rcItem;
                
                bool isPressed = dis->itemState & ODS_SELECTED;
                bool isHovered = GetPropW(dis->hwndItem, L"Hovered") != NULL;
                
                COLORREF bgCol = isPressed ? RGB(0, 122, 204) : (isHovered ? RGB(62, 62, 66) : RGB(45, 45, 48));
                HBRUSH brush = CreateSolidBrush(bgCol);
                HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
                
                HGDIOBJ oldBrush = SelectObject(hdc, brush);
                HGDIOBJ oldPen = SelectObject(hdc, pen);
                
                Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
                
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(brush);
                DeleteObject(pen);
                
                wchar_t text[256];
                GetWindowTextW(dis->hwndItem, text, 256);
                
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(240, 240, 240));
                
                HFONT hFont = (HFONT)SendMessage(dis->hwndItem, WM_GETFONT, 0, 0);
                HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                
                DrawTextW(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, hOldFont);
                return TRUE;
            } else if (dis->CtlType == ODT_COMBOBOX) {
                HDC hdc = dis->hDC;
                RECT rect = dis->rcItem;
                
                bool isSelected = dis->itemState & ODS_SELECTED;
                COLORREF bgCol = isSelected ? RGB(0, 122, 204) : RGB(45, 45, 48);
                HBRUSH brush = CreateSolidBrush(bgCol);
                FillRect(hdc, &rect, brush);
                DeleteObject(brush);
                
                if (dis->itemID != (UINT)-1) {
                    wchar_t text[256];
                    SendMessageW(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)text);
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, RGB(255, 255, 255));
                    
                    RECT textRect = rect;
                    textRect.left += 5;
                    DrawTextW(hdc, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
                return TRUE;
            } else if (dis->CtlType == ODT_STATIC && dis->hwndItem == hProgressBar) {
                HDC hdc = dis->hDC;
                RECT rect = dis->rcItem;
                
                HBRUSH bgBrush = CreateSolidBrush(RGB(45, 45, 48));
                FillRect(hdc, &rect, bgBrush);
                DeleteObject(bgBrush);
                
                if (g_nProgress > 0) {
                    RECT fillRect = rect;
                    fillRect.right = fillRect.left + (fillRect.right - fillRect.left) * g_nProgress / 100;
                    HBRUSH fillBrush = CreateSolidBrush(RGB(0, 122, 204));
                    FillRect(hdc, &fillRect, fillBrush);
                    DeleteObject(fillBrush);
                }
                
                HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
                HGDIOBJ oldPen = SelectObject(hdc, pen);
                HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(pen);
                
                if (g_nProgress > 0) {
                    wchar_t text[32];
                    wsprintfW(text, L"%d%%", g_nProgress);
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, RGB(255, 255, 255));
                    
                    HFONT hFont = (HFONT)SendMessage(dis->hwndItem, WM_GETFONT, 0, 0);
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    DrawTextW(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    SelectObject(hdc, hOldFont);
                }
                
                return TRUE;
            }
            return 0;
        }

        // Adjust layout on resize
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            
            HDC hdc = GetDC(hwnd);
            int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hwnd, hdc);

            // DPI-scaled metrics
            int padding = MulDiv(15, dpi, 96);
            int innerPadding = MulDiv(10, dpi, 96);
            int btnW = MulDiv(180, dpi, 96); // wider minimum for large text
            int btnH = MulDiv(35, dpi, 96);
            int spacing = MulDiv(10, dpi, 96);
            int progressH = MulDiv(20, dpi, 96);
            int staticH = MulDiv(25, dpi, 96);
            
            // Flow layout calculation
            int panelInnerWidth = width - 2 * padding - 2 * innerPadding;
            int maxCols = std::max(1, (panelInnerWidth + spacing) / (btnW + spacing));
            int cols = std::min(5, maxCols);
            int numBtns = 6;
            int rows = (numBtns + cols - 1) / cols;

            // The total height of the control panel including buttons, separator, extract, split, sort, and progress
            int buttonsHeight = rows * btnH + (rows - 1) * spacing;
            int separatorHeight = spacing; // Spacing for the line
            int extractHeight = btnH;
            int splitHeight = btnH;
            int sortHeight = btnH;
            int totalControlInnerHeight = buttonsHeight + separatorHeight + extractHeight + separatorHeight + splitHeight + separatorHeight + sortHeight + spacing + progressH;
            int controlsHeight = totalControlInnerHeight + innerPadding * 2;
            
            // Set panels
            g_rcControlPanel.left = padding;
            g_rcControlPanel.right = width - padding;
            g_rcControlPanel.bottom = height - padding;
            g_rcControlPanel.top = g_rcControlPanel.bottom - controlsHeight;

            g_rcListPanel.left = padding;
            g_rcListPanel.top = padding;
            g_rcListPanel.right = width - padding;
            g_rcListPanel.bottom = g_rcControlPanel.top - padding;
            
            int listW = (g_rcListPanel.right - g_rcListPanel.left) - innerPadding * 2;
            int chkColW = MulDiv(30, dpi, 96);
            int sizeColW = MulDiv(100, dpi, 96);
            int linesColW = MulDiv(100, dpi, 96);
            int scrollW = GetSystemMetrics(SM_CXVSCROLL);
            
            int pathW = listW - chkColW - sizeColW - linesColW - scrollW - MulDiv(4, dpi, 96);
            
            MoveWindow(hListView, g_rcListPanel.left + innerPadding, g_rcListPanel.top + innerPadding, 
                       listW, 
                       (g_rcListPanel.bottom - g_rcListPanel.top) - innerPadding * 2, TRUE);

            MoveWindow(hChkSelectAll, MulDiv(3, dpi, 96), MulDiv(4, dpi, 96), 
                       chkColW, MulDiv(16, dpi, 96), TRUE);
                       
            ListView_SetColumnWidth(hListView, 0, chkColW);
            ListView_SetColumnWidth(hListView, 1, pathW);
            ListView_SetColumnWidth(hListView, 2, sizeColW);
            ListView_SetColumnWidth(hListView, 3, linesColW);
            
            int buttonsStartY = g_rcControlPanel.top + innerPadding;
            
            int actualBtnW = (panelInnerWidth - (cols - 1) * spacing) / cols;
            if (actualBtnW < btnW) actualBtnW = btnW;

            HWND btns[] = {hBtnAdd, hBtnRemove, hBtnCombine, hBtnDedupe, hBtnClean, hBtnScrape};
            for (int i = 0; i < numBtns; ++i) {
                int c = i % cols;
                int r = i / cols;
                
                int x = g_rcControlPanel.left + innerPadding + c * (actualBtnW + spacing);
                int y = buttonsStartY + r * (btnH + spacing);
                MoveWindow(btns[i], x, y, actualBtnW, btnH, TRUE);
            }

            // Extract Domain Layout
            int extractStartY = buttonsStartY + buttonsHeight + separatorHeight;
            int availableW = (g_rcControlPanel.right - g_rcControlPanel.left) - innerPadding * 2 - spacing;
            int extBtnW = actualBtnW;
            int editW = availableW - extBtnW;

            MoveWindow(hEditDomain, g_rcControlPanel.left + innerPadding, extractStartY, editW, btnH, TRUE);
            MoveWindow(hBtnExtract, g_rcControlPanel.left + innerPadding + editW + spacing, extractStartY, extBtnW, btnH, TRUE);

            // Vertically center text in Edit control using EM_SETRECT
            RECT rcEdit;
            GetClientRect(hEditDomain, &rcEdit);
            HDC hdcEdit = GetDC(hEditDomain);
            HFONT hFont = (HFONT)SendMessage(hEditDomain, WM_GETFONT, 0, 0);
            HFONT hOldFont = (HFONT)SelectObject(hdcEdit, hFont);
            TEXTMETRICW tm;
            GetTextMetricsW(hdcEdit, &tm);
            SelectObject(hdcEdit, hOldFont);
            ReleaseDC(hEditDomain, hdcEdit);

            int textHeight = tm.tmHeight;
            int offset = (rcEdit.bottom - rcEdit.top - textHeight) / 2;
            if (offset > 0) {
                rcEdit.top += offset;
                rcEdit.bottom -= offset;
            }
            rcEdit.left += MulDiv(5, dpi, 96); // Left padding
            rcEdit.right -= MulDiv(5, dpi, 96);
            SendMessage(hEditDomain, EM_SETRECT, 0, (LPARAM)&rcEdit);

            // Split Layout
            int splitStartY = extractStartY + extractHeight + separatorHeight;
                        int comboSplitW = MulDiv(80, dpi, 96);
            MoveWindow(hEditSplit, g_rcControlPanel.left + innerPadding, splitStartY, editW - comboSplitW - spacing, btnH, TRUE);
            MoveWindow(hComboSplitMode, g_rcControlPanel.left + innerPadding + editW - comboSplitW, splitStartY, comboSplitW, btnH * 4, TRUE);
            MoveWindow(hBtnSplit, g_rcControlPanel.left + innerPadding + editW + spacing, splitStartY, extBtnW, btnH, TRUE);

            // Vertically center text in Edit control using EM_SETRECT
            GetClientRect(hEditSplit, &rcEdit);
            offset = (rcEdit.bottom - rcEdit.top - textHeight) / 2;
            if (offset > 0) {
                rcEdit.top += offset;
                rcEdit.bottom -= offset;
            }
            rcEdit.left += MulDiv(5, dpi, 96); // Left padding
            rcEdit.right -= MulDiv(5, dpi, 96);
            SendMessage(hEditSplit, EM_SETRECT, 0, (LPARAM)&rcEdit);

            // Sort Layout
            int sortStartY = splitStartY + splitHeight + separatorHeight;
            
            // Adjust ComboBox internal heights so its collapsed outer bounding box matches btnH exactly
            int currentItemH = btnH - MulDiv(6, dpi, 96);
            SendMessageW(hComboSort, CB_SETITEMHEIGHT, (WPARAM)-1, currentItemH);
            SendMessageW(hComboSplitMode, CB_SETITEMHEIGHT, (WPARAM)-1, currentItemH);
            
            RECT rcCombo;
            GetWindowRect(hComboSort, &rcCombo);
            int currentOuterH = rcCombo.bottom - rcCombo.top;
            
            if (currentOuterH != btnH) {
                currentItemH += (btnH - currentOuterH);
                SendMessageW(hComboSort, CB_SETITEMHEIGHT, (WPARAM)-1, currentItemH);
                SendMessageW(hComboSplitMode, CB_SETITEMHEIGHT, (WPARAM)-1, currentItemH);
            SendMessageW(hComboSplitMode, CB_SETITEMHEIGHT, (WPARAM)-1, currentItemH);
            }
            // Use standard item height for dropdown items to look nice
            SendMessageW(hComboSort, CB_SETITEMHEIGHT, 0, btnH - MulDiv(6, dpi, 96));  
            SendMessageW(hComboSplitMode, CB_SETITEMHEIGHT, 0, btnH - MulDiv(6, dpi, 96));  
            
            // ComboBox overall window height must include drop-down area
            MoveWindow(hComboSort, g_rcControlPanel.left + innerPadding, sortStartY, editW, btnH * 6, TRUE);
            MoveWindow(hBtnSort, g_rcControlPanel.left + innerPadding + editW + spacing, sortStartY, extBtnW, btnH, TRUE);

            int progressY = sortStartY + sortHeight + spacing;
            
            MoveWindow(hProgressBar, g_rcControlPanel.left + innerPadding, progressY, 
                       (g_rcControlPanel.right - g_rcControlPanel.left) - innerPadding * 2, progressH, TRUE);
            
            // Force redraw of panels
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitCommonControls(); // Initialize common controls for modern UI

    const wchar_t CLASS_NAME[] = L"TxtProcessorClass";
    
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(101));

    RegisterClassW(&wc);
    
    HDC hdcScreen = GetDC(NULL);
    int dpi = GetDeviceCaps(hdcScreen, LOGPIXELSY);
    ReleaseDC(NULL, hdcScreen);

    int initW = MulDiv(700, dpi, 96);
    int initH = MulDiv(650, dpi, 96);
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int startX = (screenW - initW) / 2;
    int startY = (screenH - initH) / 2;

    hMainWnd = CreateWindowExW(
        0, CLASS_NAME, L"TextTools",
        WS_OVERLAPPEDWINDOW, startX, startY, initW, initH,
        NULL, NULL, hInstance, NULL);

    if (hMainWnd == NULL) return 0;

    ShowWindow(hMainWnd, nCmdShow);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}







































