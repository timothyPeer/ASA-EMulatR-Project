#pragma once
#include <QObject>

class IllegalInstructionException : public std::runtime_error
{
public:
	IllegalInstructionException(const std::string& msg, quint64 pc) : std::runtime_error(msg), m_programCounter(pc) {}

	quint64 getProgramCounter() const { return m_programCounter; }

private:
	quint64 m_programCounter;
};