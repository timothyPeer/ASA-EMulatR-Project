#pragma once
#include <QString>
#include <QtGlobal>
#include "PciWindow.h"

/*────────────────────────────  CSR - window  ────────────────────────────────*/
class CsrWindow : public PciWindow
{
public:
	CsrWindow(const QString& tag, quint64 base, quint64 size)
		: PciWindow(-1, base, size, Kind::CSR), m_tag(tag) {
	}

	quint64 toBusAddr(quint64 pa) const override
	{
		return pa - base();
	}

	const QString& tag() const { return m_tag; }

private:
	QString m_tag;
};