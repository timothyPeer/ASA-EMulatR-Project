#include "tulipnic.h"

// tulipnic_mmio.cpp
#include "tulipnic.h"

/**
 * MMIO overrides for TulipNIC: delegate to generic read/write methods
 */

uint8_t TulipNIC_DC21040::mmioReadUInt8(quint64 offset) {
    // Read one byte from CSR region
    quint64 val = read(offset, 1);
    return static_cast<uint8_t>(val & 0xFFu);
}

uint16_t TulipNIC_DC21040::mmioReadUInt16(quint64 offset) {
    // Read two bytes (little-endian)
    quint64 val = read(offset, 2);
    return static_cast<uint16_t>(val & 0xFFFFu);
}

uint32_t TulipNIC_DC21040::mmioReadUInt32(quint64 offset) {
    // Read four bytes (CSR registers are 32-bit)
    quint64 val = read(offset, 4);
    return static_cast<uint32_t>(val & 0xFFFFFFFFu);
}

quint64 TulipNIC_DC21040::mmioReadUInt64(quint64 offset) {
    // For completeness, read two consecutive CSRs if requested
    uint32_t low = mmioReadUInt32(offset);
    uint32_t high = mmioReadUInt32(offset + 4);
    return (static_cast<quint64>(high) << 32) | low;
}

void TulipNIC_DC21040::mmioWriteUInt8(quint64 offset, uint8_t value) {
    // Write one byte: read-modify-write the containing 32-bit CSR
    quint32 index = static_cast<quint32>(offset >> 2);
    uint32_t csrVal = csr[index];
    // Determine byte lane
    unsigned lane = offset & 0x3;
    uint32_t mask = 0xFFu << (lane * 8);
    csrVal = (csrVal & ~mask) | (static_cast<uint32_t>(value) << (lane * 8));
    write(offset & ~0x3, csrVal, 4);
}

void TulipNIC_DC21040::mmioWriteUInt16(quint64 offset, uint16_t value) {
    // Write two bytes: read-modify-write the containing 32-bit CSR
    quint32 index = static_cast<quint32>(offset >> 2);
    uint32_t csrVal = csr[index];
    unsigned lane = offset & 0x3;
    // If unaligned halfword spans boundary, split it
    if (lane <= 2) {
        uint32_t mask = 0xFFFFu << (lane * 8);
        csrVal = (csrVal & ~mask) | (static_cast<uint32_t>(value) << (lane * 8));
        write(offset & ~0x3, csrVal, 4);
    }
    else {
        // Splitting across two CSRs: write low byte then high byte
        mmioWriteUInt8(offset, static_cast<uint8_t>(value & 0xFF));
        mmioWriteUInt8(offset + 1, static_cast<uint8_t>(value >> 8));
    }
}

void TulipNIC_DC21040::mmioWriteUInt32(quint64 offset, uint32_t value) {
    // Aligned 32-bit write replaces whole CSR
    write(offset, value, 4);
}

void TulipNIC_DC21040::mmioWriteUInt64(quint64 offset, quint64 value) {
    // Split 64-bit write into two 32-bit writes (low then high)
    uint32_t low = static_cast<uint32_t>(value & 0xFFFFFFFFu);
    uint32_t high = static_cast<uint32_t>(value >> 32);
    mmioWriteUInt32(offset, low);
    mmioWriteUInt32(offset + 4, high);
}

void TulipNIC_DC21040::initRings(int entries, AlphaMemorySystem* memorySys)
{
	ringSize = entries;
	rxRing.resize(entries);
	txRing.resize(entries);

	// allocate guest-physical for the descriptor arrays
	quint64 bytes = entries * sizeof(TulipDesc);
	rxRingPhys = memorySys->allocateGuestPhysical(bytes);
	txRingPhys = memorySys->allocateGuestPhysical(bytes);

	// clear them in guest RAM
	memorySys->writeBytes(rxRingPhys,
		reinterpret_cast<const uint8_t*>(rxRing.data()),
		bytes);
	memorySys->writeBytes(txRingPhys,
		reinterpret_cast<const uint8_t*>(txRing.data()),
		bytes);

	// allocate per-descriptor packet buffers for RX
	for (int i = 0; i < entries; ++i) {
		quint64 bufPhy = memorySys->allocateGuestPhysical(2048);
		rxRing[i].bufferPhys = bufPhy;
		rxRing[i].status = 0x80000000u;  // owner = NIC
		rxRing[i].control = 2048;         // buffer length
	}
	// write the initialized ring back into guest RAM
	memorySys->writeBytes(rxRingPhys,
		reinterpret_cast<const uint8_t*>(rxRing.data()),
		bytes);

	// tell the NIC about the new ring
	writeCSR(CSR_RDP, static_cast<uint32_t>(rxRingPhys));
}


/*
    writeCSR is a mmioWriteUInt32 wrapper.
*/

void TulipNIC_DC21040::startDMA() {
	// CSR 0x10 = Receive List Pointer
	writeCSR(0x10, rxRingPhys);
	// CSR 0x14 = Transmit List Pointer
	writeCSR(0x14, txRingPhys);
	// CSR 0x00 = Command: enable RX and TX DMA
	writeCSR(0x00, CSR_CMD_RX_ON | CSR_CMD_TX_ON);  
}

void TulipNIC_DC21040::onRxComplete() {
	// Mark RFD owner → host
	rxRing[rxIndex].status |= DESC_STATUS_COMPLETE;
	// Advance ring
	rxIndex = (rxIndex + 1) & (ringSize - 1);
	// Raise interrupt via your IRQController
	irqController->raiseIRQ(irqLine);
}
uint16_t TulipNIC_DC21040::miiRead(int phyAddr, int reg) {
	// write MII command CSR (0x50) with phyAddr/reg and start bit
	writeCSR(0x50, buildMiiReadCmd(phyAddr, reg));
	// poll CSR “MII Status” until busy clears
	while (readCSR(0x54) & MII_STAT_BUSY) {}
	return readCSR(0x58) & 0xFFFF;
}

void TulipNIC_DC21040::initPHY() {
	// reset PHY
	miiWrite(phyAddr, MII_REG_BMCR, BMCR_RESET);
	// auto-negotiate
	miiWrite(phyAddr, MII_REG_BMCR, BMCR_ANENABLE | BMCR_ANRESTART);
	// wait for link
	while (!(miiRead(phyAddr, MII_REG_BMSR) & BMSR_LSTATUS)) {}
	// program MAC Speed/Duplex based on partner
}

void TulipNIC_DC21040::reset() {
	initRings(64, hostMemory);
	initPHY();
	startDMA();
}

void TulipNIC_DC21040::mmioWriteUInt32(quint64 off, uint32_t v) {
	writeCSR(off, v);
	if (off == MMIO_RCV_LISTPTR)   startDMA();
	if (off == MMIO_XMT_LISTPTR)   startDMA();
	if (off == MMIO_INT_MASK)      intMask = v;
}

void TulipNIC_DC21040::writeCSR(quint64 offset, uint32_t value) {
	// write into the MMIO region so CPU sees the CSR change
	memory->writeUInt32(mmioBase + offset, value);
	// then handle side-effects in the device (e.g. startDMA())
	onCSRWritten(offset, value);
}

uint32_t TulipNIC_DC21040NIC::readCSR(quint64 offset) {
	return memory->readUInt32(mmioBase + offset);
}

uint32_t TulipNIC_DC21040::mmioReadUInt32(quint64 offset) {
	return readCSR(offset);
}

void TulipNIC_DC21040::mmioWriteUInt32(quint64 offset, uint32_t value) {
	writeCSR(offset, value);
}

/// Called periodically or from your IRQ handler when Rx interrupts arrive
void TulipNIC_DC21040::serviceRx()
{
	// Loop until we find a descriptor still owned by the NIC
	while (true) {
		// Read the RFD from guest memory
		TulipDesc d;
		memory->readBytes(
			rxRingPhys + rxIndex * sizeof(TulipDesc),
			reinterpret_cast<uint8_t*>(&d),
			sizeof(d)
		);

		// Ownership bit = 1 means device still owns it → no more packets
		if (d.status & 0x80000000u)  // DescOwned bit :contentReference[oaicite:0]{index=0}
			break;

		// Extract the received length (low 16 bits of status)
		int length = d.status & 0xFFFF;

		// Copy packet payload into a Qt byte array
		QByteArray frame;
		frame.resize(length);
		memory->readBytes(
			d.bufferPhys,
			reinterpret_cast<uint8_t*>(frame.data()),
			length
		);

		// Emit to whoever is listening in Qt
		emit packetReceived(frame);

		// Return descriptor to NIC: set Owner=1, preserve control & bufferPhys
		d.status = 0x80000000u;
		memory->writeBytes(
			rxRingPhys + rxIndex * sizeof(TulipDesc),
			reinterpret_cast<uint8_t*>(&d),
			sizeof(d)
		);

		// Ensure the descriptor update is visible before ringing doorbell
		MEM_WMB();

		// Ring the Receive Descriptor Pointer so NIC sees the new descriptor
		memory->writeUInt32(mmioBase + CSR_RDP, rxIndex);

		// Advance index with wrap (ringSize is power-of-two)
		rxIndex = (rxIndex + 1) & (ringSize - 1);
	}
}

void TulipNIC_DC21040::injectFrame(const QByteArray& frame) {
	// Grab next free Rx descriptor
	TulipDesc d = rxRing[rxIndex];
	// Copy into guest RAM
	memory->writeBytes(d.bufferPhys,
		reinterpret_cast<const uint8_t*>(frame.constData()),
		frame.size());
	// Clear Owner=1 → give to host CPU (owner bit = 0)
	d.status = frame.size() & 0xFFFF;
	memory->writeBytes(rxRingPhys + rxIndex * sizeof(d),
		reinterpret_cast<uint8_t*>(&d), sizeof(d));
	MEM_WMB();
	// Ring RDP so the guest sees a packet ready
	memory->writeUInt32(mmioBase + CSR_RDP, rxIndex);
	rxIndex = (rxIndex + 1) & (ringSize - 1);
}


/*
To finish the DC21040 emulation you need to wire up:


Step	Feature	Description
A	Descriptor rings	Allocate Rx/Tx rings in guest RAM; write their physical addresses into the Tulip’s RLP/TRP CSRs.
B	DMA kick	After descriptor rings and buffers are in place, set the “Command” CSR bits (RX_ON, TX_ON) to start the hardware.
C	Rx/Tx service threads	In your device’s C++ class, poll or interrupt on “Owner” bits in descriptors. When the NIC “owns” a descriptor (i.e. has written a received packet), hand the buffer up to your network stack, then re-arm the descriptor for the next packet.
D	Interrupt integration	On completion of a packet or error, raise the IRQ via your IRQController. In your CSR reads, reflect and clear the interrupt status bits.
E	PHY/MII management	Implement MDIO reads/writes via the Tulip’s MII CSRs to reset the PHY, start auto-negotiation, poll link status, then program MAC speed/duplex.
F	Memory barriers & coherence	Surround your descriptor-ring updates with MEM_WMB()/MEM_RMB() so the device sees up-to-date pointers before you ring the doorbell CSR.
G	Host network hookup	Map guest-physical packet buffers into host memory, wrap them into Qt QByteArray or similar, and push them into your host’s packet-processing layer (pcap, TAP, or real NIC).
H	Statistics & error handling	Update the Tulip’s “Missed Frame,” “Late Collision,” and other statistic CSRs as you process packets.


We use memory->readBytes/writeBytes so all accesses go through your MMU/SafeMemory layer.

The high bit of status (0x8000 0000) is the “Owner” bit—when clear, the host owns it and can process the packet; when set, the NIC owns it
GitHub
.

After handing the packet to Qt via packetReceived, we set that bit back to 1 and write the descriptor back into guest RAM, then do a write‐memory‐barrier (MEM_WMB()) and ring the NIC’s Receive Descriptor Pointer CSR so it will resume DMA.

This routine can be invoked from your IRQ handler (when the NIC raises Rx interrupt) or polled on a timer.

// wherever you construct your TulipNIC:
TulipNIC* nic = new TulipNIC(
	irqController,
	mmioBase,        // the MMIO window you mapped for BAR0
	mmioSize,
	memorySystem     // <— pass your AlphaSystemMemory / SafeMemory here
);

// read a descriptor word from guest RAM:
TulipDesc desc;
memory->readBytes(rxRingPhys + i*sizeof(TulipDesc),
				  reinterpret_cast<uint8_t*>(&desc),
				  sizeof(desc));

// write it back when you change the Owner bit:
memory->writeBytes(rxRingPhys + i*sizeof(TulipDesc),
				   reinterpret_cast<const uint8_t*>(&desc),
				   sizeof(desc));



*/