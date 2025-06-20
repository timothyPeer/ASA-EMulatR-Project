#include "alphatrace.h"
#include "alphabasicblock.h" // Now include the full header

// Constructor
AlphaTrace::AlphaTrace(AlphaBasicBlock* startBlock) : QObject(nullptr)
{
	this->blocks = QList<AlphaBasicBlock*>() << startBlock;
	this->executionCount = 0;
	this->compiledCode = nullptr;
	this->isCompiled = false;
}

// Add a block to the trace
void AlphaTrace::addBlock(AlphaBasicBlock* block)
{
	this->blocks.append(block);
}

// Get the blocks in the trace
QList<AlphaBasicBlock*> AlphaTrace::getBlocks() const
{
	return this->blocks;
}


// Get the start address of the trace
quint64 AlphaTrace::getStartAddress() const
{
	return this->blocks.first()->startAddress;
}

// Get the end address of the trace
quint64 AlphaTrace::getEndAddress() const
{
	return this->blocks.last()->endAddress;
}



