// File: AlphaMMIOAddressChecker.h
#pragma once
#include <QtCore>
#include "../AEJ/enumerations/enumCpuModel.h"

class AlphaMMIOAddressChecker
{

  public:
    static bool isMMIOAddress(quint64 address, CpuModel variant)
    {
        switch (variant)
        {
        case CpuModel::CPU_EV5:
        case CpuModel::CPU_EV56:
        {
            return isEV5_EV56_MMIO(address);
        }

        case CpuModel::CPU_EV6:
        {

            return isEV6_MMIO(address);
        }

        case CpuModel::CPU_EV67:
        case CpuModel::CPU_EV68:
        {
            return isEV67_EV68_MMIO(address);
        }

        case CpuModel::CPU_EV7:
        case CpuModel::CPU_EV78:
        {
            return isEV7_EV78_MMIO(address);
        }

        default:
        {
            return isGenericAlphaMMIO(address);
        }
        }
    }

  private:
    // EV5/EV56 use the CIA/Pyxis chipset MMIO ranges
    static bool isEV5_EV56_MMIO(quint64 address)
    {
        return (address >= 0x8000'0000'0000ULL && address < 0xA000'0000'0000ULL)     // Primary I/O window
               || (address >= 0x1000'0000'0000ULL && address < 0x2000'0000'0000ULL); // Secondary window
    }

    // EV6 on-chip MMIO region (Pentium-centric PCI bridge)
    static bool isEV6_MMIO(quint64 address)
    {
        return (address >= 0x9000'0000'0000ULL && address < 0xA000'0000'0000ULL);
    }

    // EV67/EV68 extend EV6 region with additional slots
    static bool isEV67_EV68_MMIO(quint64 address)
    {
        return (address >= 0xB000'0000'0000ULL && address < 0xC000'0000'0000ULL);
    }

    // EV7/EV78 use a unified on-chip I/O region
    static bool isEV7_EV78_MMIO(quint64 address)
    {
        return (address >= 0xC000'0000'0000ULL && address < 0xD000'0000'0000ULL);
    }

    // Generic fallback for Alpha AXP: any address in the top 256?GB
    static bool isGenericAlphaMMIO(quint64 address) { return address >= 0xF000'0000'0000ULL; }
};
