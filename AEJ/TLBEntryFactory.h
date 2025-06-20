// TLBEntryFactory.h
#pragma once

#include "InstructionTLB.h"
#include <QObject>

/**
 * @file TLBEntryFactory.h
 * @brief Factory for creating heap-allocated TLBEntry QObjects with proper ownership.
 *
 * QObjects allocated with `new` are not destroyed automatically when the creating function
 * exits. They are only deleted when:
 *   1. You call `delete` on them manually, or
 *   2. They have a non-null parent QObject—then Qt’s parent-child mechanism deletes them
 *      when the parent is destroyed.
 *
 * If you omit the parent, the caller must delete the returned pointer to avoid leaks.
 *
 * @see Alpha System Architecture Manual v6 §4.2.3 (TLB entry format)
 * @see Qt Documentation “QObject Memory Management” (https://doc.qt.io/qt-5/qobject.html#details)
 */

class TLBEntryFactory
{
  public:
    /**
     * @brief Creates a new TLBEntry on the heap and assigns it an optional QObject parent.
     * @param slot    Which TLB slot to initialize (0-based).
     * @param parent  Optional QObject parent; if provided, the parent will delete the entry.
     * @return        Pointer to the new InstructionTLB::TLBEntry.
     *                Ownership is transferred to `parent` if non-null; otherwise,
     *                the caller is responsible for calling `delete`.
     */
    static TLBEntry *createEntry(int slot, QObject *parent = nullptr)
    {
        // Allocate on the heap; passing `parent` enables automatic deletion.
        TLBEntry *entry = new TLBEntry(parent);

        // TODO: populate the entry based on 'slot'
        entry->setSlotIndex(slot);
        entry->setVirtualAddress(/* … */);
        entry->setPhysicalAddress(/* … */);

        return entry;
    }
};
