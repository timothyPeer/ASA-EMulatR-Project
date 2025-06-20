#pragma once
// AssemblerBase.H
// Base class for JIT code emission: provides low-level byte emission, buffer management, and label fixups.
// Intel® 64 and IA-32 Architectures Software Developer’s Manual, Vol. 2A:
//   Machine-code encoding principles, §2.1 
//   Near relative jump (E9 rel32), §2.2 

#ifndef ASSEMBLERBASE_H
#define ASSEMBLERBASE_H

#include <cstdint>
#include <vector>
#include <cstddef>
#include <unordered_map>

namespace assemblerSpace {
	// Condition codes for conditional branches (0F 8x / 0F 8x opcodes)
	enum class ConditionBR : uint8_t {
		EQ, NE, LT, LE, GT, GE
	};

	/* enumerations: */

	   /// Integer?compare conditions ? low?level x86?64 Jcc opcodes (two?byte)
	enum class Condition : uint8_t {
		EQ = 0x84,  // JE
		NE = 0x85,  // JNE
		LT = 0x8C,  // JL (signed)
		LE = 0x8E,  // JLE
		GT = 0x8F,  // JG
		GE = 0x8D   // JGE
	};

	/// FP?compare conditions ? use same Jcc opcodes but based on FPSCR flags
	enum class FPCondition : uint8_t {
		E = 0x84,  // equal
		NE = 0x85,  // not?equal
		L = 0x8C,  // less
		LE = 0x8E,  // less?or?equal
		G = 0x8F,  // greater
		GE = 0x8D   // greater?or?equal
	};

    /**
     * A fixup entry for resolving a placeholder relative offset.
     * @offset  Byte?index in code buffer where the 32-bit displacement begins.
     * @label   Label identifier to which we must jump.
     */
    struct Fixup {
        size_t offset;
        size_t label;
    };

    /**
     * AssemblerBase
     *
     * Provides:
     *  - emitByte():     append a single machine?code byte
     *  - emitBytes():    append a sequence of bytes
     *  - label creation & binding
     *  - emitJmp():      emit a near jump with a 32-bit placeholder
     *  - resolveFixups(): patch all jump displacements once labels are bound
     */
    class AssemblerBase {
    public:

		/// Return the assembled byte stream.
		const std::vector<uint8_t>& data() const { return _buffer; }
       

        AssemblerBase() = default;
        virtual ~AssemblerBase() = default;



		

        /// Return the emitted machine-code buffer.
        inline const std::vector<uint8_t>& code() const {
            return codeBuffer;
        }

		/// Emit the low-order 'count' bits of 'value', MSB first, into the stream.
	    /// count must be in [1..32].
	    /// Bits are packed into bytes, high-bit first: bit0 -> 0x80 of next byte.
	    /// See ASA C-format: [opcode:6][ra:5][rb:5][function:6][unused:10] :contentReference[oaicite:1]{index=1}
		inline void emitBits(uint32_t value, int count) {
			// process from MSB of the field down to LSB
			for (int i = count - 1; i >= 0; --i) {
				bool bit = (value >> i) & 1;
				// shift into our pending byte
				_bitBuffer = (_bitBuffer << 1) | uint8_t(bit);
				++_bitCount;

				// once we have 8 bits, flush to buffer
				if (_bitCount == 8) {
					_buffer.push_back(_bitBuffer);
					_bitBuffer = 0;
					_bitCount = 0;
				}
			}
		}

		/// Emit a full byte into the code buffer.
		inline void emitByte(uint8_t b) {
			flushBits();           // align on byte boundary
			_buffer.push_back(b);
		}

        /**
         * Emit multiple bytes into the code buffer.
         * @param data  Pointer to byte array
         * @param len   Number of bytes to emit
         */
        inline void emitBytes(const uint8_t* data, size_t len) {
            codeBuffer.insert(codeBuffer.end(), data, data + len);
        }


		/// Finalize any remaining bits (pad low bits with zeroes).
		inline void flushBits() {
			if (_bitCount > 0) {
				// pad remaining bits to form a full byte
				_bitBuffer <<= (8 - _bitCount);
				_buffer.push_back(_bitBuffer);
				_bitBuffer = 0;
				_bitCount = 0;
			}
		}

        /**
         * Create a new label identifier.
         * The caller must bind it later at the target position.
         * @return  A unique label ID
         */
        inline size_t createLabel() {
            size_t lbl = nextLabel++;
            return lbl;
        }

        /**
         * Bind a previously created label to the current code offset.
         * @param lbl  Label ID from createLabel()
         */
        inline void bindLabel(size_t lbl) {
            labelPositions[lbl] = codeBuffer.size();
        }

        /**
        * emitJcc
        *   cond: which integer condition (signed)
        *   target: virtual?instruction address to jump to
        *
        * Encodes:
        *   0F opcode   ? two?byte escape
        *   [cond]      ? 0x84..0x8F from Condition
        *   rel32       ? 32?bit little?endian displacement to target
        */
        inline void emitJcc(Condition cond, uint64_t target) {
            // 1) two-byte escape + condition opcode
            emitByte(0x0F);
            emitByte(static_cast<uint8_t>(cond));
            // 2) placeholder for 32-bit displacement
            size_t pos = codeBuffer.size();
            emitBytes("\0\0\0\0", 4);
            // 3) record a fixup: when target label is bound, patch [pos..pos+3] with (target - (pos+4))
            fixups.push_back({ pos, target });
        }

        /**
         * emitFpJcc
         *   cond: which FP condition (based on FPSCR flags)
         *   target: virtual?instruction address to jump to
         *
         * Same encoding as emitJcc, but intended for FP branches (FBxx).
         */
        inline void emitFpJcc(FPCondition cond, uint64_t target) {
            emitJcc(static_cast<Condition>(cond), target);
        }


        /**
         * Emit a near relative jump to a label:
         *   opcode E9, followed by 32-bit little-endian placeholder.
         * Fixup will patch the 4 bytes to (target - (pos+4)).
         * @param lbl  Label ID to jump to
         */
        inline void emitJmp(size_t lbl) {
            // E9: near relative jump opcode 
            emitByte(0xE9);
            // record position for the 4-byte displacement
            size_t pos = codeBuffer.size();
            // placeholder bytes (will be overwritten by resolveFixups)
            emitBytes(reinterpret_cast<const uint8_t*>("\0\0\0\0"), 4);
            fixups.push_back(Fixup{ pos, lbl });
        }

        /**
         * After all code is emitted and labels bound, patch all jump displacements.
         * Must be called once (before executing or writing out the buffer).
         */
        inline void resolveFixups() {
            for (auto& f : fixups) {
                auto it = labelPositions.find(f.label);
                // If label not bound, skip or error
                if (it == labelPositions.end()) continue;
                size_t target = it->second;
                // displacement = target - (offset + 4)
                int32_t rel = static_cast<int32_t>(target) - static_cast<int32_t>(f.offset + 4);
                // write 32-bit little-endian
                codeBuffer[f.offset + 0] = static_cast<uint8_t>(rel & 0xFF);
                codeBuffer[f.offset + 1] = static_cast<uint8_t>((rel >> 8) & 0xFF);
                codeBuffer[f.offset + 2] = static_cast<uint8_t>((rel >> 16) & 0xFF);
                codeBuffer[f.offset + 3] = static_cast<uint8_t>((rel >> 24) & 0xFF);
            }
        }

		/// Build a REX prefix byte (64-bit mode):
	    ///   0100WRXB, where W=1 for 64-bit operand size,
	    ///   R=1 if srcReg?8, B=1 if dstReg?8
	    /// See Intel® SDM, “REX Prefix” :contentReference[oaicite:3]{index=3}
		inline uint8_t rexByte(bool w, int srcReg, int dstReg) {
			return static_cast<uint8_t>(
				0x40 |
				(w ? 0x08 : 0) |
				((srcReg & 0x8) ? 0x04 : 0) |
				((dstReg & 0x8) ? 0x01 : 0)
				);
		}

		/// Build a ModR/M byte for register-to-register operations:
        ///   mod=11? (register), reg=src, rm=dst
		inline uint8_t modRm(int dstReg, int srcReg) {
			return static_cast<uint8_t>(0xC0 | ((srcReg & 0x7) << 3) | (dstReg & 0x7));
		}


    protected:
        std::vector<uint8_t> codeBuffer;                   ///< Emitted bytes
        std::unordered_map<size_t, size_t> labelPositions; ///< Map label ? offset
        std::vector<Fixup> fixups;   
        
		std::vector<uint8_t> _buffer;  ///< emitted bytes
		uint8_t  _bitBuffer = 0;      ///< pending bits (high bits first)
		int      _bitCount = 0;      ///< number of bits in _bitBuffer///< Pending jump fixups

    private:
        size_t nextLabel = 0;  ///< For generating unique label IDs
    };
}
#endif // ASSEMBLERBASE_H

