#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

constexpr int QUEUE_RECORD_SIZE = 20;

struct QueueHeader {
    int capacity;
    int readIndex;
    int writeIndex;
};

inline std::streamoff slotPosition(int index) {
    const std::streamoff slotBytes = QUEUE_RECORD_SIZE * sizeof(wchar_t);
    return sizeof(QueueHeader) + slotBytes * index;
}

inline bool createQueue(const std::filesystem::path& path, int capacity) {
    if (capacity <= 0) return false;

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    QueueHeader header = {capacity, 0, 0};
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    file.seekp(slotPosition(capacity) - 1);
    file.put('\0');
    file.close();
    return !file.fail();
}

inline bool readHeader(std::fstream& file, QueueHeader& header) {
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    return file && header.capacity > 0 &&
           header.readIndex >= 0 && header.readIndex < header.capacity &&
           header.writeIndex >= 0 && header.writeIndex < header.capacity;
}

inline bool saveHeader(std::fstream& file, const QueueHeader& header) {
    file.seekp(0);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.close();
    return !file.fail();
}

inline bool putMessage(const std::filesystem::path& path, const std::wstring& message) {
    if (message.size() >= QUEUE_RECORD_SIZE) return false;

    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    QueueHeader header;
    if (!readHeader(file, header)) return false;

    wchar_t record[QUEUE_RECORD_SIZE] = {};
    std::copy(message.begin(), message.end(), record);
    file.seekp(slotPosition(header.writeIndex));
    file.write(reinterpret_cast<const char*>(record), sizeof(record));
    if (!file) return false;

    header.writeIndex = (header.writeIndex + 1) % header.capacity;
    return saveHeader(file, header);
}

inline bool takeMessage(const std::filesystem::path& path, std::wstring& message) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    QueueHeader header;
    if (!readHeader(file, header)) return false;

    wchar_t record[QUEUE_RECORD_SIZE];
    file.seekg(slotPosition(header.readIndex));
    file.read(reinterpret_cast<char*>(record), sizeof(record));
    if (!file || record[QUEUE_RECORD_SIZE - 1] != L'\0') return false;

    message = record;
    header.readIndex = (header.readIndex + 1) % header.capacity;
    return saveHeader(file, header);
}
