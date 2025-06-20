#pragma once
// tst_assembler.cpp
#include <QByteArray>
#include <QtCore>
#include <QtTest>
#include "../ABA/assembler.h"

using Assembler = assemblerSpace::Assembler;

class TestAssembler : public QObject {
	Q_OBJECT
private slots:
	void test_addss_bytes() {
		Assembler as;
		as.movss(2, 3);     // XMM2 ? XMM3
		as.addss(2, 3);     // XMM2 += XMM3

		const auto& code = as.code();
		QByteArray actual(reinterpret_cast<const char*>(code.data()), code.size());
		QByteArray expect;
		expect += (char(0xF3));   // MOVSS prefix
		expect += (char(0x0F));
		expect += (char(0x10));
		expect += (char(0xC3));   // modRm(2,3)
		expect += (char(0xF3));   // ADDSS prefix
		expect += (char(0x0F));
		expect += (char(0x58));
		expect += (char(0xC3));
		QCOMPARE(actual, expect);
	}

	void test_modRm_encoding_data() {
		QCOMPARE(modRm(5, 6), quint8(0xC0 | (6 << 3) | 5));
	}

	// … repeat for all primitives: movsd, subss, cmovz, etc. …
};

QTEST_MAIN(TestAssembler)

