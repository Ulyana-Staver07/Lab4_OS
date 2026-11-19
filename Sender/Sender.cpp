#include "../common.h"

int wmain(int argc, wchar_t* argv[]) {
    setupConsole();
    if (argc != 3) {
        std::wcout << L"Запустите Receiver. Он сам откроет Sender.\n";
        return 1;
    }
    std::filesystem::path file = argv[1];
    std::wstring prefix = argv[2];

    std::fstream check(file, std::ios::binary | std::ios::in | std::ios::out);
    if (!check) {
        std::wcout << L"Не удалось открыть файл очереди.\n";
        return 1;
    }
    check.close();
    Sync s;
    s.mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, (prefix + L"Mutex").c_str());
    DWORD semRights = SYNCHRONIZE | SEMAPHORE_MODIFY_STATE;
    s.empty = OpenSemaphoreW(semRights, FALSE, (prefix + L"Empty").c_str());
    s.full  = OpenSemaphoreW(semRights, FALSE, (prefix + L"Full").c_str());
    s.ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, (prefix + L"Ready").c_str());
    s.stop  = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, (prefix + L"Stop").c_str());
    if (!s.valid() || !SetEvent(s.ready)) {
        std::wcout << L"Не удалось подключиться к Receiver.\n";
        return 1;
    }

    std::wcout << L"Sender готов. Файл: " << file.wstring() << L'\n';
    std::wstring command, message;
    while (WaitForSingleObject(s.stop, 0) == WAIT_TIMEOUT) {
        std::wcout << L"Sender: 1 — отправить, 0 — выход: ";
        if (!std::getline(std::wcin, command) || command == L"0") break;
        if (command != L"1") continue;

        std::wcout << L"Текст (до 19 символов): ";
        if (!std::getline(std::wcin, message)) break;
        if (message.size() >= QUEUE_RECORD_SIZE) {
            std::wcout << L"Сообщение слишком длинное.\n";
            continue;
        }
        std::wcout << L"Ожидаю свободную ячейку...\n" << std::flush;
        if (!waitFor(s.empty, s.stop) || !waitFor(s.mutex, s.stop)) break;
        bool ok = putMessage(file, message);
        if (!ok) SetEvent(s.stop);
        ReleaseMutex(s.mutex);
        if (!ok) {
            std::wcout << L"Ошибка при записи в файл.\n";
            return 1;
        }
        ReleaseSemaphore(s.full, 1, nullptr);
        std::wcout << L"Отправлено.\n";
    }
    return 0;
}
