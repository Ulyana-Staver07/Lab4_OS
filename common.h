#pragma once

#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <iostream>
#include <string>
#include "Queue.h"

inline void setupConsole() {
    _setmode(_fileno(stdin), _O_U16TEXT);
    _setmode(_fileno(stdout), _O_U16TEXT);
}

struct Sync {
    HANDLE mutex = nullptr; 
    HANDLE empty = nullptr;  
    HANDLE full = nullptr;  
    HANDLE ready = nullptr;
    HANDLE stop = nullptr; 

    bool valid() const {
        return mutex && empty && full && ready && stop;
    }

    ~Sync() {
        if (mutex) CloseHandle(mutex);
        if (empty) CloseHandle(empty);
        if (full) CloseHandle(full);
        if (ready) CloseHandle(ready);
        if (stop) CloseHandle(stop);
    }
};

inline bool waitFor(HANDLE object, HANDLE stop) {
    HANDLE handles[] = {stop, object};
    return WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0 + 1;
}
