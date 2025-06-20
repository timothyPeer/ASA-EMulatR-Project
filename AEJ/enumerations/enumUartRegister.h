#pragma once
#include <QObject>

enum class Register : quint8
{
    RBR = 0x00, // Receive Buffer Register (read-only)
    THR = 0x00, // Transmit Holding Register (write-only)
    IER = 0x01, // Interrupt Enable Register
    IIR = 0x02, // Interrupt Identification Register (read-only)
    FCR = 0x02, // FIFO Control Register (write-only)
    LCR = 0x03, // Line Control Register
    MCR = 0x04, // Modem Control Register
    LSR = 0x05, // Line Status Register
    MSR = 0x06, // Modem Status Register
    SCR = 0x07, // Scratch Register
    DLL = 0x00, // Divisor Latch LSB (when DLAB=1)
    DLM = 0x01  // Divisor Latch MSB (when DLAB=1)
};