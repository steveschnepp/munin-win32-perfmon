#include "stdafx.h"
#include <iostream>

using namespace std;

#define _CRT_SECURE_NO_WARNINGS

void dumpPdhObjects();
void getHumanReadableError(DWORD dwErrorCode);

struct perf_counter  {
	_TCHAR *FieldName;
	_TCHAR *CounterPath;
};

struct perf_counters {
	size_t len;
	struct perf_counter *counters;
};

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
	if (argc >= 2 && wcscmp(argv[1], L"list") == 0 )  {
		// Send all the counter paths.
		dumpPdhObjects();
		return 0;
	}

	struct perf_counters counters;
	counters.counters = NULL;
	counters.len = 0;

	// Optional support for multiple fields, space separated
	_TCHAR *fields = _wgetenv(L"graph_order");
	if (fields == NULL) {
		// Nothing specified, just use the single field code
		
		struct perf_counter counter;
		// There is a method to use english counter names, see
		// http://support.microsoft.com/kb/287159/en
		counter.FieldName = L"field";
		counter.CounterPath = _wgetenv(L"counter_path");
		if (counter.CounterPath == NULL) {
			wprintf(L"No counter configured\n");
			return -1;
		}

		addToPerfCounters(&counters, counter);
	} else {
		// Transform the fields strings in a Win32 standard PZZTSTR.
		size_t fieldsLen = wcslen(fields);

		// Replace all spaces by a NULL char
		_TCHAR *fieldsCurrent = fields;
		for (_TCHAR *fieldsCurrent = fields; *fieldsCurrent != 0; fieldsCurrent ++) {
			if (*fieldsCurrent == L' ') *fieldsCurrent = 0;
		}

		PZZTSTR fieldsAsPZZTSTR = (PZZTSTR) realloc(fields, sizeof(_TCHAR) *  (fieldsLen + 2));
		
		// Finish by a double NULL
		fieldsAsPZZTSTR[fieldsLen] = 0;
		fieldsAsPZZTSTR[fieldsLen+1] = 0;
		
		for (PZZWSTR fieldsCurrent = fieldsAsPZZTSTR; *fieldsCurrent != 0; fieldsCurrent += wcslen(fieldsCurrent) + 1) {
			struct perf_counter counter;
			counter.FieldName = fieldsCurrent;

			// XXX - temporary stack_based
			_TCHAR envPath[256];
			wsprintf(envPath, L"%s.counter_path", fieldsCurrent );

			counter.CounterPath = _wgetenv(envPath);
			addToPerfCounters(&counters, counter);
		}
	}

	_TCHAR *graph_title = _wgetenv(L"graph_title");

	if (argc >= 2 && wcscmp(argv[1], L"config") == 0 )  {
		// Send the config
		wprintf(L"graph_title %s\n", graph_title);

		for (size_t i = 0; i < counters.len; i ++) {
			struct perf_counter* c = counters.counters + i;
			
			// c shall not be null
			if (c == 0) {
				wprintf(L"# counters.counters[%d] is NULL\n", i);
				continue;
			}
			wprintf(L"%s.label %s\n", c->FieldName, c->CounterPath);
		}

		return 0;
	}

	_TCHAR *counter_type = _wgetenv(L"counter_type");
	_TCHAR *counter_path = _wgetenv(L"counter_path");

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
