#pragma once
enum class MachineCheckType : quint16
{
    /**
     * @brief No machine check error
     * Default state when no hardware errors are detected
     */
    NONE = 0x0000,

    // =================== Cache-Related Errors ===================

    /**
     * @brief Instruction cache parity error
     * Parity error detected in the primary instruction cache (I-Cache)
     * - Single-bit or multi-bit parity errors
     * - Cache line corruption
     */
    ICACHE_PARITY_ERROR = 0x0001,

    /**
     * @brief Data cache parity error
     * Parity error detected in the primary data cache (D-Cache)
     * - Write-through cache errors
     * - Cache coherency violations
     */
    DCACHE_PARITY_ERROR = 0x0002,

    /**
     * @brief Secondary cache error (S-Cache)
     * Error in the on-chip secondary cache (21164 and later)
     * - ECC errors in S-Cache
     * - Cache tag mismatches
     */
    SCACHE_ERROR = 0x0003,

    /**
     * @brief Backup cache error (B-Cache)
     * Error in external tertiary cache
     * - External SRAM errors
     * - Cache controller failures
     */
    BCACHE_ERROR = 0x0004,

    /**
     * @brief Cache tag error
     * Corruption in cache tag arrays
     * - Tag parity errors
     * - Invalid cache states
     */
    CACHE_TAG_ERROR = 0x0005,

    /**
     * @brief Cache coherency error
     * Cache coherency protocol violations
     * - Multi-processor cache conflicts
     * - Invalid cache line states
     */
    CACHE_COHERENCY_ERROR = 0x0006,

    // =================== Memory System Errors ===================

    /**
     * @brief System memory error
     * Errors in main system memory
     * - ECC uncorrectable errors
     * - Memory controller failures
     */
    SYSTEM_MEMORY_ERROR = 0x0010,

    /**
     * @brief Memory controller error
     * Failures in the memory interface logic
     * - Address/data bus errors
     * - Memory timing violations
     */
    MEMORY_CONTROLLER_ERROR = 0x0011,

    /**
     * @brief Translation Buffer error (TB/TLB)
     * Errors in the Translation Lookaside Buffer
     * - TLB parity errors
     * - Invalid translation entries
     */
    TRANSLATION_BUFFER_ERROR = 0x0012,

    /**
     * @brief Memory management unit error
     * MMU hardware failures
     * - Page table walker errors
     * - Virtual address translation failures
     */
    MMU_ERROR = 0x0013,

    // =================== Bus and Interface Errors ===================

    /**
     * @brief System bus error
     * Errors on the external system bus
     * - Bus parity errors
     * - Bus timeout errors
     * - Address/data bus corruption
     */
    SYSTEM_BUS_ERROR = 0x0020,

    /**
     * @brief I/O bus error
     * Errors on I/O buses (PCI, EISA, etc.)
     * - I/O bus parity errors
     * - I/O controller failures
     */
    IO_BUS_ERROR = 0x0021,

    /**
     * @brief External interface error
     * Errors in external chip interfaces
     * - Clock generation failures
     * - Signal integrity problems
     */
    EXTERNAL_INTERFACE_ERROR = 0x0022,

    /**
     * @brief Interprocessor communication error
     * Errors in multi-processor communication
     * - Inter-processor interrupt failures
     * - SMP coherency violations
     */
    INTERPROCESSOR_ERROR = 0x0023,

    DOUBLE_FAULT = 0x00231, // Exception during exception
    // =================== Processor Core Errors ===================

    /**
     * @brief Execution unit error
     * Errors in functional execution units
     * - Integer ALU failures
     * - Floating-point unit errors
     * - Pipeline corruption
     */
    EXECUTION_UNIT_ERROR = 0x0030,

    /**
     * @brief Instruction fetch error
     * Errors during instruction fetch operations
     * - PC generation errors
     * - Instruction decode failures
     */
    INSTRUCTION_FETCH_ERROR = 0x0031,

    /**
     * @brief Register file error
     * Corruption in processor register files
     * - Integer register parity errors
     * - FP register corruption
     */
    REGISTER_FILE_ERROR = 0x0032,

    /**
     * @brief Control logic error
     * Errors in processor control logic
     * - Microcode ROM errors
     * - Control state machine failures
     */
    CONTROL_LOGIC_ERROR = 0x0033,

    /**
     * @brief Pipeline error
     * Instruction pipeline integrity failures
     * - Pipeline stage corruption
     * - Hazard detection failures
     */
    PIPELINE_ERROR = 0x0034,

    // =================== Power and Environmental ===================

    /**
     * @brief Thermal error
     * Processor overheating conditions
     * - Temperature threshold exceeded
     * - Thermal sensor failures
     */
    THERMAL_ERROR = 0x0040,

    /**
     * @brief Power supply error
     * Power-related hardware failures
     * - Voltage regulation failures
     * - Power-on-reset problems
     */
    POWER_SUPPLY_ERROR = 0x0041,

    /**
     * @brief Clock error
     * Clock generation and distribution errors
     * - PLL failures
     * - Clock skew problems
     */
    CLOCK_ERROR = 0x0042,

    // =================== PALcode and System Errors ===================

    /**
     * @brief PALcode error
     * Errors in Privileged Architecture Library code
     * - PAL memory corruption
     * - PAL execution failures
     */
    PALCODE_ERROR = 0x0050,

    /**
     * @brief System chipset error
     * Errors in supporting chipset components
     * - Chipset register corruption
     * - System controller failures
     */
    SYSTEM_CHIPSET_ERROR = 0x0051,

    /**
     * @brief Firmware error
     * System firmware corruption or failure
     * - BIOS/SRM corruption
     * - Firmware checksum errors
     */
    FIRMWARE_ERROR = 0x0052,

    // =================== Uncategorized and Compound Errors ===================

    /**
     * @brief Uncorrectable error
     * Generic uncorrectable hardware error
     * - Errors that cannot be recovered
     * - Multiple simultaneous failures
     */
    UNCORRECTABLE_ERROR = 0x00F0,

    /**
     * @brief Machine check timeout
     * Machine check processing exceeded time limits
     * - Hardware watchdog timeout
     * - Error processing deadlock
     */
    MACHINE_CHECK_TIMEOUT = 0x00F1,

    /**
     * @brief Double machine check
     * Second machine check occurred during first one's processing
     * - Recursive hardware failures
     * - System in unstable state
     */
    DOUBLE_MACHINE_CHECK = 0x00F2,

    /**
     * @brief Unknown machine check
     * Unrecognized or vendor-specific error
     * - Non-standard error codes
     * - Future processor extensions
     */
    UNKNOWN_MACHINE_CHECK = 0x00FF,

    // =================== Alpha Model-Specific Errors ===================

    /**
     * @brief EV4 specific errors (21064)
     * Errors specific to Alpha 21064 processors
     */
    EV4_SPECIFIC_ERROR = 0x1000,

    /**
     * @brief EV5 specific errors (21164)
     * Errors specific to Alpha 21164 processors
     */
    EV5_SPECIFIC_ERROR = 0x1100,

    /**
     * @brief EV6 specific errors (21264)
     * Errors specific to Alpha 21264 processors
     */
    EV6_SPECIFIC_ERROR = 0x1200,

    /**
     * @brief EV7 specific errors (21364)
     * Errors specific to Alpha 21364 processors
     */
    EV7_SPECIFIC_ERROR = 0x1300,

    /**
     * @brief Maximum error code value
     * Used for bounds checking and validation
     */
    MAX_ERROR_CODE = 0xFFFF
};
