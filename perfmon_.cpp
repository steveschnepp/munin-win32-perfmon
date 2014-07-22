#include "stdafx.h"
#include <iostream>

using namespace std;

const size_t BUFF_SIZE = 4 * 1024 * 1024; // 4MiB

vector<wstring> convertToVectOfString(_TCHAR* str, size_t len) {
		vector<wstring> strings;
		_TCHAR* s = str;
		for (size_t i = 0; i < len; i ++) {
			if (str[i] != 0) {
				continue;
			}
			auto currentString = wstring(s);
			if (currentString.size() == 0) break;
			strings.push_back(currentString);
			s = str + i + 1;
		}
		return strings;
}

#define _CRT_SECURE_NO_WARNINGS

void dumpPdhObjects();


int _tmain(int argc, _TCHAR* argv[], _TCHAR* envp[])
{
	if (argc >= 2 && wcscmp(argv[1], L"list") == 0 )  {
		// Send all the counter paths.
		dumpPdhObjects();
		return 0;
	}

	_TCHAR *counter_path = _wgetenv(L"counter_path");
	if (counter_path == NULL) {
		wprintf(L"No counter configured\n");
		return -1;
	}

	if (argc >= 2 && wcscmp(argv[1], L"config") == 0 )  {
		// Send the config
		wprintf(L"graph_title \n");
		wprintf(L"field.label %s\n", counter_path);
		return 0;
	}

	_TCHAR *counter_type = _wgetenv(L"counter_type");

	PDH_STATUS Status;
    HQUERY Query = NULL;
    HCOUNTER Counter;
    PDH_FMT_COUNTERVALUE DisplayValue;
    DWORD CounterType;

    // Create a query.
    Status = PdhOpenQuery(NULL, NULL, &Query);
    if (Status != ERROR_SUCCESS) {
       wprintf(L"PdhOpenQuery failed with status 0x%x.\n", Status);
       goto Cleanup;
    }

	// Add the selected counter to the query.
    Status = PdhAddCounter(Query, counter_path, 0, &Counter);
    if (Status != ERROR_SUCCESS) {
        wprintf(L"PdhAddCounter failed with status 0x%x.\n", Status);
        goto Cleanup;
    }

	// Compute a displayable value for the counter.
    Status = PdhGetFormattedCounterValue(Counter, PDH_FMT_DOUBLE, &CounterType, &DisplayValue);
	if (Status != ERROR_SUCCESS) {
		// The first get() failed. This is expected for derived counters
		// So we have to try, wait a little and retry.
		// see the Remarks section of
		// http://msdn.microsoft.com/en-us/library/windows/desktop/aa372637(v=vs.85).aspx
		Status = PdhCollectQueryData(Query);
		if (Status != ERROR_SUCCESS) {
			wprintf(L"PdhCollectQueryData failed with 0x%x.\n", Status);
			goto Cleanup;
		}
	    Sleep(1000);
		Status = PdhCollectQueryData(Query);
		if (Status != ERROR_SUCCESS) {
			wprintf(L"PdhCollectQueryData failed with 0x%x.\n", Status);
			goto Cleanup;
		}
	    Status = PdhGetFormattedCounterValue(Counter, PDH_FMT_DOUBLE, &CounterType, &DisplayValue);
		if (Status != ERROR_SUCCESS) {
			// Now it *really* fails.
			wprintf(L"PdhGetFormattedCounterValue failed with status 0x%x.", Status);
			goto Cleanup;
		}
	}


	wprintf(L"counter.value %.20g\n", DisplayValue.doubleValue);

Cleanup:
	// Close the query.
	if (Query) {
		PdhCloseQuery(Query);
	}
}

void dumpEnv(_TCHAR* envp[]) {
	_TCHAR** env = envp;
	while(*env) {
		wprintf(L"%s\n", *env);
		env ++;
	}
}

void dumpPdhObjects()
{
	PDH_STATUS Status;

	vector<wstring> objects;
	{
		DWORD len = BUFF_SIZE;
		PZZWSTR objectList = (PZZWSTR) malloc(len);
		Status = PdhEnumObjects(0, 0, objectList, &len, PERF_DETAIL_WIZARD, false);
		if (Status != ERROR_SUCCESS) {
			wprintf(L"PdhEnumObjects failed with status 0x%x.", Status);
			return;
		}
		objects = convertToVectOfString(objectList, len);
		free(objectList);
	}

	for (auto i = objects.begin(); i != objects.end(); i ++) {
		wstring object = *i;
		DWORD lenCounterList = BUFF_SIZE;
		PZZWSTR counterList = (PZZWSTR) malloc(lenCounterList);
		DWORD lenInstanceList = BUFF_SIZE;
		PZZWSTR instanceList = (PZZWSTR) malloc(lenInstanceList);
		Status = PdhEnumObjectItems(0, 0, object.c_str(), counterList, &lenCounterList, instanceList, &lenInstanceList, PERF_DETAIL_WIZARD, true);
		if (Status != ERROR_SUCCESS) {
			wprintf(L"PdhEnumObjectItems failed with status 0x%x.", Status);
			return;
		}
		
		if (lenInstanceList == 0) {
			auto counters = convertToVectOfString(counterList, lenCounterList);
			for (auto counterIterator = counters.begin(); counterIterator != counters.end(); counterIterator++) {
				wstring counterAsStr;
				counterAsStr +=  wstring(L"\\") + object;
				counterAsStr +=  wstring(L"\\") + (*counterIterator);
				wcout << counterAsStr << endl;
			}
		} else {
			auto instances = convertToVectOfString(instanceList, lenInstanceList);
			for (auto instanceIterator = instances.begin(); instanceIterator != instances.end(); instanceIterator++) {
				auto counters = convertToVectOfString(counterList, lenCounterList);
				for (auto counterIterator = counters.begin(); counterIterator != counters.end(); counterIterator++) {
					wstring counterAsStr;
					counterAsStr +=  wstring(L"\\") + object;
					counterAsStr +=  wstring(L"(") + (*instanceIterator) + wstring(L")");
					counterAsStr +=  wstring(L"\\") + (*counterIterator);
					wcout << counterAsStr << endl;
				}
			}
		}

		free(counterList);
		free(instanceList);
	}
}
