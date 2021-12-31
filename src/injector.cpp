#include <windows.h>

#include <cstdio>
#include <sstream>
#include <string>

#include "silkroad.h"

int main(int argc, char *argv[])
{
  if (argc < 3)
  {
    return -1;
  }

  SilkroadData data;
  if (!LoadData(argv[1], data)) {
    return 0;
  }

  std::stringstream cmd;
  cmd << argv[1] << "\\sro_client.exe 0 /" << (int)data.divInfo.locale << " 0 0";

  STARTUPINFOA si = {0};
  PROCESS_INFORMATION pi = {0};
  si.cb = sizeof(STARTUPINFOA);

  bool result =
      (0 != CreateProcessA(0, (LPSTR)cmd.str().c_str(), 0, NULL, FALSE,
                           CREATE_SUSPENDED, NULL, argv[1], &si, &pi));
  if (result == false)
  {
    MessageBoxA(0, "Could not start \"sro_client.exe\".", "Fatal Error",
                MB_ICONERROR);
    return 0;
  }

  // Fire a signal to the dll to do something spical.
  // in our case, it will redirect the connection of the GatewayServer.
  // but this injector was made to be really general so it can be used for any perpose.
  if (argc == 4)
  {
    // Create a bytes array to hold the Process ID but treating it as string
    // that a cheat to avoid the need for a conversion
    char pid[5] = {0};
    memcpy(&pid, &pi.dwProcessId, sizeof(DWORD));

    // Create Semaphore with the Process ID as a name
    auto semaphore = CreateSemaphoreA(NULL, 0, 1, (LPCSTR)&pid);
  }

  // Get full path of our dll
  char fullDllName[1024];
  if (GetFullPathName((LPCSTR)argv[2], sizeof(fullDllName), fullDllName,
                      nullptr) == 0)
  {
    return false;
  }

  // Open process using pid
  auto handle = OpenProcess(PROCESS_ALL_ACCESS, false, pi.dwProcessId);
  if (handle == NULL)
  {
    return false;
  }

  // Get the address to the function LoadLibraryA in kernel32.dll
  auto LoadLibAddr =
      (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
  if (LoadLibAddr == NULL)
  {
    return false;
  }

  // Allocate memory inside the opened process
  auto dereercomp = VirtualAllocEx(handle, NULL, strlen(fullDllName),
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (dereercomp == NULL)
  {
    return false;
  }

  // Write the DLL name to the allocated memory
  if (!WriteProcessMemory(handle, dereercomp, fullDllName, strlen(fullDllName),
                          NULL))
  {
    return false;
  }

  // Create a thread in the opened process
  auto remoteThread =
      CreateRemoteThread(handle, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibAddr,
                         dereercomp, 0, NULL);
  if (remoteThread == NULL)
  {
    return false;
  }

  // Wait until thread have started (or stopped?)
  WaitForSingleObject(remoteThread, INFINITE);

  // Free the allocated memory
  VirtualFreeEx(handle, dereercomp, strlen(fullDllName), MEM_RELEASE);

  // Close the handles
  CloseHandle(remoteThread);
  CloseHandle(handle);

  // Resume the process
  ResumeThread(pi.hThread);
  ResumeThread(pi.hProcess);

  return pi.dwProcessId;
}