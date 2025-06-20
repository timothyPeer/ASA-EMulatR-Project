#pragma once
const int VECTOR_CLOCK = 0;              // Clock interrupt
const int VECTOR_IPI_RESCHEDULE = 1;     // IPI for rescheduling
const int VECTOR_IPI_CALLFUNCTION = 2;   // IPI for calling a function
const int VECTOR_IPI_INVALIDATE_TLB = 3; // IPI for TLB invalidation
const int VECTOR_DEVICE1 = 8;            // Device interrupts start here
                                         // Additional vectors...
