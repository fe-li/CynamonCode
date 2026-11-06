// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Smoke test zależności: QScintilla w wariancie Qt6 kompiluje się, linkuje
// i ładuje w procesie bez bibliotek Qt5. Nie tworzy instancji QsciScintilla —
// konfiguracja edytora (z wyłączonymi podpowiedziami) należy do E1.

#include <Qsci/qsciglobal.h>
#include <Qsci/qsciscintilla.h>

#include <QFile>
#include <QLibraryInfo>
#include <QMetaObject>
#include <QVersionNumber>
#include <QWidget>
#include <QtTest>

class TestQScintillaLink : public QObject
{
    Q_OBJECT

private slots:
    void reportsVersion();
    void linksAgainstLibrary();
    void runtimeIsQt6();
    void noQt5LibrariesLoaded();
};

void TestQScintillaLink::reportsVersion()
{
    qInfo("QScintilla %s, Qt %s", QSCINTILLA_VERSION_STR, qVersion());
    QVERIFY(QSCINTILLA_VERSION > 0);
}

void TestQScintillaLink::linksAgainstLibrary()
{
    // Odwołanie do staticMetaObject wymusza rzeczywiste zlinkowanie biblioteki.
    const QMetaObject &meta = QsciScintilla::staticMetaObject;
    QCOMPARE(QLatin1String(meta.className()), QLatin1String("QsciScintilla"));
    QVERIFY(meta.inherits(&QWidget::staticMetaObject));
}

void TestQScintillaLink::runtimeIsQt6()
{
    QCOMPARE(QLibraryInfo::version().majorVersion(), 6);
    QCOMPARE(QT_VERSION_MAJOR, 6);
}

void TestQScintillaLink::noQt5LibrariesLoaded()
{
    QFile maps(QStringLiteral("/proc/self/maps"));
    if (!maps.open(QIODevice::ReadOnly | QIODevice::Text))
        QSKIP("Brak /proc/self/maps — ten test wymaga Linuksa.");

    const QByteArray loaded = maps.readAll();
    QVERIFY2(!loaded.contains("libQt5"), "W procesie załadowano bibliotekę Qt5.");
    QVERIFY2(loaded.contains("qscintilla2_qt6"), "Nie załadowano biblioteki QScintilla dla Qt6.");
}

QTEST_GUILESS_MAIN(TestQScintillaLink)
#include "tst_qscintilla_link.moc"
