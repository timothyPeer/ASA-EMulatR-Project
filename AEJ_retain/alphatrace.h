#pragma once
#include <QObject>
#include <QList>

class AlphaBasicBlock;
/**
 * Alpha Trace - represents a frequently executed path through the code
 */
class AlphaTrace : public QObject
{
    Q_OBJECT

private:
    QList<AlphaBasicBlock*> blocks;           // List of basic blocks in this trace
    int executionCount;                       // Number of times this trace has been executed
    std::function<void()> compiledCode;       // The optimized compiled code for this trace

public:
    /**
     * Constructor for the trace
     * @param startBlock - The first block in the trace
     */
    explicit AlphaTrace(AlphaBasicBlock* startBlock);

    /**
     * Add a block to the trace
     * @param block - The block to add
     */
    void addBlock(AlphaBasicBlock* block);

    /**
     * Get the blocks in the trace
     * @return The blocks in the trace
     */
    QList<AlphaBasicBlock*> getBlocks() const;

    /**
     * Get the start address of the trace
     * @return The start address
     */
    quint64 getStartAddress() const;

    /**
     * Get the end address of the trace
     * @return The end address
     */
    quint64 getEndAddress() const;

    /**
     * Increment the execution count for this trace
     */
    void incrementExecutionCount()
    {
        this->executionCount++;
    }

    /**
     * Check if trace is compiled
     * @return True if trace is compiled
     */
    bool getIsCompiled() const { return isCompiled; }

    /**
     * Get the compiled code
     * @return The compiled function
     */
    std::function<void()> getCompiledCode() const { return compiledCode; }

	bool isCompiled;                          // Whether this trace has been compiled

    /**
     * Check if this trace has been executed enough times to warrant compilation
     * @param threshold - The compilation threshold
     * @return True if the trace should be compiled
     */
    bool shouldCompile(int threshold) const
    {
        return this->executionCount >= threshold && !this->isCompiled;
    }

    /**
     * Mark the trace as compiled
     * @param compiledCode - The compiled code for this trace
     */
    void setCompiled(std::function<void()> compiledCode)
    {
        this->compiledCode = compiledCode;
        this->isCompiled = true;
    }
    /**
     * Get the execution count for this trace
     * @return The execution count
     */
    int getExecutionCount() const { return executionCount; }
};