#include "alphabasicblock.h"
/*#include "alphaInstruction.h"*/

AlphaBasicBlock::AlphaBasicBlock(quint64 startAddr, quint64 endAddr) : QObject(nullptr)
{
	this->startAddress = startAddr;
	this->endAddress = endAddr;
	this->executionCount = 0;
	this->fallThroughBlock = nullptr;
}

// AlphaBasicBlock::AlphaBasicBlock(QObject *parent)
// 	: QObject(parent)
// {}

QList<AlphaBasicBlock*> AlphaBasicBlock::getPrevBlocks() const
{
	return this->prevBlocks;
}

// Get the list of successor blocks
QList<AlphaBasicBlock*> AlphaBasicBlock::getNextBlocks() const
{
	return this->nextBlocks;
}

// Get the number of instructions in the block
int AlphaBasicBlock::length() const
{
	return this->instructions.size();
}


