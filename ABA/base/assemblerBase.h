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
        AssemblerBase() = default;
        virtual ~AssemblerBase() = default;

        /// Return the emitted machine-code buffer.
        inline const std::vector<uint8_t>& code() const {
            return codeBuffer;
        }

        /// Emit a single byte into the code buffer.
        inline void emitByte(uint8_t b) {
            codeBuffer.push_back(b);
        }

        /**
         * Emit multiple bytes into the code buffer.
         * @param data  Pointer to byte array
         * @param len   Number of bytes to emit
         */
        inline void emitBytes(const uint8_t* data, size_t len) {
            codeBuffer.insert(codeBuffer.end(), data, data + len);
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

    protected:
        std::vector<uint8_t> codeBuffer;                   ///< Emitted bytes
        std::unordered_map<size_t, size_t> labelPositions; ///< Map label ? offset
        std::vector<Fixup> fixups;                         ///< Pending jump fixups

    private:
        size_t nextLabel = 0;  ///< For generating unique label IDs
    };
}
#endif // ASSEMBLERBASE_H

