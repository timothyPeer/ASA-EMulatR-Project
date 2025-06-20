#pragma once
#ifndef Register_h__
#define Register_h__
#include <QObject>

/**
 * @brief Represents a register value in the Alpha CPU
 */
class Register {
public:
    enum Type {
        GENERAL,    // General purpose register
        SPECIAL,    // Special purpose register
        CONTROL,    // Control register
        FLOATING    // Floating point register
    };

    Register(quint64 value = 0, Type type = GENERAL)
        : m_value(value), m_type(type) {
    }

    quint64 value() const { return m_value; }
    void setValue(quint64 value) { m_value = value; }

    Type type() const { return m_type; }
    void setType(Type type) { m_type = type; }

private:
    quint64 m_value;
    Type m_type;
};
#endif // Register_h__