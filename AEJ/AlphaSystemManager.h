#pragma once
// AlphaSystemManager.h
#include <QVector>
#include <QtGlobal>
#include "MMIOManager.h"

struct MemWindow
{
    quint64 base, size;
    enum Type
    {
        DRAM,
        ROM,
        MMIO
    } kind;
};

class AlphaSystemManager
{
    QVector<MemWindow> windows;
    MMIOManager *m_mmioManager{nullptr};
   

    void buildMemoryMap()
    {
        /* ---------- hard-wired EV4/EV5 layout (can be read from CSR too) --- */
        windows = {/* DRAM 0–1 GB (dense)      */ {0x0000'0000, 0x4000'0000, MemWindow::DRAM},
                   /* DRAM 1–2 GB (dense)      */ {0x4000'0000, 0x4000'0000, MemWindow::DRAM},

                   /* Sparse I/O  (PA<33:31>=100) */ {0x8000'0000, 0x4000'0000, MemWindow::MMIO},
                   /* Sparse MEM (PA<33:31>=101) */ {0xC000'0000, 0x4000'0000, MemWindow::MMIO},
                   /* Dense  I/O (PA<33:31>=110) */ {0x1'0000'0000, 0x4'0000'0000, MemWindow::MMIO}};

        /* Tell MMIO manager **once** */
        for (auto &w : windows)
            if (w.kind == MemWindow::MMIO)
                m_mmioManager->registerWindow(w.base, w.size);
    }

  public:

      void attachMMIOManager(MMIOManager *mmio) { m_mmioManager = mmio; }
      void initialize(AsaModes::CpuModel cpuModel) {

            // AlphaSystemManager::powerOn()
          switch (cpuModel)
          {
          case AsaModes::CpuModel::CPU_EV67:
          case AsaModes::CpuModel::CPU_EV6:
          case AsaModes::CpuModel::CPU_EV68:
              //m_mmioManager->installTsunamiCSRWindow();
              break;

          case AsaModes::CpuModel::CPU_EV7:
          case AsaModes::CpuModel::CPU_EV79:
             // m_mmioManager->installMarvelDefaultWindows(); // similar helper you would add
              break;

              /* … other models … */
          }
      }
    quint64 physToDramOffset(quint64 pa) const
    {
        for (auto &w : windows)
            if (w.kind == MemWindow::DRAM && pa >= w.base && pa < w.base + w.size)
                return pa - w.base; // OK – hand to DRAM controller
        return ~0ULL;               // not DRAM
    }

  
};
