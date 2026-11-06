// Indentation::usesTabs — wykrywanie stylu wcięć istniejącego pliku (D024).

#include "services/Indentation.h"

#include <QtTest>

using namespace cynamoncode;

class TestIndentation : public QObject
{
    Q_OBJECT

private slots:
    void detectsTabs_data();
    void detectsTabs();
};

void TestIndentation::detectsTabs_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("tabs");
    QTest::newRow("pusty") << QString() << false;
    QTest::newRow("bez wcięć") << QStringLiteral("a\nb\n") << false;
    QTest::newRow("tabulatory") << QStringLiteral("f() {\n\tx;\n\ty;\n}\n") << true;
    QTest::newRow("spacje") << QStringLiteral("f() {\n    x;\n    y;\n}\n") << false;
    QTest::newRow("przewaga tabulatorów") << QStringLiteral("\ta\n\tb\n  c\n") << true;
    QTest::newRow("przewaga spacji") << QStringLiteral("\ta\n  b\n  c\n") << false;
    QTest::newRow("remis") << QStringLiteral("\ta\n  b\n") << false;
    QTest::newRow("pojedyncza spacja nie liczy się")
        << QStringLiteral("/*\n * opis\n */\n\tx;\n") << true;
    QTest::newRow("crlf") << QStringLiteral("a\r\n\tb\r\n") << true;
}

void TestIndentation::detectsTabs()
{
    QFETCH(QString, text);
    QFETCH(bool, tabs);
    QCOMPARE(Indentation::usesTabs(text), tabs);
}

QTEST_GUILESS_MAIN(TestIndentation)
#include "tst_indentation.moc"
