﻿#include "stdafx.h"
#include <iostream>

struct perf_counter  {
	_TCHAR *FieldName;
	_TCHAR *CounterPath;
	HCOUNTER Handle;
};

struct perf_counters {
	size_t len;
	struct perf_counter *counters;
};

void dumpPdhObjects();
void getHumanReadableError(DWORD dwErrorCode);
void computeFieldName(struct perf_counter *counter);

size_t addToPerfCounters(struct perf_counters *perf_counters, struct perf_counter counter) 
{
	// Very naive way of allocating a dynamic array
	// But KISS should work well enough here.

	size_t new_len = perf_counters->len + 1;
	struct perf_counter *new_counters = (struct perf_counter *) realloc(perf_counters->counters, new_len * sizeof(struct perf_counter));
	perf_counters->counters = new_counters; 
	perf_counters->len = new_len;
	perf_counters->counters[new_len - 1] = counter;
	return new_len;
}

int _tmain(int argc, _TCHAR* argv[], _TCHAR* envp[])
{
	PDH_STATUS Status;

	if (argc >= 2 && wcscmp(argv[1], L"list") == 0 )  {
		// Send all the counter paths.
		dumpPdhObjects();
		return 0;
	}

	struct perf_counters counters;
	counters.counters = NULL;
	counters.len = 0;

	DWORD PathListLength = 0;
    HQUERY Query = NULL;

	// We expand the given CounterPath to a number of fields
	// There is a method to use english counter names, see
	// http://support.microsoft.com/kb/287159/en
	_TCHAR* szWildCardPath = _wgetenv(L"WildCardPath");
	// Get the size to alloc
	Status = PdhExpandWildCardPath(0, szWildCardPath, 0, &PathListLength, 0);
	if (Status != PDH_MORE_DATA) {
		wprintf(L"PdhExpandWildCardPath failed with status 0x%x.", Status);
		getHumanReadableError(Status);
        goto Cleanup;
	}

	// MSDN says "You must add one to the required size on Windows XP", 
	// so we just always add 1 for safety 
	PathListLength += 1;

	//		wprintf(L"Allocation of %u bytes for objectList \n", len * sizeof(_TCHAR));
	PZZWSTR mszExpandedPathList = (PZZWSTR) malloc(PathListLength * sizeof(_TCHAR));

	Status = PdhExpandWildCardPath(0, szWildCardPath, mszExpandedPathList, &PathListLength, 0);
	if (Status != ERROR_SUCCESS) {
		wprintf(L"PdhExpandWildCardPath failed with status 0x%x.", Status);
		getHumanReadableError(Status);
        goto Cleanup;
	}

	for (PZZWSTR Current = mszExpandedPathList; *Current != 0; Current += wcslen(Current) + 1) {
		struct perf_counter counter;
		
		counter.CounterPath = Current;
		
		// Compute FieldName
		computeFieldName(&counter);

		addToPerfCounters(&counters, counter);
	}

	_TCHAR *graph_title = _wgetenv(L"graph_title");
	if (graph_title == 0) {
		graph_title = szWildCardPath;
	}

	if (argc >= 2 && wcscmp(argv[1], L"config") == 0 )  {
		// Send the config
		wprintf(L"graph_title %s\n", graph_title);

		for (size_t i = 0; i < counters.len; i ++) {
			struct perf_counter* c = counters.counters + i;
			wprintf(L"%s.label %s\n", c->FieldName, c->CounterPath);
		}

		return 0;
	}

    // Create a query.
    Status = PdhOpenQuery(NULL, NULL, &Query);
    if (Status != ERROR_SUCCESS) {
       wprintf(L"PdhOpenQuery failed with status 0x%x.\n", Status);
       goto Cleanup;
    }

	// Add all the selected counters to the query.
	for (size_t i = 0; i < counters.len; i ++) {
		struct perf_counter* c = counters.counters + i;

		Status = PdhAddCounter(Query, c->CounterPath, 0, &(c->Handle));
		if (Status != ERROR_SUCCESS) {
			wprintf(L"PdhAddCounter failed with status 0x%x.\n", Status);
			goto Cleanup;
		}
	}

	Status = PdhCollectQueryData(Query);
	if (Status != ERROR_SUCCESS) {
		wprintf(L"PdhCollectQueryData failed with 0x%x.\n", Status);
		goto Cleanup;
	}

	// We just wait for 1 second, in order to work with deriving counters
	DWORD sleepInMs = 1 * 1000;
	Sleep(sleepInMs);

	Status = PdhCollectQueryData(Query);
	if (Status != ERROR_SUCCESS) {
		wprintf(L"PdhCollectQueryData failed with 0x%x.\n", Status);
		goto Cleanup;
	}

	// Fetch the selected counters to the query.
	for (size_t i = 0; i < counters.len; i ++) {
		struct perf_counter* c = counters.counters + i;

		PDH_FMT_COUNTERVALUE DisplayValue;
		DWORD CounterType;

		Status = PdhGetFormattedCounterValue(c->Handle, PDH_FMT_DOUBLE, &CounterType, &DisplayValue);
		if (Status != ERROR_SUCCESS) {
			wprintf(L"PdhGetFormattedCounterValue failed with status 0x%x.", Status);
			goto Cleanup;
		}

		wprintf(L"%s.value %.20g\n", c->FieldName, DisplayValue.doubleValue);
	}

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

	DWORD lenObjectList = 0;

	// Get the size to alloc
	Status = PdhEnumObjects(0, 0, 0, &lenObjectList, PERF_DETAIL_WIZARD, false);
	if (Status != PDH_MORE_DATA) {
		wprintf(L"PdhEnumObjects failed with status 0x%x.", Status);
		getHumanReadableError(Status);
		return;
	}

	//		wprintf(L"Allocation of %u bytes for objectList \n", len * sizeof(_TCHAR));
	PZZTSTR objectList = (PZZTSTR) malloc(lenObjectList * sizeof(_TCHAR));

	Status = PdhEnumObjects(0, 0, objectList, &lenObjectList, PERF_DETAIL_WIZARD, false);
	if (Status != ERROR_SUCCESS) {
		wprintf(L"PdhEnumObjects failed with status 0x%x.", Status);
		getHumanReadableError(Status);
		return;
	}

	for (PZZWSTR objectListCurrent = objectList; *objectListCurrent != 0; objectListCurrent += wcslen(objectListCurrent) + 1) {
		DWORD lenCounterList = 0;
		DWORD lenInstanceList = 0;
		
		Status = PdhEnumObjectItems(0, 0, objectListCurrent, 0, &lenCounterList, 0, &lenInstanceList, PERF_DETAIL_WIZARD, true);
		if (Status != PDH_MORE_DATA) {
			wprintf(L"PdhEnumObjectItems 1 failed with status 0x%x.", Status);
			getHumanReadableError(Status);
			return;
		}

//		wprintf(L"Allocation of %u bytes for counterList \n", lenCounterList * sizeof(_TCHAR));
		PZZWSTR counterList = (PZZWSTR) malloc(lenCounterList * sizeof(_TCHAR));
//		wprintf(L"Allocation of %u bytes for instanceList \n", lenInstanceList * sizeof(_TCHAR));
		PZZWSTR instanceList = (PZZWSTR) malloc(lenInstanceList * sizeof(_TCHAR));

		Status = PdhEnumObjectItems(0, 0, objectListCurrent, counterList, &lenCounterList, instanceList, &lenInstanceList, PERF_DETAIL_WIZARD, true);
		if (Status != ERROR_SUCCESS) {
			wprintf(L"PdhEnumObjectItems 2 failed with status 0x%x.", Status);
			getHumanReadableError(Status);
			return;
		}
		
		if (lenInstanceList == 0) {
			// Walk the counters list. The list can contain one
            // or more null-terminated strings. The list is terminated
            // using two null-terminator characters.
            for (PZZWSTR counterListCurrent = counterList; *counterListCurrent != 0; counterListCurrent += wcslen(counterListCurrent) + 1) {
				wprintf(L"\\%s\\%s\n",  objectListCurrent, counterListCurrent);
			}
		} else {
			// Same as before, but both
			for (PZZWSTR instanceListCurrent = instanceList; *instanceListCurrent != 0; instanceListCurrent += wcslen(instanceListCurrent) + 1) {
				 for (PZZWSTR counterListCurrent = counterList; *counterListCurrent != 0; counterListCurrent += wcslen(counterListCurrent) + 1) {
					wprintf(L"\\%s(%s)\\%s\n",  objectListCurrent, instanceListCurrent, counterListCurrent);
				 }
			}
		}

//		wprintf(L"Free of counterList \n");
		free(counterList);
		
//		wprintf(L"Free of instanceList \n");
		free(instanceList);
	}

	free(objectList);
}

void computeFieldName(struct perf_counter *counter) {
	// Compute a sortof CRC, to generate the FieldName.
	// It is not crypto-secure, but we don't care about _intentional_ collisions.
	DWORD sortofCRC = 0;
	_TCHAR* current = counter->CounterPath;
	while(*current) {
		sortofCRC ^= (DWORD) (*current) + 7 * (sortofCRC << 1);
		current++;
	}

	// Max is 7 chars for fields
	_TCHAR* FieldName = (_TCHAR*) malloc(8 * sizeof(_TCHAR));
	wsprintf(FieldName, L"f_%x", sortofCRC);
	counter->FieldName = FieldName;
}

#include <pdhmsg.h>

void getHumanReadableError(DWORD dwErrorCode) {
HANDLE hPdhLibrary = NULL;
    LPWSTR pMessage = NULL;
    DWORD_PTR pArgs[] = { (DWORD_PTR)L"<collectionname>" };
 
    hPdhLibrary = LoadLibrary(L"pdh.dll");
    if (NULL == hPdhLibrary)
    {
        wprintf(L"LoadLibrary failed with %lu\n", GetLastError());
        return;
    }

    // Use the arguments array if the message contains insertion points, or you
    // can use FORMAT_MESSAGE_IGNORE_INSERTS to ignore the insertion points.

    if (!FormatMessage(FORMAT_MESSAGE_FROM_HMODULE |
                       FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       /*FORMAT_MESSAGE_IGNORE_INSERTS |*/
                       FORMAT_MESSAGE_ARGUMENT_ARRAY,
                       hPdhLibrary, 
                       dwErrorCode,
                       0,  
                       (LPWSTR)&pMessage, 
                       0, 
                       //NULL))
                       (va_list*)pArgs))
    {
        wprintf(L"Format message failed with 0x%x\n", GetLastError());
        return;
    }

    wprintf(L"Formatted message: %s\n", pMessage);
    LocalFree(pMessage);
}
