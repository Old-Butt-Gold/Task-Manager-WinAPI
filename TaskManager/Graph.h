#pragma once
#include "Constants.h"
#include <Windows.h>
#include <sstream>
#include <vector>

typedef struct system_processor_times {
    ULONGLONG IdleTime;
    ULONGLONG KernelTime;
    ULONGLONG UserTime;
    ULONGLONG DpcTime;
    ULONGLONG InterruptTime;
    ULONG     InterruptCount;
} SYSTEM_PROCESSOR_TIMES;

typedef NTSTATUS (NTAPI *ZwQuerySystemInformationFunc)(
    ULONG SystemInformationClass, 
    PVOID SystemInformation, 
    ULONG SystemInformationLength, 
    PULONG ReturnLength
);

int GetCurrentRAMUsage();

int GetCurrentCPUUsage(SYSTEM_INFO sysInfo, SYSTEM_PROCESSOR_TIMES* CurrentSysProcTimes, SYSTEM_PROCESSOR_TIMES* PreviousSysProcTimes, ZwQuerySystemInformationFunc ZwQuerySystemInformation);

std::wstring GetSystemUptime();

std::wstring GetMemoryInfo();

void DrawGraph(HWND hPerfomance, std::vector<int>& history, const int maxHistory);

void PushValue(std::vector<int>& history, const int maxHistory, int newValue);