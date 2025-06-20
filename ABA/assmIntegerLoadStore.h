#pragma once
#include "Assembler.h"

namespace assemblerSpace {
    class assmIntegerLoadStore :
        public Assembler
    {
        assmIntegerLoadStore() = default;
        ~assmIntegerLoadStore() = default;


        //---------------------------------
            // Address Operations (LDA, LDAH)
            //---------------------------------

            /**
             * @brief LDA: R[ra] = R[rb] + sext(disp)
             */
        inline void emitLda(uint8_t ra, uint8_t rb, int32_t disp) {
            emitMovRegReg(HostReg::RAX, HostReg::GPR_BASE, rb);
            emitAddRegImm(HostReg::RAX, disp);
            emitStoreRegMem(HostReg::RAX, HostReg::GPR_BASE, ra, 64);
        }

        /**
         * @brief LDAH: R[ra] = R[rb] + (disp << 16)
         */
        inline void emitLdah(uint8_t ra, uint8_t rb, int16_t disp) {
            emitMovRegReg(HostReg::RAX, HostReg::GPR_BASE, rb);
            emitAddRegImm(HostReg::RAX, int32_t(disp) << 16);
            emitStoreRegMem(HostReg::RAX, HostReg::GPR_BASE, ra, 64);
        }
    };

}