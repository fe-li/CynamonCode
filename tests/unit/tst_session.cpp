// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// SessionService (D034): zapis i odczyt sesji JSON, odporność na uszkodzone
// lub obce dane, brak treści dokumentów w sesji.

#include "services/SessionService.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace cynamoncode;

namespace {

Session sample()
{
    Session session;
    session.panes[0].tabs = {{QStringLiteral("/home/kamil/a.cpp"), 10, 4, 3, QString()},
                             {QStringLiteral("/home/kamil/łódź.py"), 0, 0, 0, QStringLiteral("sql")}};
    session.panes[0].current = 1;
    session.panes[1].tabs = {{QStringLiteral("/home/kamil/a.cpp"), 200, 0, 180, QString()}};
    session.panes[1].current = 0;
    session.activePane = 1;
    session.workspaceFolder = QStringLiteral("/home/kamil/projekt");
    session.splitterSizes = {500, 400};
    return session;
}

} // namespace

class TestSession : public QObject
{
    Q_OBJECT

private slots:
    void jsonRoundTrip();
    void fileRoundTrip();
    void missingFileIsNoSession();
    void rejectsBrokenOrForeignData();
    void sanitizesValues();
};

void TestSession::jsonRoundTrip()
{
    const std::optional<Session> restored = SessionService::fromJson(SessionService::toJson(sample()));
    QVERIFY(restored);
    QVERIFY(*restored == sample());
}

void TestSession::fileRoundTrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("session.json"));
    QVERIFY(SessionService::save(path, sample()));
    const std::optional<Session> restored = SessionService::load(path);
    QVERIFY(restored);
    QVERIFY(*restored == sample());

    // Tylko ścieżki i pozycje — żadnych pól z treścią.
    const QByteArray json = SessionService::toJson(sample());
    QVERIFY(!json.contains("text"));
    QVERIFY(!json.contains("content"));
}

void TestSession::missingFileIsNoSession()
{
    QTemporaryDir dir;
    QVERIFY(!SessionService::load(dir.filePath(QStringLiteral("brak.json"))));
}

void TestSession::rejectsBrokenOrForeignData()
{
    QVERIFY(!SessionService::fromJson("{ to nie json"));
    QVERIFY(!SessionService::fromJson("[1, 2, 3]"));
    QVERIFY(!SessionService::fromJson("{\"version\": 99, \"panes\": []}"));
    QVERIFY(!SessionService::fromJson(QByteArray()));
}

void TestSession::sanitizesValues()
{
    const QByteArray json = R"({
        "version": 1,
        "activePane": 7,
        "panes": [
            {"current": 5, "tabs": [{"path": "/a", "line": -3, "index": -1},
                                     {"path": ""},
                                     {"line": 4}]},
            {"current": -9, "tabs": []},
            {"current": 0, "tabs": [{"path": "/trzeci-panel-ignorowany"}]}
        ]
    })";
    const std::optional<Session> session = SessionService::fromJson(json);
    QVERIFY(session);
    QCOMPARE(session->panes[0].tabs.size(), qsizetype(1)); // puste ścieżki pominięte
    QCOMPARE(session->panes[0].tabs.first().line, 0);
    QCOMPARE(session->panes[0].tabs.first().index, 0);
    QCOMPARE(session->panes[0].current, -1);
    QCOMPARE(session->panes[1].current, -1);
    QCOMPARE(session->activePane, 0);
}

QTEST_GUILESS_MAIN(TestSession)
#include "tst_session.moc"
