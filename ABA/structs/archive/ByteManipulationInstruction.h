#pragma once
// executorFmtByteManipulation.h
#pragma once
#include "Assembler.h"
#include "OperateInstruction.h"
#include "../helpers/IExecutor.h"

class ByteManipulationExecutor  : public IExecutor {

	Assembler& assembler;

	/// pack [opcode:6][rd:5][ra:5][width:6][pos:6]
	inline void emitMField(int rd, int ra, int width, int pos) {
		assembler.emitBits(0x12, 6);       // M-format group opcode
		assembler.emitBits(rd & 0x1F, 5);       // dest
		assembler.emitBits(ra & 0x1F, 5);       // srcA
		assembler.emitBits(width & 0x3F, 6);       // field width
		assembler.emitBits(pos & 0x3F, 6);       // bit position
		assembler.flushBits();                     // align next instr
	}

public:
	using handler = void(ByteManipulationExecutor::*)(const OperateInstruction&);

	inline uint8_t modRmGp(int dst, int src) {
		return static_cast<uint8_t>(0xC0 | ((src & 0x7) << 3) | (dst & 0x7));
	}

	ByteManipulationExecutor(Assembler& a) : as(a) {}

	void execute(const OperateInstruction& inst) {
		OperateInstruction i = inst;

		i.decode(i.opcode);

		// Map primary opcode to subtable index
		static const QVector<quint8> primaries = {  0x12, 0x13};
		int pidx = primaries.indexOf(i.opcode);
		if (pidx < 0) return;  // unsupported opcode

		// Lookup in subtable, then by function code
		const auto& sub = dispatchTable()[pidx];
		int fidx = i.fnc & 0x7F;       // lower 7 bits
		handler h = sub.at(fidx);      // safe: sub is QVector<handler>
		if (h) (this->*h)(i);
	}

	// Returns the singleton 2-D dispatch table
	static const QVector<QVector<handler>>& dispatchTable() {
		static const QVector<QVector<handler>> table = createDispatchTable();
		return table;
	}

	static QVector<QVector<handler>> createDispatchTable()
	{
		QVector<QVector<handler>> all(2);


		auto& t12 = all[0]; // primary opcode 0x12
		auto& t13 = all[1]; // primary opcode 0x13



		t12[0x6A] = &ByteManipulationExecutor::emitExtLh;  // ExtLh
		t12[0x26] = &ByteManipulationExecutor::emitExtLl;   // ExtLl
		t12[0x7A] = &ByteManipulationExecutor::emitExtQh;  // ExtQh
		t12[0x36] = &ByteManipulationExecutor::emitExtQl;   // ExtQl
		t12[0x5A] = &ByteManipulationExecutor::emitExtWh;   // ExtWh
		t12[0x16] = &ByteManipulationExecutor::emitExtWl;   // ExtWl	
		t12[0x0B] = &ByteManipulationExecutor::emitInsBl;   // InsBl
		t12[0x67] = &ByteManipulationExecutor::emitInsBh;  // InsBh
		t12[0x67] = &ByteManipulationExecutor::emitInsLh;  // InsLh
		t12[0x2B] = &ByteManipulationExecutor::emitInsLl;  // InsLl
		t12[0x77] = &ByteManipulationExecutor::emitInsQh;  // InsQh
		t12[0x3B] = &ByteManipulationExecutor::emitInsQl;  // InsQl
		t12[0x57] = &ByteManipulationExecutor::emitInsWh;  // InsWh
		t12[0x1B] = &ByteManipulationExecutor::emitInsWl;   // InsWl
		t12[0x72] = &ByteManipulationExecutor::emitMskQh;  // MskQh	
		t12[0x02] = &ByteManipulationExecutor::emitMskBl;  // MskBl
		t12[0x62] = &ByteManipulationExecutor::emitMskLh;  // MskLh
		t12[0x22] = &ByteManipulationExecutor::emitMskLl;  // MskLl
		t12[0x32] = &ByteManipulationExecutor::emitMskQl;  // MskQl
		t12[0x52] = &ByteManipulationExecutor::emitMskWh;  // MskWh
		t12[0x12] = &ByteManipulationExecutor::emitMskWl;  // MskWl


		return all;
	}
	/// Generic field?extract:  rd = (ra >> shift) & ((1<<width)-1)
	inline void extractField(int rd, int ra, int width, int shift) {
		as.movq(rd, ra);               // copy Ra?Rd
		as.shrq(rd, shift);            // Rd >>= shift
		uint64_t mask = ((1ull << width) - 1);
		as.movImm64( /*tmp=*/RCX, mask); // load mask constant into RCX
		as.andq(rd, /*tmp=*/RCX);      // Rd &= mask
	}

	/// Generic field?insert: 
	///    rd = (oldRd & ~mask) | ((ra>>srcShift)&mask)<<dstShift
	inline void insertField(int rd, int oldRd, int ra, int srcShift, int width, int dstShift) {
		// clear the target bits in oldRd
		as.movq(rd, oldRd);
		uint64_t clearMask = ~(((1ull << width) - 1) << dstShift);
		as.movImm64( /*tmp=*/RCX, clearMask);
		as.andq(rd, /*tmp=*/RCX);

		// build the value to insert
		as.movq(/*tmp=*/RDX, ra);
		as.shrq(/*tmp=*/RDX, srcShift);
		uint64_t valueMask = (1ull << width) - 1;
		as.movImm64( /*tmp2=*/R8, valueMask);
		as.andq(/*tmp=*/RDX, /*tmp2=*/R8);
		as.shlq(/*tmp=*/RDX, dstShift);

		// or into rd
		as.orq(rd, /*tmp=*/RDX);
	}


	// Extract Byte (EXT.BL, t12[0x6A])
	inline void emitExtBl(const OperateInstruction& i) {
		// rd = dest(), ra = srcA(), pos = low-6 bits of fnc field
		emitMField(i.dest(), i.srcA(), /*width=*/8, /*pos=*/i.fnc & 0x3F);
	}

	// Insert High-Byte (INS.BH, t12[0x67])
	inline void emitInsBh(const OperateInstruction& i) {
		emitMField(i.dest(), i.srcA(), /*width=*/8, /*pos=*/i.fnc & 0x3F);
	}

	// Mask Low-Halfword (MSK.LL, t12[0x22])
	inline void emitMskLl(const OperateInstruction& i) {
		emitMField(i.dest(), i.srcA(), /*width=*/16, /*pos=*/i.fnc & 0x3F);
	}

	// …and so on for ExtLh, ExtWh, InsLh, InsWl, MskQh, etc.…

	/// Generic mask?constant: rd = ((1<<width)-1) << shift
	inline void makeMask(int rd, int width, int shift) {
		uint64_t m = ((1ull << width) - 1) << shift;
		as.movImm64(rd, m);
	}

	// ---------------------------------------
	// Now your 20 handlers become one-liners:
	// ---------------------------------------

	// Extract Byte Leftmost  (fnc=0x06): width=8, shift=56
	inline void emitExtBl(const OperateInstruction& i) {
		extractField(i.dest(), i.srcA(), /*width*/8, /*shift*/56);
	}

	// Extract Byte Low      (fnc=0x16): width=8, shift=0
	inline void emitExtWl(const OperateInstruction& i) {
		extractField(i.dest(), i.srcA(), 8, 0);
	}

	// Extract Halfword High (fnc=0x6A): width=16, shift=48
	inline void emitExtLh(const OperateInstruction& i) {
		extractField(i.dest(), i.srcA(), 16, 48);
	}

	// Extract Halfword Low  (fnc=0x26): width=16, shift=0
	inline void emitExtLl(const OperateInstruction& i) {
		extractField(i.dest(), i.srcA(), 16, 0);
	}

	// Extract Word High     (fnc=0x7A): width=32, shift=32
	inline void emitExtQh(const OperateInstruction& i) {
		extractField(i.dest(), i.srcA(), 32, 32);
	}

	// Extract Word Low      (fnc=0x36): width=32, shift=0
	inline void emitExtQl(const OperateInstruction& i) {
		extractField(i.dest(), i.srcA(), 32, 0);
	}

	// Extract Double?Word High (fnc=0x5A): width=64, shift=0 (identity)
	inline void emitExtWh(const OperateInstruction& i) {
		// full 64-bit extract = just a copy
		as.movq(i.dest(), i.srcA());
	}

	// Insert Byte Leftmost  (fnc=0x0B): srcShift=56, width=8, dstShift=56
	inline void emitInsBl(const OperateInstruction& i) {
		insertField(i.dest(), /*old*/i.dest(), i.srcA(), 56, 8, 56);
	}

	// Insert Byte Low       (fnc=0x1B): srcShift=0, width=8, dstShift=0
	inline void emitInsWl(const OperateInstruction& i) {
		insertField(i.dest(), i.dest(), i.srcA(), 0, 8, 0);
	}

	inline void emitInsLh(const OperateInstruction& i) { insertField(i.dest(), i.dest(), i.srcA(), 48, 16, 48); }
	inline void emitInsLl(const OperateInstruction& i) { insertField(i.dest(), i.dest(), i.srcA(), 0, 16, 0); }
	inline void emitInsQh(const OperateInstruction& i) { insertField(i.dest(), i.dest(), i.srcA(), 32, 32, 32); }
	inline void emitInsQl(const OperateInstruction& i) { insertField(i.dest(), i.dest(), i.srcA(), 0, 32, 0); }
	inline void emitInsWh(const OperateInstruction& i) { /* for 64-bit you can simply movq */ as.movq(i.dest(), i.srcA()); }
	// …and so on for the other 13 handlers:

	

	inline void emitMskBl(const OperateInstruction& i) { makeMask(i.dest(), 8, 56); }
	inline void emitMskWl(const OperateInstruction& i) { makeMask(i.dest(), 8, 0); }
	inline void emitMskLh(const OperateInstruction& i) { makeMask(i.dest(), 16, 48); }
	inline void emitMskLl(const OperateInstruction& i) { makeMask(i.dest(), 16, 0); }
	inline void emitMskQh(const OperateInstruction& i) { makeMask(i.dest(), 32, 32); }
	inline void emitMskQl(const OperateInstruction& i) { makeMask(i.dest(), 32, 0); }
	inline void emitMskWh(const OperateInstruction& i) { makeMask(i.dest(), 64, 0); }
};


