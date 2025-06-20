#include "AlphaCPU_refactored.h"
#include "RegisterFileWrapper.h"
#include "AlphaMemorySystem_refactored.h"
#include "tlbSystem.h"


// In test_instructions.cpp
// TEST(StqInstructionTest, BasicStoreTest) {
// 	// Setup
// 	AlphaCPU cpu;
// 	RegisterFileWrapper regs(&cpu);
// 	TLBSystem tlb;
// 	AlphaMemorySystem memSys;
// 
// 	// Setup memory with test data
// 	memSys.attachSafeMemory(new SafeMemory());
// 	memSys.getSafeMemory()->resize(1024 * 1024);
// 
// 	// Create instruction
// 	StqInstruction stq(0x27B00000); // STQ R27, 0(R0)
// 
// 	// Set PC and source register value
// 	stq.SetPC(0x1000);
// 	regs.writeIntReg(27, 0x12345678ABCDEF00);
// 
// 	// Execute instruction
// 	stq.Execute(&regs, &memSys, &tlb);
// 
// 	// Verify the result
// 	quint64 result;
// 	ASSERT_TRUE(memSys.readVirtualMemory(&cpu, 0, &result, 8));
// 	ASSERT_EQ(result, 0x12345678ABCDEF00);
// }
