#pragma once

#include <QObject>
#include <QList>

class alphaInstruction;

/**
 * Alpha Basic Block - represents a basic block of code in the program
 * A basic block is a sequence of instructions with a single entry point and a single exit point
 */
class AlphaBasicBlock : public QObject
{
    Q_OBJECT

private:
    QList<alphaInstruction*> instructions;  // List of instructions in this block
    quint64 startAddress;                   // Memory address where this block starts
    quint64 endAddress;                     // Memory address where this block ends
    quint64 executionCount;                     // Number of times this block has been executed
    AlphaBasicBlock* fallThroughBlock;      // Next block in sequential execution
    QList<AlphaBasicBlock*> branchTargets;  // Possible branch targets from this block
    QList<AlphaBasicBlock*> nextBlocks;     // 
    QList<AlphaBasicBlock*> prevBlocks;     // 
    friend class AlphaTrace;                // Allow AlphaTrace direct access to private members

public:
    /**
     * Constructor for a basic block
     * @param startAddr - The starting address of the block
     * @param endAddr - The ending address of the block
     */
    AlphaBasicBlock(quint64 startAddr, quint64 endAddr);
    ~AlphaBasicBlock();

    /**
     * Get the starting address of this block
     * @return The starting address
     */
    quint64 getStartAddress() 
    {
        return startAddress;
    }

    /**
     * Get the ending address of this block
     * @return The ending address
     */
    quint64 getEndAddress() 
    {
        return endAddress;
    }

    /**
     * Add an instruction to this block
     * @param instruction - The instruction to add
     */
    void addInstruction(alphaInstruction* instruction)
    {
        instructions.append(instruction);
    }

    /**
     * Get all instructions in this block
     * @return The list of instructions
     */
    QList<alphaInstruction*> getInstructions() const
    {
        return this->instructions;
    }

    /**
     * Set the fall-through successor block
     * @param block - The successor block
     */
    void setFallThroughBlock(AlphaBasicBlock* block)
    {
        fallThroughBlock = block;
    }

    /**
     * Get the fall-through successor block
     * @return The fall-through block
     */
    AlphaBasicBlock* getFallThroughBlock() 
    {
        return fallThroughBlock;
    }

    /**
     * Add a possible branch target for this block
     * @param target - The target block
     */
    void addBranchTarget(AlphaBasicBlock* target)
    {
        this->branchTargets.append(target);
    }

    /**
     * Get all possible branch targets from this block
     * @return List of branch targets
     */
    QList<AlphaBasicBlock*> getBranchTargets() const
    {
        return branchTargets;
    }

    /**
     * Increment the execution count for this block
     */
    void incrementExecutionCount()
    {
        this->executionCount++;
    }

    /**
     * Get the number of times this block has been executed
     * @return The execution count
     */
    int getExecutionCount() const
    {
        return executionCount;
    }
    QList<AlphaBasicBlock*> getPrevBlocks() const;
    QList<AlphaBasicBlock*> getNextBlocks() const;
    int length() const;

    bool isCompiled;
};