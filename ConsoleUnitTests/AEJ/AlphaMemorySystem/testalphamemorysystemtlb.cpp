// Comprehensive unit tests for AlphaMemorySystem TLB integration
#include <QtTest/QtTest>
#include <QObject>
#include <QSignalSpy>
#include <QThread>
#include <atomic>
#include "../AEJ/tlbVictimTLBCache.h"
#include "../AEJ/AlphaCPU_refactored.h"
#include "../AEJ/SafeMemory_refactored.h"
#include "testalphamemorysystemtlb.h"

/**
 * @brief Test helper class to create minimal AlphaCPU for testing
 * This creates real AlphaCPU objects but with minimal configuration
 */



void TestAlphaMemorySystemTLB::initTestCase()
{
    // Create real SafeMemory for testing
    m_testSafeMemory = new SafeMemory();
    m_testSafeMemory->initialize(0x10000000); // 256MB test memory

    // Create real AlphaCPU objects configured for testing
    m_testCPU1 = TestCPUHelper::createTestCPU(0);
    m_testCPU2 = TestCPUHelper::createTestCPU(1);

    qDebug() << "Test setup: Created SafeMemory and 2 test CPUs";
}

void TestAlphaMemorySystemTLB::cleanupTestCase()
{
    TestCPUHelper::cleanupTestCPU(m_testCPU1);
    TestCPUHelper::cleanupTestCPU(m_testCPU2);
    delete m_testSafeMemory;

    qDebug() << "Test cleanup: Destroyed all test components";
}

void TestAlphaMemorySystemTLB::init()
{
    // Create fresh memory system for each test
    m_memorySystem = new AlphaMemorySystem();
    m_memorySystem->attachSafeMemory(m_testSafeMemory);

    // Clear any previous memory contents
    m_testSafeMemory->clear();

    // Set up basic test memory layout
    setupBasicTestMemory();
}

void TestAlphaMemorySystemTLB::cleanup()
{
    delete m_memorySystem;
    m_memorySystem = nullptr;
}

// =======================
// BASIC TLB INTEGRATION TESTS
// =======================

void TestAlphaMemorySystemTLB::testTLBSystemCreation()
{
    // Test that TLB system is created during construction
    QVERIFY(m_memorySystem != nullptr);

    // Verify internal TLB system exists (use integrity check)
    QVERIFY(m_memorySystem->validateTLBSystemIntegrity());

    // Test that we can register CPUs
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU2, 1));

    // Verify CPU count
    QCOMPARE(m_memorySystem->getCPUCount(), (quint16)2);
}

void TestAlphaMemorySystemTLB::testCPURegistrationWithTLB()
{
    // Test CPU registration creates TLB entries
    QSignalSpy registrationSpy(m_memorySystem, &AlphaMemorySystem::sigCPURegistered);

    // Register first CPU
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));
    QCOMPARE(registrationSpy.count(), 1);
    QCOMPARE(registrationSpy.takeFirst().at(0).value<quint16>(), (quint16)0);

    // Register second CPU
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU2, 1));
    QCOMPARE(registrationSpy.count(), 1);

    // Test that duplicate registration fails
    QVERIFY(!m_memorySystem->registerCPU(m_mockCPU1, 0));

    // Verify both CPUs are registered
    QCOMPARE(m_memorySystem->getCPUCount(), (quint16)2);
    QVERIFY(m_memorySystem->getCPU(0) == m_mockCPU1);
    QVERIFY(m_memorySystem->getCPU(1) == m_mockCPU2);
}

void TestAlphaMemorySystemTLB::testCPUUnregistrationWithTLB()
{
    // Set up: register two CPUs
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU2, 1));

    QSignalSpy unregistrationSpy(m_memorySystem, &AlphaMemorySystem::sigCPUUnregistered);

    // Unregister one CPU
    QVERIFY(m_memorySystem->unregisterCPU(0));
    QCOMPARE(unregistrationSpy.count(), 1);
    QCOMPARE(unregistrationSpy.takeFirst().at(0).value<quint16>(), (quint16)0);

    // Verify CPU count decreased
    QCOMPARE(m_memorySystem->getCPUCount(), (quint16)1);
    QVERIFY(m_memorySystem->getCPU(0) == nullptr);
    QVERIFY(m_memorySystem->getCPU(1) == m_mockCPU2);

    // Test that duplicate unregistration fails gracefully
    QVERIFY(!m_memorySystem->unregisterCPU(0));
}

// =======================
// TRANSLATION PIPELINE TESTS
// =======================

void TestAlphaMemorySystemTLB::testTranslationWithTLBHit()
{
    // Set up: register CPU and create memory mapping
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));

    quint64 virtualAddr = 0x10000;
    quint64 physicalAddr = 0x20000;

    m_memorySystem->mapMemory(virtualAddr, physicalAddr, 0x1000, 0x7); // RWX permissions

    // First translation should miss TLB and populate it
    quint64 result1;
    QVERIFY(m_memorySystem->readVirtualMemory(0, virtualAddr, result1, 8, 0));

    // Second translation should hit TLB (faster)
    quint64 result2;
    QElapsedTimer timer;
    timer.start();
    QVERIFY(m_memorySystem->readVirtualMemory(0, virtualAddr, result2, 8, 0));
    qint64 hitTime = timer.nsecsElapsed();

    // TLB hit should be very fast (< 1000 nanoseconds on modern hardware)
    QVERIFY(hitTime < 1000);

    qDebug() << "TLB hit time:" << hitTime << "nanoseconds";
}

void TestAlphaMemorySystemTLB::testTranslationWithTLBMiss()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));

    // Create mock page table entry
    quint64 virtualAddr = 0x10000;
    quint64 physicalAddr = 0x20000;
    m_mockSafeMemory->setPageTableEntry(virtualAddr, physicalAddr, true, true, true);

    QElapsedTimer timer;
    timer.start();

    // This should cause TLB miss and page table walk
    quint64 result;
    QVERIFY(m_memorySystem->readVirtualMemory(0, virtualAddr, result, 8, 0));

    qint64 missTime = timer.nsecsElapsed();
    qDebug() << "TLB miss time:" << missTime << "nanoseconds";

    // TLB miss should be slower than hit but still reasonable
    QVERIFY(missTime > 1000);  // Should be slower than TLB hit
    QVERIFY(missTime < 100000); // But still under 100 microseconds
}

void TestAlphaMemorySystemTLB::testTLBPopulationAfterPageTableWalk()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));

    quint64 virtualAddr = 0x10000;
    quint64 physicalAddr = 0x20000;

    // Set up page table entry
    m_mockSafeMemory->setPageTableEntry(virtualAddr, physicalAddr, true, true, true);

    // First access should populate TLB
    quint64 result1;
    QVERIFY(m_memorySystem->readVirtualMemory(0, virtualAddr, result1, 8, 0));

    // Second access should be faster due to TLB population
    QElapsedTimer timer1, timer2;

    // Clear TLB and time the miss
    m_memorySystem->invalidateAllTLB(0);
    timer1.start();
    QVERIFY(m_memorySystem->readVirtualMemory(0, virtualAddr, result1, 8, 0));
    qint64 missTime = timer1.nsecsElapsed();

    // Time the hit
    timer2.start();
    QVERIFY(m_memorySystem->readVirtualMemory(0, virtualAddr, result1, 8, 0));
    qint64 hitTime = timer2.nsecsElapsed();

    // Hit should be significantly faster than miss
    QVERIFY(hitTime < missTime);
    qDebug() << "TLB population effectiveness: miss=" << missTime << "ns, hit=" << hitTime << "ns";
}

// =======================
// ERROR HANDLING TESTS
// =======================

void TestAlphaMemorySystemTLB::testInvalidAddressHandling()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));

    // Test invalid virtual addresses
    quint64 result;

    // Null pointer
    QVERIFY(!m_memorySystem->readVirtualMemory(0, 0x0, result, 8, 0));

    // Address in gap between user and kernel space (invalid on Alpha)
    QVERIFY(!m_memorySystem->readVirtualMemory(0, 0x8000000000000000ULL, result, 8, 0));

    // Address beyond maximum virtual address
    QVERIFY(!m_memorySystem->readVirtualMemory(0, 0xFFFFFFFFFFFFFFFFULL, result, 8, 0));

    // Verify TLB system integrity after invalid accesses
    QVERIFY(m_memorySystem->validateTLBSystemIntegrity());
}

void TestAlphaMemorySystemTLB::testTLBCorruptionRecovery()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));

    QSignalSpy errorSpy(m_memorySystem, &AlphaMemorySystem::sigTLBSystemError);

    // Simulate TLB corruption by forcing many consecutive failures
    for (int i = 0; i < 15; ++i) {
        quint64 result;
        m_memorySystem->readVirtualMemory(0, 0x1000 + i * 0x1000, result, 8, 0);
    }

    // Should trigger emergency cleanup after enough failures
    // Verify system is still functional
    QVERIFY(m_memorySystem->validateTLBSystemIntegrity());

    // Should be able to continue normal operation
    m_memorySystem->mapMemory(0x10000, 0x20000, 0x1000, 0x7);
    quint64 result;
    QVERIFY(m_memorySystem->readVirtualMemory(0, 0x10000, result, 8, 0));
}

void TestAlphaMemorySystemTLB::testExceptionSafety()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));

    // Test that exceptions don't corrupt TLB state
    try {
        // Force an exception by accessing mock memory incorrectly
        m_mockSafeMemory->setThrowException(true);

        quint64 result;
        m_memorySystem->readVirtualMemory(0, 0x10000, result, 8, 0);

        m_mockSafeMemory->setThrowException(false);
    }
    catch (...) {
        // Expected - but TLB should still be intact
    }

    // Verify TLB system integrity after exception
    QVERIFY(m_memorySystem->validateTLBSystemIntegrity());

    // Should be able to continue normal operation
    m_memorySystem->mapMemory(0x10000, 0x20000, 0x1000, 0x7);
    quint64 result;
    QVERIFY(m_memorySystem->readVirtualMemory(0, 0x10000, result, 8, 0));
}

// =======================
// SMP INVALIDATION TESTS
// =======================

void TestAlphaMemorySystemTLB::testSingleAddressInvalidation()
{
    // Set up two CPUs
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU2, 1));

    QSignalSpy invalidationSpy(m_memorySystem, &AlphaMemorySystem::sigTlbInvalidated);

    quint64 virtualAddr = 0x10000;
    quint64 physicalAddr = 0x20000;

    // Create memory mapping and populate TLBs on both CPUs
    m_memorySystem->mapMemory(virtualAddr, physicalAddr, 0x1000, 0x7);

    quint64 result;
    QVERIFY(m_memorySystem->readVirtualMemory(0, virtualAddr, result, 8, 0)); // CPU 0
    QVERIFY(m_memorySystem->readVirtualMemory(1, virtualAddr, result, 8, 0)); // CPU 1

    // Invalidate single address
    m_memorySystem->invalidateTlbSingle(virtualAddr, 0);

    // Should emit invalidation signal
    QCOMPARE(invalidationSpy.count(), 1);

    // Both CPUs should now miss on this address (slower access)
    QElapsedTimer timer1, timer2;

    timer1.start();
    QVERIFY(m_memorySystem->readVirtualMemory(0, virtualAddr, result, 8, 0));
    qint64 time1 = timer1.nsecsElapsed();

    timer2.start();
    QVERIFY(m_memorySystem->readVirtualMemory(1, virtualAddr, result, 8, 0));
    qint64 time2 = timer2.nsecsElapsed();

    qDebug() << "Post-invalidation times: CPU0=" << time1 << "ns, CPU1=" << time2 << "ns";

    // Times should be relatively similar (both doing page table walks)
    QVERIFY(abs(time1 - time2) < 50000); // Within 50 microseconds
}

void TestAlphaMemorySystemTLB::testASNInvalidation()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU2, 1));

    // Set different ASNs for testing
    m_mockCPU1->setCurrentASN(1);
    m_mockCPU2->setCurrentASN(2);

    // Populate TLBs with different ASN entries
    m_memorySystem->mapMemory(0x10000, 0x20000, 0x1000, 0x7);
    m_memorySystem->mapMemory(0x11000, 0x21000, 0x1000, 0x7);

    quint64 result;
    QVERIFY(m_memorySystem->readVirtualMemory(0, 0x10000, result, 8, 0)); // ASN 1
    QVERIFY(m_memorySystem->readVirtualMemory(1, 0x11000, result, 8, 0)); // ASN 2

    QSignalSpy invalidationSpy(m_memorySystem, &AlphaMemorySystem::sigTlbInvalidated);

    // Invalidate ASN 1
    m_memorySystem->invalidateTLBByASN(1, 0);

    QCOMPARE(invalidationSpy.count(), 1);

    // CPU 0 (ASN 1) should miss, CPU 1 (ASN 2) should potentially still hit
    QElapsedTimer timer1, timer2;

    timer1.start();
    QVERIFY(m_memorySystem->readVirtualMemory(0, 0x10000, result, 8, 0));
    qint64 time1 = timer1.nsecsElapsed();

    timer2.start();
    QVERIFY(m_memorySystem->readVirtualMemory(1, 0x11000, result, 8, 0));
    qint64 time2 = timer2.nsecsElapsed();

    qDebug() << "ASN invalidation times: CPU0(ASN1)=" << time1 << "ns, CPU1(ASN2)=" << time2 << "ns";
}

void TestAlphaMemorySystemTLB::testGlobalTLBFlush()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU2, 1));

    // Populate TLBs on both CPUs
    m_memorySystem->mapMemory(0x10000, 0x20000, 0x1000, 0x7);
    m_memorySystem->mapMemory(0x11000, 0x21000, 0x1000, 0x7);

    quint64 result;
    QVERIFY(m_memorySystem->readVirtualMemory(0, 0x10000, result, 8, 0));
    QVERIFY(m_memorySystem->readVirtualMemory(1, 0x11000, result, 8, 0));

    QSignalSpy invalidationSpy(m_memorySystem, &AlphaMemorySystem::sigTlbInvalidated);

    // Global flush
    m_memorySystem->invalidateAllTLB(0);

    QCOMPARE(invalidationSpy.count(), 1);

    // Both CPUs should now miss on all addresses
    QElapsedTimer timer1, timer2;

    timer1.start();
    QVERIFY(m_memorySystem->readVirtualMemory(0, 0x10000, result, 8, 0));
    qint64 time1 = timer1.nsecsElapsed();

    timer2.start();
    QVERIFY(m_memorySystem->readVirtualMemory(1, 0x11000, result, 8, 0));
    qint64 time2 = timer2.nsecsElapsed();

    // Both should be doing page table walks (similar times)
    QVERIFY(time1 > 1000); // Should be slow (TLB miss)
    QVERIFY(time2 > 1000); // Should be slow (TLB miss)

    qDebug() << "Global flush times: CPU0=" << time1 << "ns, CPU1=" << time2 << "ns";
}

void TestAlphaMemorySystemTLB::testSeparateDataInstructionInvalidation()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));

    quint64 codeAddr = 0x10000;
    quint64 dataAddr = 0x20000;

    // Create executable and data mappings
    m_memorySystem->mapMemory(codeAddr, 0x100000, 0x1000, 0x5); // R-X (code)
    m_memorySystem->mapMemory(dataAddr, 0x200000, 0x1000, 0x3); // RW- (data)

    // Access both as instruction and data
    quint64 result;
    QVERIFY(m_memorySystem->readVirtualMemory(0, codeAddr, result, 8, 0));   // Instruction read
    QVERIFY(m_memorySystem->readVirtualMemory(0, dataAddr, result, 8, 0));   // Data read

    QSignalSpy invalidationSpy(m_memorySystem, &AlphaMemorySystem::sigTlbInvalidated);

    // Invalidate only instruction entries
    m_memorySystem->invalidateTlbSingleInstruction(codeAddr, 0);
    QCOMPARE(invalidationSpy.count(), 1);

    // Code access should be slower, data access should still be fast
    QElapsedTimer timer1, timer2;

    timer1.start();
    QVERIFY(m_memorySystem->readVirtualMemory(0, codeAddr, result, 8, 0));
    qint64 codeTime = timer1.nsecsElapsed();

    timer2.start();
    QVERIFY(m_memorySystem->readVirtualMemory(0, dataAddr, result, 8, 0));
    qint64 dataTime = timer2.nsecsElapsed();

    qDebug() << "Selective invalidation: code=" << codeTime << "ns, data=" << dataTime << "ns";

    // Code should be slower due to invalidation
    QVERIFY(codeTime > dataTime);
}

// =======================
// PERFORMANCE TESTS
// =======================

void TestAlphaMemorySystemTLB::testTLBHitPerformance()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));

    // Create multiple memory mappings
    const int numPages = 100;
    for (int i = 0; i < numPages; ++i) {
        quint64 vaddr = 0x10000 + (i * 0x1000);
        quint64 paddr = 0x100000 + (i * 0x1000);
        m_memorySystem->mapMemory(vaddr, paddr, 0x1000, 0x7);
    }

    // Warm up TLB
    quint64 result;
    for (int i = 0; i < numPages; ++i) {
        quint64 vaddr = 0x10000 + (i * 0x1000);
        QVERIFY(m_memorySystem->readVirtualMemory(0, vaddr, result, 8, 0));
    }

    // Measure TLB hit performance
    QElapsedTimer timer;
    timer.start();

    const int iterations = 1000;
    for (int iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < numPages; ++i) {
            quint64 vaddr = 0x10000 + (i * 0x1000);
            QVERIFY(m_memorySystem->readVirtualMemory(0, vaddr, result, 8, 0));
        }
    }

    qint64 totalTime = timer.nsecsElapsed();
    qint64 avgTimePerAccess = totalTime / (iterations * numPages);

    qDebug() << "TLB hit performance:" << avgTimePerAccess << "ns per access";
    qDebug() << "Total accesses:" << (iterations * numPages);

    // TLB hits should be very fast (< 500ns on modern hardware)
    QVERIFY(avgTimePerAccess < 500);
}

void TestAlphaMemorySystemTLB::testInvalidationPerformance()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU2, 1));

    // Create many mappings
    const int numPages = 128; // Fill TLB completely
    for (int i = 0; i < numPages; ++i) {
        quint64 vaddr = 0x10000 + (i * 0x1000);
        quint64 paddr = 0x100000 + (i * 0x1000);
        m_memorySystem->mapMemory(vaddr, paddr, 0x1000, 0x7);
    }

    // Populate TLBs
    quint64 result;
    for (int i = 0; i < numPages; ++i) {
        quint64 vaddr = 0x10000 + (i * 0x1000);
        QVERIFY(m_memorySystem->readVirtualMemory(0, vaddr, result, 8, 0));
        QVERIFY(m_memorySystem->readVirtualMemory(1, vaddr, result, 8, 0));
    }

    QSignalSpy performanceSpy(m_memorySystem, &AlphaMemorySystem::sigTLBInvalidationPerformance);

    // Measure invalidation performance
    QElapsedTimer timer;
    timer.start();

    m_memorySystem->invalidateAllTLB(0);

    qint64 invalidationTime = timer.nsecsElapsed();

    qDebug() << "Global TLB invalidation time:" << invalidationTime << "ns";
    qDebug() << "Time per entry:" << (invalidationTime / (numPages * 2)) << "ns";

    // Invalidation should be fast (< 100 microseconds total)
    QVERIFY(invalidationTime < 100000);

    // Should have emitted performance signal
    QCOMPARE(performanceSpy.count(), 1);
}

void TestAlphaMemorySystemTLB::testConcurrentAccess()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU2, 1));

    // Create shared mapping
    m_memorySystem->mapMemory(0x10000, 0x100000, 0x10000, 0x7); // 64KB mapping

    std::atomic<bool> startFlag(false);
    std::atomic<int> completedAccesses(0);
    const int accessesPerThread = 1000;

    // Thread 1: CPU 0 accesses
    std::thread thread1([&]() {
        while (!startFlag.load()) { /* wait */ }

        quint64 result;
        for (int i = 0; i < accessesPerThread; ++i) {
            quint64 addr = 0x10000 + (i % 64) * 0x100; // Vary addresses
            if (m_memorySystem->readVirtualMemory(0, addr, result, 8, 0)) {
                completedAccesses.fetch_add(1);
            }
        }
        });

    // Thread 2: CPU 1 accesses
    std::thread thread2([&]() {
        while (!startFlag.load()) { /* wait */ }

        quint64 result;
        for (int i = 0; i < accessesPerThread; ++i) {
            quint64 addr = 0x10000 + (i % 64) * 0x100; // Same addresses
            if (m_memorySystem->readVirtualMemory(1, addr, result, 8, 0)) {
                completedAccesses.fetch_add(1);
            }
        }
        });

    // Start concurrent access
    QElapsedTimer timer;
    timer.start();
    startFlag.store(true);

    thread1.join();
    thread2.join();

    qint64 totalTime = timer.nsecsElapsed();
    int totalAccesses = completedAccesses.load();

    qDebug() << "Concurrent access: " << totalAccesses << " accesses in " << totalTime << "ns";
    qDebug() << "Average time per access: " << (totalTime / totalAccesses) << "ns";

    // Should complete most accesses successfully
    QVERIFY(totalAccesses > (accessesPerThread * 2 * 0.95)); // 95% success rate

    // TLB system should still be intact
    QVERIFY(m_memorySystem->validateTLBSystemIntegrity());
}

// =======================
// INTEGRATION TESTS
// =======================

void TestAlphaMemorySystemTLB::testMemoryMapIntegration()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));

    // Test that memory map entries are automatically TLB-cached
    quint64 virtualAddr = 0x10000;
    quint64 physicalAddr = 0x20000;

    m_memorySystem->mapMemory(virtualAddr, physicalAddr, 0x1000, 0x7);

    // First access should populate TLB from memory map
    QElapsedTimer timer1;
    timer1.start();
    quint64 result;
    QVERIFY(m_memorySystem->readVirtualMemory(0, virtualAddr, result, 8, 0));
    qint64 firstTime = timer1.nsecsElapsed();

    // Second access should hit TLB
    QElapsedTimer timer2;
    timer2.start();
    QVERIFY(m_memorySystem->readVirtualMemory(0, virtualAddr, result, 8, 0));
    qint64 secondTime = timer2.nsecsElapsed();

    // Second access should be faster
    QVERIFY(secondTime < firstTime);

    qDebug() << "Memory map integration: first=" << firstTime << "ns, second=" << secondTime << "ns";
}

void TestAlphaMemorySystemTLB::testCacheCoherencyIntegration()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU2, 1));

    QSignalSpy coherencySpy(m_memorySystem, &AlphaMemorySystem::sigCacheCoherencyEvent);
    QSignalSpy tlbSpy(m_memorySystem, &AlphaMemorySystem::sigTlbInvalidated);

    quint64 virtualAddr = 0x10000;
    quint64 physicalAddr = 0x20000;

    m_memorySystem->mapMemory(virtualAddr, physicalAddr, 0x1000, 0x7);

    // Both CPUs read (populate TLBs)
    quint64 result;
    QVERIFY(m_memorySystem->readVirtualMemory(0, virtualAddr, result, 8, 0));
    QVERIFY(m_memorySystem->readVirtualMemory(1, virtualAddr, result, 8, 0));

    // CPU 0 writes (should trigger cache coherency)
    QVERIFY(m_memorySystem->writeVirtualMemory(0, virtualAddr, 0xDEADBEEF, 8, 0));

    // Should trigger cache coherency events
    QVERIFY(coherencySpy.count() > 0);

    qDebug() << "Cache coherency events:" << coherencySpy.count();
}

void TestAlphaMemorySystemTLB::testStatisticsIntegration()
{
    QVERIFY(m_memorySystem->registerCPU(m_mockCPU1, 0));

    // Generate some TLB activity
    m_memorySystem->mapMemory(0x10000, 0x20000, 0x1000, 0x7);

    quint64 result;
    for (int i = 0; i < 10; ++i) {
        QVERIFY(m_memorySystem->readVirtualMemory(0, 0x10000, result, 8, 0));
    }

    // Get system status
    QString status = m_memorySystem->getSystemStatus();

    // Should contain TLB-related information
    QVERIFY(status.contains("translations"));
    QVERIFY(status.contains("accesses"));

    qDebug() << "System status:" << status;

    // Dump system state
    m_memorySystem->dumpSystemState(); // Should not crash

    // Validate TLB integrity
    QVERIFY(m_memorySystem->validateTLBSystemIntegrity());
}

QTEST_MAIN(TestAlphaMemorySystemTLB)
#include "TestAlphaMemorySystemTLB.moc"

