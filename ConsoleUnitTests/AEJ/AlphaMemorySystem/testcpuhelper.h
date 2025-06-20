// Comprehensive unit tests for AlphaMemorySystem TLB integration
#include <QtTest/QtTest>
#include <QObject>
#include <QSignalSpy>
#include <QThread>
#include <atomic>
#include "../AEJ/tlbVictimTLBCache.h"
#include "../AEJ/AlphaCPU_refactored.h"
#include "../AEJ/SafeMemory_refactored.h"

/**
 * @brief Test helper class to create minimal AlphaCPU for testing
 * This creates real AlphaCPU objects but with minimal configuration
 */
 class TestCPUHelper
{
public:
    static AlphaCPU* createTestCPU(quint16 cpuId) {
        AlphaCPU* cpu = new AlphaCPU();

        // Set basic test configuration
        cpu->setCPUID(cpuId);
        cpu->setMMUEnabled(true);
        cpu->setCurrentASN(1); // Default test ASN
        cpu->setKernelMode(false); // Start in user mode

        return cpu;
    }

    static void cleanupTestCPU(AlphaCPU* cpu) {
        delete cpu;
    }
};