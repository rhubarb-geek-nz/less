/************************************
 * Copyright (c) 2024 Roger Brown.
 * Licensed under the MIT License.
 */

#include <windows.h>
#include <winerror.h>
#include <stdio.h>
#include <stdlib.h>

static void printWin32Error(DWORD err)
{
	char buf[256];
	DWORD len = FormatMessageA(
		FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		err,
		0,
		buf,
		sizeof(buf),
		NULL);

	if (len)
	{
		DWORD dw;
		WriteFile(GetStdHandle(STD_ERROR_HANDLE), buf, len, &dw, NULL);
	}
}

static void copyFileContent(HANDLE hSrc, HANDLE hDest)
{
	char buf[4096];
	DWORD dw;

	while (ReadFile(hSrc, buf, sizeof(buf), &dw, NULL))
	{
		DWORD off = 0;
		const char* p = buf;

		if (!dw) break;

		while (dw)
		{
			DWORD len = 0;

			if (WriteFile(hDest, p, dw, &len, NULL))
			{
				if (len)
				{
					dw -= len;
					p += len;
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
	}
}

int wmain(int argc, wchar_t** argv)
{
	wchar_t tmpPath[MAX_PATH], tmpFile[MAX_PATH], cmdLine[MAX_PATH + MAX_PATH];
	wchar_t executable[MAX_PATH];
	UINT ui;
	HANDLE hTmp, hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	const wchar_t* editor = _wgetenv(L"EDITOR");
	BOOL bStdInConsole = GetConsoleMode(hStdin, &mode);
	int exitCode = 0;
	DWORD len;

	if (bStdInConsole && argc <= 1)
	{
		printWin32Error(ERROR_INVALID_PARAMETER);

		return ERROR_INVALID_PARAMETER;
	}

	if (editor == NULL)
	{
		editor = L"notepad";
	}

	if ((!wcschr(editor, '/')) && !wcschr(editor, '\\'))
	{
		const wchar_t* path = _wgetenv(L"PATH");

		while (path)
		{
			const wchar_t* semicolon = wcschr(path, ';');
			size_t len = semicolon ? semicolon - path : wcslen(path);

			if (len > 0)
			{
				DWORD attr;

				memcpy_s(executable, sizeof(executable), path, len << 1);

				executable[len++] = '\\';

				wcscpy_s(executable + len, _countof(executable) - len, editor);

				if (!wcschr(editor, '.'))
				{
					wcscat_s(executable, _countof(executable), L".exe");
				}

				attr = GetFileAttributesW(executable);

				if ((attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY))
				{
					editor = executable;
					break;
				}
			}

			if (semicolon)
			{
				path = semicolon + 1;
			}
			else
			{
				path = NULL;
			}
		}
	}

	len = GetTempPathW(_countof(tmpPath), tmpPath);

	if (!len)
	{
		DWORD dw = GetLastError();

		printWin32Error(dw);

		return dw;
	}

	ui = GetTempFileNameW(tmpPath, L"less", 0, tmpFile);

	if (!ui)
	{
		DWORD dw = GetLastError();

		printWin32Error(dw);

		return dw;
	}

	hTmp = CreateFileW(tmpFile,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_TEMPORARY,
		NULL);

	if (hTmp == INVALID_HANDLE_VALUE)
	{
		DWORD dw = GetLastError();
		printWin32Error(dw);
		return dw;
	}

	if (!bStdInConsole)
	{
		copyFileContent(hStdin, hTmp);
	}

	if (argc > 1)
	{
		int i = 1;

		while (i < argc)
		{
			const wchar_t* fileName = argv[i++];
			HANDLE hFile = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

			if (hFile == INVALID_HANDLE_VALUE)
			{
				DWORD dw = GetLastError();
				printWin32Error(dw);
				exitCode = dw;
			}

			copyFileContent(hFile, hTmp);

			CloseHandle(hFile);

			if (exitCode)
			{
				break;
			}
		}
	}

	CloseHandle(hTmp);

	__try
	{
		if (exitCode == 0)
		{
			STARTUPINFOW startup;
			PROCESS_INFORMATION info;

			ZeroMemory(&startup, sizeof(startup));

			startup.cb = sizeof(startup);

			startup.hStdInput = hStdin;
			startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
			startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			startup.dwFlags = STARTF_USESTDHANDLES;

			swprintf_s(cmdLine, _countof(cmdLine), L"\"%s\" \"%s\"", editor, tmpFile);

			if (startup.dwFlags && !bStdInConsole)
			{
				HANDLE pid = GetCurrentProcess();
				const wchar_t* fileName = L"CONIN$";

				hStdin = CreateFileW(fileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

				if (hStdin == INVALID_HANDLE_VALUE)
				{
					exitCode = GetLastError();
					printWin32Error(exitCode);
				}
				else
				{
					if (!DuplicateHandle(pid, hStdin, pid,
						&startup.hStdInput,
						0, TRUE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE))
					{
						exitCode = GetLastError();
						printWin32Error(exitCode);
					}
				}
			}

			if (exitCode == 0)
			{
				if (CreateProcessW(editor, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &info))
				{
					DWORD dw;

					WaitForSingleObject(info.hProcess, INFINITE);

					if (GetExitCodeProcess(info.hProcess, &dw))
					{
						exitCode = dw;
					}
					else
					{
						exitCode = GetLastError();
					}

					CloseHandle(info.hProcess);
					CloseHandle(info.hThread);
				}
				else
				{
					DWORD dw = GetLastError();
					exitCode = dw;
					printWin32Error(exitCode);
				}
			}
		}
	}
	__finally
	{
		if (!DeleteFileW(tmpFile))
		{
			DWORD dw = GetLastError();
			exitCode = dw;
			printWin32Error(exitCode);
		}
	}

	return exitCode;
}
