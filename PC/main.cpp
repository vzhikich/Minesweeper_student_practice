#include <windows.h>
#include <iostream>
int main()
{
    HANDLE hSerial;
    hSerial = CreateFile("COM6",GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
    if (hSerial == INVALID_HANDLE_VALUE)
    {
        std::cout << "Error opening COM port\n";
        return 1;
    }
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hSerial, &dcbSerialParams);
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;
    SetCommState(hSerial, &dcbSerialParams);

    const char* message = "Hello from C++ via UART!\r\n";
    DWORD bytesWritten;
    WriteFile(hSerial,message,strlen(message),&bytesWritten,NULL);
    std::cout << "Byte sent:" << bytesWritten << std::endl;
    CloseHandle(hSerial);
    return 0;
}
