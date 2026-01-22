#include <windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>

/* ================= PROTOCOL ================= */
#define CMD_MINEFIELD 'M'

/* ================= CHECKSUM ================= */
uint8_t XOR_Checksum(const std::vector<uint8_t>& data, size_t len)
{
    uint8_t chk = 0;
    for (size_t i = 0; i < len; i++)
        chk ^= data[i];
    return chk;
}

/* ================= SERIAL ================= */
HANDLE OpenSerial(const char* port)
{
    HANDLE hSerial = CreateFileA(
        port,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (hSerial == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Cannot open COM port\n";
        exit(1);
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(hSerial, &dcb);

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;

    SetCommState(hSerial, &dcb);

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 200;
    timeouts.ReadTotalTimeoutMultiplier = 50;
    SetCommTimeouts(hSerial, &timeouts);

    return hSerial;
}

/* ================= SEND COMMAND ================= */
void SendCommand(HANDLE hSerial, char difficulty)
{
    std::vector<uint8_t> packet;
    packet.push_back(CMD_MINEFIELD);        // CMD
    packet.push_back(1);                     // LEN = 1 byte
    packet.push_back((uint8_t)difficulty);  // DATA = 'E','M','H'
    packet.push_back(XOR_Checksum(packet, packet.size())); // CHK

    DWORD written;
    WriteFile(hSerial, packet.data(), packet.size(), &written, nullptr);

    std::cout << "Command sent (" << written << " bytes), difficulty: " << difficulty << "\n";
}

/* ================= RECEIVE PACKET ================= */
std::vector<uint8_t> ReceivePacket(HANDLE hSerial)
{
    std::vector<uint8_t> rx;
    uint8_t byte;
    DWORD read;

    // Приймаємо мінімум CMD + STATUS + LEN
    while (rx.size() < 3)
    {
        if (ReadFile(hSerial, &byte, 1, &read, nullptr) && read)
            rx.push_back(byte);
    }

    uint8_t len = rx[2]; // LEN = кількість клітинок

    // Приймаємо всі дані + чексума
    while (rx.size() < (size_t)(len + 4)) // CMD+STATUS+LEN+DATA+CHK
    {
        if (ReadFile(hSerial, &byte, 1, &read, nullptr) && read)
            rx.push_back(byte);
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(5)); // чекати дані
    }

    return rx;
}

/* ================= PRINT FIELD ================= */
void PrintField(const std::vector<uint8_t>& field, uint8_t fieldSize)
{
    std::cout << "\nMinefield (" << (int)fieldSize << "x" << (int)fieldSize << "):\n";
    for (uint8_t i = 0; i < fieldSize; i++)
    {
        for (uint8_t j = 0; j < fieldSize; j++)
        {
            uint8_t cell = field[i * fieldSize + j];
            if (cell == 9) std::cout << "* "; // міна
            else std::cout << (int)cell << " "; // цифра 0..8
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

/* ================= MAIN ================= */
int main()
{
    const char* COM_PORT = "COM5"; // змінити під свій порт
    HANDLE hSerial = OpenSerial(COM_PORT);

    char difficulty;
    std::cout << "Enter difficulty (E / M / H): ";
    std::cin >> difficulty;

    SendCommand(hSerial, difficulty);

    std::cout << "Waiting for minefield from STM...\n";
    auto response = ReceivePacket(hSerial);

    std::cout << "Received packet (" << response.size() << " bytes)\n";

    // ---- перевірка чексуми ----
    uint8_t recvChk = response.back();
    uint8_t calcChk = XOR_Checksum(response, response.size() - 1);

    if (recvChk == calcChk)
        std::cout << "✅ Checksum OK\n";
    else
        std::cout << "❌ Checksum ERROR\n";

    // ---- перевірка статусу ----
    if (response[1] != 0x00)
    {
        std::cout << "STM returned error: 0x" << std::hex << (int)response[1] << "\n";
        CloseHandle(hSerial);
        return 1;
    }

    // ---- визначаємо розмір квадратного поля ----
    uint8_t len = response[2]; // кількість клітин
    uint8_t fieldSize = (uint8_t)std::sqrt(len);
    std::vector<uint8_t> field(response.begin() + 3, response.end() - 1);

    PrintField(field, fieldSize);

    CloseHandle(hSerial);
    return 0;
}
