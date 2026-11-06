// LanguageRegistry: rozszerzenia, znane nazwy plików, shebang i kolejność
// wykrywania (D023, SPECYFIKACJA §3.6).

#include "services/LanguageRegistry.h"

#include <QtTest>

using namespace cynamoncode;

class TestLanguageRegistry : public QObject
{
    Q_OBJECT

private slots:
    void detectsByFileName_data();
    void detectsByFileName();
    void detectsByShebang_data();
    void detectsByShebang();
    void fileNameBeatsShebang();
    void unknownIsPlainText();
    void everyLanguageHasKeyAndName();
    void detectsByFirstLineWithoutExtension_data();
    void detectsByFirstLineWithoutExtension();
    void defaultExtensionRoundTrips();
};

void TestLanguageRegistry::detectsByFirstLineWithoutExtension_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("expected");
    const auto row = [](const char *name, const char *path, const char *line,
                        LanguageId language) {
        QTest::newRow(name) << QString::fromUtf8(path) << QString::fromUtf8(line)
                            << int(language);
    };
    // Zgłoszony przypadek: „Nowy 1” + „html” bez kropki.
    row("Nowy 1html", "/home/kamil/Nowy 1html", "<html>", LanguageId::Html);
    row("doctype", "strona", "<!DOCTYPE html>", LanguageId::Html);
    row("wielkie litery i spacje", "strona", "   <HTML lang=\"pl\">", LanguageId::Html);
    row("xml", "dane", "<?xml version=\"1.0\"?>", LanguageId::Xml);
    row("php", "skrypt", "<?php", LanguageId::Php);
    row("zwykły tekst", "notatka", "<to nie html>", LanguageId::Plain);
    // Rozszerzenie rozstrzyga: plik .txt z HTML-em pozostaje tekstem.
    row("txt z html", "notatki.txt", "<html>", LanguageId::Plain);
    row("py z html", "a.py", "<html>", LanguageId::Python);
}

void TestLanguageRegistry::detectsByFirstLineWithoutExtension()
{
    QFETCH(QString, path);
    QFETCH(QString, line);
    QFETCH(int, expected);
    QCOMPARE(int(LanguageRegistry::detect(path, line)), expected);
}

void TestLanguageRegistry::defaultExtensionRoundTrips()
{
    for (const LanguageId language : LanguageRegistry::all()) {
        const QString extension = LanguageRegistry::defaultExtension(language);
        if (language == LanguageId::Plain) {
            QVERIFY(extension.isEmpty());
            continue;
        }
        QVERIFY2(!extension.isEmpty(), qPrintable(LanguageRegistry::key(language)));
        QCOMPARE(int(LanguageRegistry::fromFileName(QStringLiteral("Nowy 1.") + extension)),
                 int(language));
    }
}

void TestLanguageRegistry::detectsByFileName_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<int>("expected");
    const auto row = [](const char *path, LanguageId language) {
        QTest::newRow(path) << QString::fromUtf8(path) << int(language);
    };
    row("/src/main.cpp", LanguageId::Cpp);
    row("a.c", LanguageId::Cpp);
    row("a.h", LanguageId::Cpp);
    row("a.HPP", LanguageId::Cpp);
    row("skrypt.py", LanguageId::Python);
    row("Main.java", LanguageId::Java);
    row("app.js", LanguageId::JavaScript);
    row("modul.mjs", LanguageId::JavaScript);
    row("index.html", LanguageId::Html);
    row("stara.HTM", LanguageId::Html);
    row("styl.css", LanguageId::Css);
    row("strona.php", LanguageId::Php);
    row("zapytanie.sql", LanguageId::Sql);
    row("dane.json", LanguageId::Json);
    row("pom.xml", LanguageId::Xml);
    row("ikona.svg", LanguageId::Xml);
    row("build.sh", LanguageId::Bash);
    row("/home/kamil/.bashrc", LanguageId::Bash);
    row(".profile", LanguageId::Bash);
    row("katalog.z.kropka/plik.py", LanguageId::Python);
}

void TestLanguageRegistry::detectsByFileName()
{
    QFETCH(QString, path);
    QFETCH(int, expected);
    QCOMPARE(int(LanguageRegistry::fromFileName(path)), expected);
    QCOMPARE(int(LanguageRegistry::detect(path, QString())), expected);
}

void TestLanguageRegistry::detectsByShebang_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("expected");
    const auto row = [](const char *line, LanguageId language) {
        QTest::newRow(line) << QString::fromUtf8(line) << int(language);
    };
    row("#!/usr/bin/python3", LanguageId::Python);
    row("#!/usr/bin/env python", LanguageId::Python);
    row("#!/usr/bin/env -S python3.12 -u", LanguageId::Python);
    row("#!/bin/bash", LanguageId::Bash);
    row("#!/bin/sh -e", LanguageId::Bash);
    row("#! /usr/bin/env bash", LanguageId::Bash);
    row("#!/usr/bin/env node", LanguageId::JavaScript);
    row("#!/usr/bin/php", LanguageId::Php);
    row("#!/usr/bin/perl", LanguageId::Plain);
    row("# zwykły komentarz", LanguageId::Plain);
}

void TestLanguageRegistry::detectsByShebang()
{
    QFETCH(QString, line);
    QFETCH(int, expected);
    QCOMPARE(int(LanguageRegistry::fromShebang(line)), expected);
    QCOMPARE(int(LanguageRegistry::detect(QStringLiteral("/bin/skrypt"), line)), expected);
}

void TestLanguageRegistry::fileNameBeatsShebang()
{
    QCOMPARE(int(LanguageRegistry::detect(QStringLiteral("x.sql"), QStringLiteral("#!/bin/bash"))),
             int(LanguageId::Sql));
}

void TestLanguageRegistry::unknownIsPlainText()
{
    QCOMPARE(int(LanguageRegistry::detect(QStringLiteral("notatki.txt"), QString())),
             int(LanguageId::Plain));
    QCOMPARE(int(LanguageRegistry::detect(QStringLiteral("Makefile"), QString())),
             int(LanguageId::Plain));
    QCOMPARE(int(LanguageRegistry::detect(QStringLiteral("archiwum.tar.gz"), QString())),
             int(LanguageId::Plain));
    QCOMPARE(int(LanguageRegistry::detect(QStringLiteral("bez-rozszerzenia."), QString())),
             int(LanguageId::Plain));
}

void TestLanguageRegistry::everyLanguageHasKeyAndName()
{
    QSet<QString> keys;
    for (const LanguageId language : LanguageRegistry::all()) {
        const QString key = LanguageRegistry::key(language);
        QVERIFY(!key.isEmpty());
        QVERIFY2(!keys.contains(key), qPrintable(key));
        keys.insert(key);
        QVERIFY(!LanguageRegistry::displayName(language).isEmpty());
    }
    QCOMPARE(keys.size(), 12);
}

QTEST_GUILESS_MAIN(TestLanguageRegistry)
#include "tst_language_registry.moc"
