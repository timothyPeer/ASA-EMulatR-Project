#ifndef JITBlock_h__
#define JITBlock_h__

#pragma once
#include <QVector>
#include <Qobject>
#include "RegisterFileWrapper.h"
#include "SafeMemory.h"

struct JITBlock {
	enum OpCodeClass {
		// Major opcode classifications
		OpPAL = 0x00,                // All PAL opcodes (0x00)
		OpMemory_Load = 0x08,        // Memory load operations (0x08-0x0F)
		OpInteger_Operate = 0x10,    // Integer operations (0x10-0x13)
		OpFP_Operate = 0x16,         // Floating-point operations (0x16-0x17)
		OpMemory_Barrier = 0x18,     // Memory barriers (0x18-0x19)
		OpJump_Branch = 0x1A,        // Control flow operations (0x1A-0x1F)
		OpMemory_FPLoad = 0x20,      // FP memory loads (0x20-0x23)
		OpMemory_FPStore = 0x24,     // FP memory stores (0x24-0x27)
		OpMemory_Store = 0x28,       // Integer memory stores (0x28-0x2F)
		OpControl_Branch = 0x30,     // Branch operations (0x30-0x3F)
		OpVector = 0x60,              // Vector operations (0x60)
		OpIntegerShift = 0x12		 // Integer Shift Operations

	};
	struct Operation {
		enum class OpType {
			// Integer arithmetic
			INT_ADD,
			INT_SUB,
			INT_MUL,
			INT_UMULH,

			// Integer logic
			INT_AND,
			INT_ORNOT,
			INT_BIC,
			INT_BIS,
			INT_XOR,

			// Byte Manipulation
			BYTE_INSERT,
			BYTE_MASK, 
			BYTE_EXTRACT,

			// Shifts
			INT_SLL,
			INT_SRL,
			INT_SRA,

			// Memory operations
			MEM_LDAH,
			MEM_LDQ,
			MEM_STQ,	// Store Quadword 
			MEM_LDL,
			MEM_STL,
			MEM_LDA,
			MEM_LDBU,
			MEM_LDWU,
			MEM_LDQ_U,  // Unaligned load quadword
			MEM_STB,
			MEM_STW,
			MEM_LDL_L,
			MEM_LDQ_L,
			MEM_PREFETCH,	// Prefetch Hint
			MEM_STL_C,	 
			MEM_STQ_C,	 // Store Quadword Unconditional
			MEM_STQ_U,	 // Store Quadword Unaligned

			// Unaligned memory access specialized operations
			MEM_UNALIGNED_LOAD_WORD,      // Unaligned 2-byte load
			MEM_UNALIGNED_LOAD_LONGWORD,  // Unaligned 4-byte load
			MEM_UNALIGNED_LOAD_QUADWORD,  // Unaligned 8-byte load
			MEM_UNALIGNED_STORE_WORD,     // Unaligned 2-byte store
			MEM_UNALIGNED_STORE_LONGWORD, // Unaligned 4-byte store
			MEM_UNALIGNED_STORE_QUADWORD, // Unaligned 8-byte store
			MEM_UNALIGNED_ACCESS,			// JIT Opimization

			// Branch Instructions
			BRANCH_BEQ,
			BRANCH_BNE,      // Branch if Not Equal
			BRANCH_BLT,      // Branch if Less Than
			BRANCH_BLE,      // Branch if Less than or Equal
			BRANCH_BGT,      // Branch if Greater Than
			BRANCH_BGE,      // Branch if Greater than or Equal
			BRANCH_BLBC,     // Branch if Low Bit Clear
			BRANCH_BLBS,     // Branch if Low Bit Set
            BRANCH_BR, 

			// Compare operations
			CMP_EQ,
			CMP_ULT,
			CMP_LE,
			CMP_LT,

			// Conditional move
			CMOVE_EQ,
			CMOVE_NE,
			CMOVE_GT,

			// Special operations (requiring system calls)
			SYS_MEMORY_BARRIER,
			SYS_CALL_PAL,
			SYS_TLB_OP,

			// Complex byte manipulation
			BYTE_ZAP,
	
			// Floating Point
			FP_ADD,
			FP_SUB,
			FP_MUL,
			FP_DIV,
			FP_CMP_EQ,
			FP_CMP_LT,
			FP_CMP_LE,
			FP_CVT,
			NOP, // No operation (used for removed operations)
			INT_CMOVE,
			FP_CMOVE,
			MVI_MAX,
			BWX_LDBU
			,
			// Byte manipulation operations
			BYTE_EXTBL,      // Extract Byte Low
			BYTE_EXTWL,      // Extract Word Low
			BYTE_EXTLL,      // Extract Longword Low
			BYTE_EXTQL,      // Extract Quadword Low
			BYTE_EXTBH,      // Extract Byte High
			BYTE_EXTWH,      // Extract Word High
			BYTE_EXTLH,      // Extract Longword High
			BYTE_EXTQH,      // Extract Quadword High

			BYTE_INSBL,      // Insert Byte Low
			BYTE_INSWL,      // Insert Word Low
			BYTE_INSLL,      // Insert Longword Low
			BYTE_INSQL,      // Insert Quadword Low

			BYTE_MSKBL,      // Mask Byte Low
			BYTE_MSKWL,      // Mask Word Low
			BYTE_MSKLL,      // Mask Longword Low
			BYTE_MSKQL,      // Mask Quadword Low
			
			// Atomics
            ATOMIC_OP, 
			// Fallback for anything else
			FALLBACK
		};

		OpType type;
		quint32 rawInstr;
		quint8 ra;
		quint8 rb;
		quint8 rc;
		quint32 function;  // For function code in instructions
		quint64 immediate; // For immediate values
		// For special operations that need custom handlers
		std::function<void(RegisterFileWrapper*, RegisterFileWrapper*, SafeMemory*)> specialHandler;
		UnalignedAccessContext unalignedContext  // Context for unaligned operations
	};

	quint64 startPC;
	QVector<Operation> operations;
	bool isFallback;
	bool containsSpecialOps;  // Flag for blocks with special operations
};
#endif // JITBlock_h__
