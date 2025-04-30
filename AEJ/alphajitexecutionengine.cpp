#include "alphajitexecutionengine.h"

// AlphaJITExecutionEngine::AlphaJITExecutionEngine(QObject *parent)
// 	: QObject(parent)
// {}

QMap<quint64, AlphaBasicBlock*>& AlphaJITExecutionEngine::getBasicBlocks()  
{
	return basicBlocks;
}

QMap<QString, AlphaTrace*>& AlphaJITExecutionEngine::getTraces() 
{
	return traces;
}

