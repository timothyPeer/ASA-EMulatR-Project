#pragma once

#include <QObject>
#include "helpers.h"
#include "RegisterBank.h"
#include "FpRegisterBankCls.h"
#include "AlphaMemorySystem.h"
#include <QScopedPointer>

class AlphaCPUInterface;
class VectorExecutorPrivate;

/**
 * @brief Executes vector and SIMD-like instructions for Alpha.
 *        Uses a d-pointer (pimpl) to hide implementation details.
 */
class VectorExecutor : public QObject {
    Q_OBJECT
        Q_DECLARE_PRIVATE(VectorExecutor)
public:
    explicit VectorExecutor(AlphaCPUInterface* cpu,
        AlphaMemorySystem* memory,
        RegisterBank* regs,
        FpRegisterBankcls* fpRegs,
        QObject* parent = nullptr);
    ~VectorExecutor() override;

    // Base vector ops
    void execVLD(const helpers_JIT::OperateInstruction& op);
    void execVST(const helpers_JIT::OperateInstruction& op);
    void execVADD(const helpers_JIT::OperateInstruction& op);
    void execVSUB(const helpers_JIT::OperateInstruction& op);
    void execVAND(const helpers_JIT::OperateInstruction& op);
    void execVOR(const helpers_JIT::OperateInstruction& op);
    void execVXOR(const helpers_JIT::OperateInstruction& op);
    void execVMUL(const helpers_JIT::OperateInstruction& op);

    // BWX extensions
    void execLDBU(const helpers_JIT::OperateInstruction& op);
    void execLDWU(const helpers_JIT::OperateInstruction& op);
    void execSTB(const helpers_JIT::OperateInstruction& op);
    void execSTW(const helpers_JIT::OperateInstruction& op);
    void execSEXTW(const helpers_JIT::OperateInstruction& op);
    void execSEXTBU(const helpers_JIT::OperateInstruction& op);

    // MVI extensions
    void execMAXSB8(const helpers_JIT::OperateInstruction& op);
    void execMINUB8(const helpers_JIT::OperateInstruction& op);
    // ... other MVI methods as needed

signals:
    void registerUpdated(int reg, quint64 value);
    void memoryAccessed(quint64 addr, bool isWrite, int size);

private:
    QScopedPointer<VectorExecutorPrivate> d_ptr;
};
