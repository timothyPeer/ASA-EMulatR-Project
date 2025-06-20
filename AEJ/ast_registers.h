#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>
#include "ASA_nameSpaces.h"
#include "AsaNamespaces.h"

/**
 * @file ast_registers.h
 * @brief Implementation of ASTEN and ASTSR registers for AST management (Qt style)
 */

#ifndef AST_REGISTERS_H
#define AST_REGISTERS_H



/**
 * @brief Base class for AST registers (Qt style)
 */
class AstRegisterBase : public QObject
{
    Q_OBJECT

  protected:
    QVector<QVector<bool>> bits;

  public:
    explicit AstRegisterBase(QObject *parent = nullptr)
        : QObject(parent),
          bits(static_cast<int>(enumProcessorMode::MAX_MODES), QVector<bool>(static_cast<int>(AstLevel::MAX_LEVELS), false))
    {
    }

    bool getBit(enumProcessorMode mode, AstLevel level) const
    {
        return bits[static_cast<int>(mode)][static_cast<int>(level)];
    }

    void setBit(enumProcessorMode mode, AstLevel level, bool value)
    {
        bits[static_cast<int>(mode)][static_cast<int>(level)] = value;
    }

    quint32 getRawValue() const
    {
        quint32 value = 0;
        for (int mode = 0; mode < static_cast<int>(enumProcessorMode::MAX_MODES); ++mode)
        {
            for (int level = 0; level < static_cast<int>(AstLevel::MAX_LEVELS); ++level)
            {
                if (bits[mode][level])
                {
                    quint32 bitPos = mode * static_cast<int>(AstLevel::MAX_LEVELS) + level;
                    value |= (1U << bitPos);
                }
            }
        }
        return value;
    }

    void setRawValue(quint32 value)
    {
        for (int mode = 0; mode < static_cast<int>(enumProcessorMode::MAX_MODES); ++mode)
        {
            for (int level = 0; level < static_cast<int>(AstLevel::MAX_LEVELS); ++level)
            {
                quint32 bitPos = mode * static_cast<int>(AstLevel::MAX_LEVELS) + level;
                bits[mode][level] = (value & (1U << bitPos)) != 0;
            }
        }
    }
};

/**
 * @brief AST Enable (ASTEN) register
 */
class AstenRegister : public AstRegisterBase
{
    Q_OBJECT
  public:
    explicit AstenRegister(QObject *parent = nullptr) : AstRegisterBase(parent) {}

    bool isEnabled(enumProcessorMode mode, AstLevel level) const { return getBit(mode, level); }

    void enable(enumProcessorMode mode, AstLevel level) { setBit(mode, level, true); }

    void disable(enumProcessorMode mode, AstLevel level) { setBit(mode, level, false); }
};

/**
 * @brief AST Status (ASTSR) register
 */
class AstSrRegister : public AstRegisterBase
{
    Q_OBJECT
  public:
    explicit AstSrRegister(QObject *parent = nullptr) : AstRegisterBase(parent) {}

    bool isPending(enumProcessorMode mode, AstLevel level) const { return getBit(mode, level); }

    void setPending(enumProcessorMode mode, AstLevel level) { setBit(mode, level, true); }

    void clearPending(enumProcessorMode mode, AstLevel level) { setBit(mode, level, false); }
};

/**
 * @brief AST Manager to coordinate ASTEN/ASTSR interaction
 */
class AstManager : public QObject
{
    Q_OBJECT

  private:
    AstenRegister asten;
    AstSrRegister astsr;
    AsaModes::enumProcessorMode currentMode;

  public:
    explicit AstManager(QObject *parent = nullptr)
        : QObject(parent), asten(this), astsr(this), currentMode(AsaModes::enumProcessorMode::KERNEL)
    {
    }

    AstenRegister &getAsten() { return asten; }
    AstSrRegister &getAstsr() { return astsr; }

    void setCurrentMode(AsaModes::enumProcessorMode mode) { currentMode = mode; }
    AsaModes::enumProcessorMode getCurrentMode() const { return currentMode; }

    bool swasten(AsaModes::enumProcessorMode mode, AstLevel level, bool enable)
    {
        bool previousState = asten.isEnabled(mode, level);
        asten.setBit(mode, level, enable);

        if (enable && astsr.isPending(mode, level) && mode == currentMode)
        {
            deliverAst(mode, level);
        }

        return previousState;
    }

    void requestAst(AsaModes::enumProcessorMode mode, AstLevel level)
    {
        astsr.setPending(mode, level);

        if (asten.isEnabled(mode, level) && mode == currentMode)
        {
            deliverAst(mode, level);
        }
    }

  private:
    void deliverAst(AsaModes::enumProcessorMode mode, AstLevel level)
    {
        astsr.clearPending(mode, level);
        // Real handler logic would go here (PAL AST delivery, context switch)
    }
};

#endif // AST_REGISTERS_H
