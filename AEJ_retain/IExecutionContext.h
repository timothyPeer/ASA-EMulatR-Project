// IExecutionContext.h
#pragma once
#include <cstdint>
#include <cstring>
#include "fpRegisterBankCls.h"



/**
	 * @brief Types of traps/exceptions that can occur during execution
	 */
enum class TrapType {
	PrivilegeViolation,    ///< Access violation due to privilege level
	MMUAccessFault,        ///< Memory management unit fault
	FloatingPointDisabled, ///< FP instruction when FP disabled
	ReservedInstruction,   ///< Unimplemented instruction
	SoftwareInterrupt,
	ArithmeticTrap,
	Breakpoint,
	DivideByZero_int
};

struct OperateInstruction;

class SafeMemory;
class FpcrRegister;
class RegisterBank;
class FpRegisterBank;

class IExecutionContext {
public:
	virtual ~IExecutionContext() {}

	virtual quint64 getPC() const = 0;
	virtual void setPC(quint64 pc) = 0;

	virtual bool translate(quint64 vAddr, quint64& pAddr, int accessType) = 0;

	// Optional: direct access to internal helpers
	virtual SafeMemory* getSafeMemory() { return nullptr; }
	virtual RegisterBank* getIntRegisterBank() { return nullptr; }
	virtual FpRegisterBankcls* getFpRegisterBank() = 0;
	virtual FpcrRegister* getFpcr() { return nullptr; }

	// Register access
	virtual uint64_t	readIntReg(unsigned idx) = 0;
	virtual void		writeIntReg(unsigned idx, uint64_t value) = 0;
	virtual double		readFpReg(unsigned idx) = 0;
	virtual void		writeFpReg(unsigned idx, double value) = 0;

	// Memory
	virtual bool      readMemory(uint64_t addr, void* buf, size_t size) = 0;
	virtual bool      writeMemory(uint64_t addr, void* buf, size_t size) = 0;

	// Control/status
	virtual void      raiseTrap(int trapCode) = 0;

	// Events (to UI, logs…)
	virtual void    notifyRegisterUpdated(bool isFp, unsigned idx, uint64_t rawValue) = 0;
	virtual void	notifyRegisterUpdated(bool isFp, quint8 reg, quint64 value)= 0;

	virtual void	notifyMemoryAccessed(quint64 addr, quint64 value, bool isWrite) = 0;

	//virtual void	notifyTrapRaised(helpers_JIT::TrapType type) = 0;
	virtual void	notifyTrapRaised(quint64 type) = 0;
	virtual void	notifyFpRegisterUpdated(unsigned idx, double value) = 0;
	virtual void	notifyIllegalInstruction(quint64 instructionWord, quint64 pc) = 0;
	virtual void	notifyReturnFromTrap() = 0;
	
};

/*
The interface is illustrated as such:

+---------------+        +------------------+
|   AlphaSMPMgr |<>------|    AlphaCPU      |
+---------------+        +------------------+
							  ^   ^   ^
	   owns & implements      |   |   |
							  |   |   |
			   +--------------+   |   +--------------+
			   |                  |                  |
	 +----------------+    +----------------+   +----------------+
	 | IntegerExecutor|    |VectorExecutor  |   |FloatingExecutor|
	 +----------------+    +----------------+   +----------------+
			 \                    |                     /
			  \                   |                    /
			   +--------------------------------------+
			   |         IExecutionContext            |
			   +--------------------------------------+


*/