#include <cstdio>
#include <winsock2.h>
#include <iphlpapi.h>

#include <Windows.h>

#include "detours.h"
#include "silkroad.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "detours.lib")

static SilkroadData data;
static WSADATA wsaData = {0};

// Checks if the injector fired a spical signal, to know if the loader
// should redirect the GatewayServer connection.
BOOL IsSignalFired()
{
  DWORD currentProcessID = GetCurrentProcessId();

  // Create a bytes array to hold the Process ID but treating it as string
  // that a cheat to avoid the need for a conversion
  char pid[5] = {0};
  memcpy(&pid, &currentProcessID, sizeof(DWORD));

  // Create Semaphore with the Process ID as a name
  HANDLE semaphore = CreateSemaphoreA(NULL, 0, 1, (LPCSTR)&pid);

  // Close the Semaphore
  CloseHandle(semaphore);

  return GetLastError() == ERROR_ALREADY_EXISTS;
}

// original functions.
int(WINAPI *pConnect)(SOCKET, const sockaddr *, int) = connect;
int(WINAPI *pBind)(SOCKET, const sockaddr *, int) = bind;
DWORD(WINAPI *pGetAdaptersInfo)
(PIP_ADAPTER_INFO, PULONG) = GetAdaptersInfo;
HANDLE(WINAPI *pCreateMutexA)
(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR) = CreateMutexA;
HANDLE(WINAPI *pCreateSemaphoreA)
(LPSECURITY_ATTRIBUTES, LONG, LONG, LPCSTR) = CreateSemaphoreA;

// detour function
int WINAPI MyConnect(SOCKET s, const sockaddr *name, int len)
{
  auto *my_name = reinterpret_cast<sockaddr_in *>(const_cast<sockaddr *>(name));

  printf("[connect] ip: %s, port: %d, len: %d\n", inet_ntoa(my_name->sin_addr), htons(my_name->sin_port), len);

  for (auto &div : data.divInfo.divisions)
  {
    for (auto &addr : div.addresses)
    {
      struct in_addr maddr;
      maddr.s_addr = *(u_long *)gethostbyname(addr.c_str())->h_addr_list[0];
      if (strcmp(inet_ntoa(my_name->sin_addr), inet_ntoa(maddr)) == 0 && htons(my_name->sin_port) == data.gatePort)
      {

        my_name->sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
        my_name->sin_port = htons(static_cast<u_short>(GetCurrentProcessId()));

        printf("[connect] redirected gateway\n");
      }
    }
  }

  return pConnect(s, reinterpret_cast<const sockaddr *>(my_name), len);
}

int WINAPI MyBind(SOCKET s, const sockaddr *addr, int namelen)
{
  auto *my_addr = (sockaddr_in *)addr;
  printf("[bind] ip: %s, port: %d, len: %d\n", inet_ntoa(my_addr->sin_addr), htons(my_addr->sin_port), namelen);

  if (addr && namelen == 16)
  {
    if (my_addr->sin_port == ntohs(data.gatePort))
    {
      printf("[bind] blocked\n");
      return 0;
    }
  }
  return pBind(s, addr, namelen);
}

DWORD WINAPI MyGetAdaptersInfo(PIP_ADAPTER_INFO pAdapterInfo, PULONG pOutBufLen)
{
  DWORD dwResult = pGetAdaptersInfo(pAdapterInfo, pOutBufLen);
  printf("[GetAdaptersInfo] result: %d\n", dwResult);
  if (dwResult == ERROR_SUCCESS)
  {
    PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
    srand(__rdtsc() & 0xFFFFFFFF);
    while (pAdapter)
    {
      for (UINT i = 1; i < pAdapter->AddressLength; i++)
      {
        printf("%.2X -> ", pAdapter->Address[i]);
        pAdapter->Address[i] = rand() % 256; // NOLINT(cert-msc50-cpp)
        printf("%.2X ", pAdapter->Address[i]);
      }
      printf("\n");
      pAdapter = pAdapter->Next;
    }
  }
  return dwResult;
}

HANDLE WINAPI MyCreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName)
{
  printf("[CreateMutexA] name: %s\n", lpName);

  if (lpName && strcmp(lpName, "Silkroad Client") == 0)
  {
    char newName[128] = {0};
    _snprintf_s(newName,
                sizeof(newName),
                sizeof(newName) / sizeof(newName[0]) - 1,
                "Silkroad Client_%d",
                0xFFFFFFFF & __rdtsc());

    printf("[CreateMutexA] rename: %s\n", newName);

    return pCreateMutexA(lpMutexAttributes, bInitialOwner, newName);
  }

  return pCreateMutexA(lpMutexAttributes, bInitialOwner, lpName);
}

HANDLE WINAPI MyCreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
                                 LONG lInitialCount,
                                 LONG lMaximumCount,
                                 LPCSTR lpName)
{
  printf("[CreateSemaphoreA] name: %s\n", lpName);

  if (lpName && strcmp(lpName, "Global\\Silkroad Client") == 0)
  {
    char newName[128] = {0};
    _snprintf_s(newName,
                sizeof(newName),
                sizeof(newName) / sizeof(newName[0]) - 1,
                "Global\\Silkroad Client_%d",
                0xFFFFFFFF & __rdtsc());

    printf("[CreateSemaphoreA] rename: %s\n", newName);

    return pCreateSemaphoreA(lpSemaphoreAttributes, lInitialCount, lMaximumCount, newName);
  }

  return pCreateSemaphoreA(lpSemaphoreAttributes, lInitialCount, lMaximumCount, lpName);
}

extern "C" _declspec(dllexport) BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  if (DetourIsHelperProcess())
  {
    return true;
  }

  if (ul_reason_for_call == DLL_PROCESS_ATTACH)
  {
#ifdef _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONIN$", "r", stdin);
#endif

    printf("dll attached\n");
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    LoadData(".", data);

    pCreateMutexA(nullptr, 0, "Silkroad Online Launcher");
    pCreateMutexA(nullptr, 0, "Ready");

    DetourRestoreAfterWith();
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (IsSignalFired())
    {
      DetourAttach(&(PVOID &)pConnect, MyConnect);
    }
    DetourAttach(&(PVOID &)pBind, MyBind);
    DetourAttach(&(PVOID &)pGetAdaptersInfo, MyGetAdaptersInfo);
    DetourAttach(&(PVOID &)pCreateMutexA, MyCreateMutexA);
    DetourAttach(&(PVOID &)pCreateSemaphoreA, MyCreateSemaphoreA);
    if (DetourTransactionCommit() == NO_ERROR)
    {
      printf("detours injected successfully\n");
    }
  }
  else if (ul_reason_for_call == DLL_PROCESS_DETACH)
  {
    printf("dll detached\n");
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    //DetourDetach(&(PVOID &)pConnect, MyConnect);
    DetourDetach(&(PVOID &)pBind, MyBind);
    DetourDetach(&(PVOID &)pGetAdaptersInfo, MyGetAdaptersInfo);
    DetourDetach(&(PVOID &)pCreateMutexA, MyCreateMutexA);
    DetourDetach(&(PVOID &)pCreateSemaphoreA, MyCreateSemaphoreA);
    if (DetourTransactionCommit() == NO_ERROR)
    {
      printf("detours removed successfully\n");
    }

    WSACleanup();
  }

  return true;
}