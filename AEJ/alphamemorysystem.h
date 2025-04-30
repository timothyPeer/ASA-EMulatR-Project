#pragma once

#include <QObject>
#include <QMap>
#include <QHash>
#include <QPair>
#include <QMutex>
#include <QByteArray>
#include "SafeMemory.h"
#include "MMIOManager.h"
#include <QMutexLocker>
#include <QDebug>
#include <QVector>
#include "alphacpu.h"

/*

Class				Role	Issues

SafeMemory			Flat contiguous physical RAM storage, no VA translation	
MMIOManager			Manages memory-mapped devices, 8/16/32/64-bit access	
AlphaMemorySystem	Virtual → Physical translation, Protection, Traps	

TODO:: 
Item	Action
Integration	AlphaMemorySystem should route ALL access through SafeMemory or MMIOManager
API Cleanup	AlphaMemorySystem should expose only virtual address operations (no direct physical access)
Memory Mapping	AlphaMemorySystem should manage VA→PA mappings fully
Protection	AlphaMemorySystem should raise protectionFault signal if access violations occur
Trap Handling	AlphaMemorySystem should raise translationMiss signal if unmapped VA accessed
Thread Safety	Locking should be handled at AlphaMemorySystem level for mappings, but SafeMemory internal locks for actual RAM
*/


/**
 * @brief Full virtual memory system for Alpha CPU
 * Supports virtual to physical translation, memory protection, MMIO access.
 */
class AlphaMemorySystem : public QObject {
    Q_OBJECT





public:
    explicit AlphaMemorySystem(QObject* parent = nullptr);
    ~AlphaMemorySystem();

//     quint64 readMemory(quint64 virtualAddress, int size);
//     void writeMemory(quint64 virtualAddress, quint64 value, int size);
    quint64 readVirtualMemory(AlphaCPU* alphaCPU, quint64 virtualAddress, int size);
    bool readVirtualMemory(AlphaCPU* alphaCPU, quint64 virtualAddr, quint32& value, int size);
    void clearMappings();
    QJsonDocument getMappedRegionsJson() const;
    void writeVirtualMemory(AlphaCPU* alphaCPU, quint64 virtualAddress, quint64 value, int size);

    bool isMapped(quint64 vaddr) const;
    bool checkAccess(quint64 vaddr, int accessType) const;
    void mapMemory(quint64 virtualAddr, quint64 physicalAddr, quint64 size, int protectionFlags);
    void unmapMemory(quint64 virtualAddr);
    void setMemoryAlloc(quint64 memory_) {
        initialize(memory_);
    }
    SafeMemory* getSafeMemory() const { return m_memorySystem; }
    MMIOManager* getMMIOManager() const { return mmioManager; }
	bool translate(AlphaCPU* alphaCPU, quint64 virtualAddr, quint64& physicalAddr, int accessType);
    void initialize_signalsAndSlots();
    
    QVector<QPair<quint64, MappingEntry>> getMappedRegions() const;

public slots:
   

signals:
    void memoryRead(quint64 address, quint64 value, int size);
    void memoryWritten(quint64 address, quint64 value, int size);
    void protectionFault(quint64 address, int accessType);
    void translationMiss(quint64 virtualAddress);
    void mappingsCleared();

private:
    QMutex memoryLock;
    SafeMemory* m_memorySystem;
    MMIOManager* mmioManager;
    // Virtual Address Mapping Table
    QMap<quint64, MappingEntry> memoryMap; // 🆕 Virtual Address → MappingEntry
    // Configured via setMemory(qUInt64)
    void initialize(quint64 size_mb) { m_memorySystem->resize(size_mb); } // Initialize Physical Memory (SafeMemory)
    bool isMMIOAddress(quint64 physicalAddr) ;
};
