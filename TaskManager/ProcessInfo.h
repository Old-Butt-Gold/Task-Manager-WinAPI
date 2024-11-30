#pragma once

#include <Windows.h>
#include <TlHelp32.h> 
#include <Psapi.h>   
#include <map>
#include <sstream>
#include <set>
#include <shlwapi.h>
#include "dwmapi.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")

void CloseProcessById(DWORD processId);

void OpenProcessLocation(DWORD processId);

std::wstring GetPriority(DWORD processId);

void ChangeIcon(DWORD processID, HWND hListView, LVITEM& lvi, std::map<std::wstring, int>& iconMap);

bool Is64BitProcess(DWORD processID);

std::wstring GetProcessTimeCreation(DWORD processId);

std::wstring FormatFloat(int precision, double value);

std::wstring GetMemoryUsage(DWORD processID);

int FindProcessInListViewByPID(HWND hListView, DWORD processID);

void RemoveStaleProcesses(HWND hListView, const std::set<DWORD>& existingProcesses);

std::wstring GetCpuUsage(DWORD processID);

void PopulateProcessListView(HWND hListView, std::map<std::wstring, int>& iconMap);