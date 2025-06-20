#pragma once
#include <QtCore>
#include <QSharedPointer>
#include <QPromise>

struct CacheRequest
{
    enum Type
    {
        InstructionFetch,
        RegisterRead,
        RegisterWrite
    };
    Type type;
    quint64 address;
    quint8 registerNum;
    quint64 data;
    QSharedPointer<QPromise<bool>> promise;

    CacheRequest(Type t, quint64 addr) : type(t), address(addr) { promise = QSharedPointer<QPromise<bool>>::create(); }
};
