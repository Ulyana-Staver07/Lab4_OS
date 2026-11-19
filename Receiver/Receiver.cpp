#include "../common.h"
#include <cstdlib>
#include <sstream>

bool chooseFile(std::filesystem::path& path) {
    std::wstring choice, name;
    while (true) {
        std::wcout << L"Имя файла: 1 — с клавиатуры, 2 — из переменной среды: ";
        if (!std::getline(std::wcin, choice)) return false;

        if (choice == L"1") {
            std::wcout << L"Введите имя файла (например, messages.bin): ";
            if (!std::getline(std::wcin, name)) return false;
            if (!name.empty()) {
                path = name;
                return true;
            }
            std::wcout << L"Имя файла пустое.\n";
        } else if (choice == L"2") {
            std::wcout << L"Введите свою фамилию так, как названа переменная среды: ";
            if (!std::getline(std::wcin, name)) return false;
            wchar_t* value = nullptr;
            size_t length = 0;
            if (!name.empty() && _wdupenv_s(&value, &length, name.c_str()) == 0 && value && *value) {
                path = value;
                std::free(value);
                std::wcout << L"Файл из переменной среды: " << path.wstring() << L'\n';
                return true;
            }
            std::free(value);
            std::wcout << L"Переменная не найдена или пуста. Выберите способ ещё раз.\n";
        } else {
            std::wcout << L"Введите 1 или 2.\n";
        }
    }
}

bool readPositive(const wchar_t* prompt, int& number) {
    std::wstring line;
    while (true) {
        std::wcout << prompt;
        if (!std::getline(std::wcin, line)) return false;
        std::wistringstream input(line);
        wchar_t extra;
        if ((input >> number) && number > 0 && !(input >> extra)) return true;
        std::wcout << L"Нужно целое число больше нуля.\n";
    }
}

bool startSender(const std::filesystem::path& exe, const std::filesystem::path& file,
                 const std::wstring& prefix, HANDLE ready) {
    std::wstring command = L"\"" + exe.wstring() + L"\" \"" + file.wstring() + L"\" " + prefix;
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NEW_CONSOLE, nullptr, nullptr, &startup, &process)) {
        std::wcout << L"Не удалось запустить Sender.exe. Код Windows: " << GetLastError() << L'\n';
        return false;
    }
    CloseHandle(process.hThread);
    HANDLE handles[] = {ready, process.hProcess};
    DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    CloseHandle(process.hProcess);
    return result == WAIT_OBJECT_0;
}

int wmain() {
    setupConsole();
    std::filesystem::path file;
    int capacity, senderCount;
    if (!chooseFile(file) || !readPositive(L"Количество ячеек: ", capacity)) return 1;
    if (!createQueue(file, capacity)) {
        std::wcout << L"Не удалось создать файл очереди.\n";
        return 1;
    }
    if (!readPositive(L"Количество процессов Sender: ", senderCount)) return 1;

    std::wstring prefix = L"Local\\Lab4_" + std::to_wstring(GetCurrentProcessId()) + L"_";
    Sync s;
    s.mutex = CreateMutexW(nullptr, FALSE, (prefix + L"Mutex").c_str());
    s.empty = CreateSemaphoreW(nullptr, capacity, capacity, (prefix + L"Empty").c_str());
    s.full  = CreateSemaphoreW(nullptr, 0, capacity, (prefix + L"Full").c_str());
    s.ready = CreateEventW(nullptr, FALSE, FALSE, (prefix + L"Ready").c_str());
    s.stop  = CreateEventW(nullptr, TRUE, FALSE, (prefix + L"Stop").c_str());
    if (!s.valid()) {
        std::wcout << L"Не удалось создать объекты синхронизации.\n";
        return 1;
    }

    wchar_t program[32768];
    DWORD length = GetModuleFileNameW(nullptr, program, 32768);
    if (length == 0 || length >= 32768) return 1;
    auto sender = std::filesystem::path(program).parent_path() / L"Sender.exe";
    for (int i = 0; i < senderCount; ++i) {
        if (!startSender(sender, file, prefix, s.ready)) {
            std::wcout << L"Отправитель не запустился или не подтвердил готовность.\n";
            SetEvent(s.stop);
            return 1;
        }
    }

    std::wcout << L"Все отправители готовы.\n";
    std::wstring command, message;
    int result = 0;
    while (true) {
        std::wcout << L"Receiver: 1 — прочитать, 0 — выход: ";
        if (!std::getline(std::wcin, command) || command == L"0") break;
        if (command != L"1") continue;

        std::wcout << L"Ожидаю сообщение...\n" << std::flush;
        if (!waitFor(s.full, s.stop) || !waitFor(s.mutex, s.stop)) {
            result = 1;
            break;
        }
        bool ok = takeMessage(file, message);
        if (!ok) SetEvent(s.stop);
        ReleaseMutex(s.mutex);
        if (!ok) {
            result = 1;
            break;
        }
        ReleaseSemaphore(s.empty, 1, nullptr);
        std::wcout << L"Получено: " << message << L'\n';
    }
    if (result != 0) std::wcout << L"Ошибка при работе с очередью.\n";
    SetEvent(s.stop);
    return result;
}
