#include <stdio.h>
#include <windows.h>
#include <tchar.h>


#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define MAINPROC 1
#define SASPROC 1
#include "uwproc.h"

#include "llama.h"
void* __dso_handle = (void*)&__dso_handle;
ptr OLAPLG U_PARMS((void));

char tempPath[MAX_PATH];
char tempFileName[MAX_PATH];
int debug;

// Thread function to execute a command
DWORD WINAPI ExecuteCommandThread(LPVOID lpParam)
{
	const char* command = (const char*)lpParam;

	// Get the path to the temporary directory
	if (GetTempPath(MAX_PATH, tempPath) == 0) {
		SAS_XPSLOG("Failed to get temp path\n");
		return FALSE;
	}

	// Generate a temporary file name
	if (GetTempFileName(tempPath, "LLM", 0, tempFileName) == 0) {
		SAS_XPSLOG("GetTempFileName failed (%d)\n", GetLastError());
		return FALSE;
	}

	// Open the file for reading
	remove(tempFileName);
   if(debug)
	   SAS_XPSLOG("Temp file name: %s\n", tempFileName);
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	// Create a file to store the output
	HANDLE hFile = CreateFile(tempFileName, FILE_APPEND_DATA, FILE_SHARE_WRITE | FILE_SHARE_READ, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		SAS_XPSLOG("Could not create output file.\n");
		return FALSE;
	}

	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdOutput = hFile;
   if(debug)
	   si.hStdError = hFile;

	BOOL success = CreateProcessA(
		NULL,                   // Application name (use command line instead)
		(LPSTR)command,         // Command line
		NULL,                   // Process handle not inheritable
		NULL,                   // Thread handle not inheritable
		TRUE,                  // Inherit handles
		CREATE_NO_WINDOW,       // Flags (hide console window)
		NULL,                   // Environment block
		NULL,                   // Current directory
		&si,                    // Startup info
		&pi                     // Process info
	);

	if (success) {
		WaitForSingleObject(pi.hProcess, INFINITE); // Wait for process to finish
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(hFile);
	}
	else {
		SAS_XPSLOG("CreateProcess failed (%d).\n", GetLastError());
	}

	return 0;
}


void U_MAIN(ollama)()

{
	ptr outfid;
	ptr pTr, xvputptr;
	struct NAMESTR varName;
	char* pStr;
	TCHAR cmdLine[MAX_PATH * 2] = "llama-cli.exe -m ";

	int n, len;
	const size_t chunkSize = 32767;
	char buffer[32767];
	FILE* file;
	size_t bytesRead;

	int timeout = 0;

	UWPRCC(&proc);

	SAS_XSPARSE(OLAPLG(), NULL, &proc);
	if (proc.error >= XEXITPROBLEM)
		SAS_XEXIT(XEXITSYNTAX, 0);

/* This call to SAS_XPAGE  is necessary to check any OPTIONS */
      /* statements within the PROC call.   */
   SAS_XPAGE();
   debug = SFOPT(proc.head, 1) ? 1 : 0;
   if(debug)
      SAS_XPSLOG("Debug: %d", debug);

   if(debug){
      SAS_XPSLOG("ollama procedure written in C\n");
      SAS_XPSLOG("Compiled using %s on %s at %s.\n", _COMPILER_,
         __DATE__, __TIME__);
   }

	if (SFTYPE(proc.head, 5) != 0) {
		timeout = (int)SFF(proc.head, 5);
      if(debug)
		   SAS_XPSLOG("timeout = %d ms", timeout);
	}

	pStr = SFLD(proc.head, 8);
	n = SAS_ZPARMN((struct STLIST*)pStr);
	if (pStr == NULL || n == 0) {
		SAS_XPSLOG("ERROR: There is no model parameter!");
		goto exitproc;
	}

	pTr = SAS_ZPARM((struct STLIST*)pStr, 0, &len);
	if (len > (MAX_PATH)) {
		SAS_XPSLOG("ERROR: The length of the model path cannot exceed %d.", MAX_PATH);
		goto exitproc;
	}

	strncat(cmdLine, pTr, len);

	pStr = SFLD(proc.head, 7);
	n = SAS_ZPARMN((struct STLIST*)pStr);
	if (pStr != NULL || n > 0) {
		pTr = SAS_ZPARM((struct STLIST*)pStr, 0, &len);
		if (len > (MAX_PATH)) {
			SAS_XPSLOG("ERROR: The length of the param cannot exceed %d.", MAX_PATH);
		}
		else {
			strncat(cmdLine, " ", 1);
			strncat(cmdLine, pTr, len);
		}
	}

   if(debug)
	   SAS_XPSLOG("command = %s", cmdLine);

	HANDLE hThread = CreateThread(
		NULL,                   // Default security attributes
		0,                      // Default stack size
		ExecuteCommandThread,   // Thread function
		(LPVOID)cmdLine,        // Argument to thread function
		0,                      // Default creation flags
		NULL                    // Don't return thread ID
	);

	if (hThread == NULL) {
		SAS_XPSLOG("Failed to create thread (%d).\n", GetLastError());
		goto exitproc;
	}

	// Wait for the command thread to finish (optional)
	if (timeout == 0) {
		WaitForSingleObject(hThread, INFINITE);
	}
	else {
		DWORD waitResult = WaitForSingleObject(hThread, timeout);
		switch (waitResult) {
		case WAIT_OBJECT_0:
			SAS_XPSLOG("Thread finished gracefully.\n");
			break;
		case WAIT_TIMEOUT:
			SAS_XPSLOG("Timeout! Terminating thread...\n");
			if (!TerminateThread(hThread, 1)) { // Exit code 1
				SAS_XPSLOG("TerminateThread failed (%lu)\n", GetLastError());
			}
			break;
		case WAIT_FAILED:
			SAS_XPSLOG("WaitForSingleObject failed (%lu)\n", GetLastError());
			break;
		}
	}

	CloseHandle(hThread);


	// Open the file for reading
	file = fopen(tempFileName, "rb");
	if (file == NULL) {
		SAS_XPSLOG("Failed to open temp file for reading\n");
		goto exitproc;
	}

	// Get the file size
	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	rewind(file);
   if(debug)
	   SAS_XPSLOG("Reading result file, size: %d", fileSize);
	outfid = SFFILE(proc.head, 3);
	if (outfid != NULL) {
		SAS_XVPUTI(outfid, 1, &xvputptr);
		SAS_XVNAMEI(&varName);
		varName.nname = "result  ";
		varName.namelen = 6;
		varName.ntype = 2;
		varName.nlng = chunkSize;
		SAS_XVPUTD(xvputptr, &varName, 0, (ptr)buffer, 0);
		SAS_XVPUTE(xvputptr);
	}

   memset(buffer, 0, chunkSize);
	while ((bytesRead = fread(buffer, 1, chunkSize, file)) > 0) {
		// Process the chunk (for example, print it)
		SAS_XPSLOG("%s", buffer);
		if (outfid != NULL) {
			SAS_XVPUT(xvputptr, NULL);
			SAS_XOADD(outfid, NULL);
		}
      memset(buffer, 0, chunkSize);
	}

	// Close and remove the temporary result file
	fclose(file);
	remove(tempFileName);

exitproc:

	SAS_XEXIT(XEXITNORMAL, 0);
}
