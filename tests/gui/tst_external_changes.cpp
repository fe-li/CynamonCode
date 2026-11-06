// Zewnętrzne zmiany otwartych plików w oknie głównym (D026, D027; TESTY.md T18,
// T26): zawsze pytanie, przeładowanie lub zachowanie zmian, konflikt przy zapisie,
// brak alarmu po własnym zapisie, usunięty plik chroniony przy zamykaniu.

#include "ScriptedPrompts.h"
#include "editor/EditorWidget.h"
#include "services/FileWatchService.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace cynamoncode;
using testing::ScriptedPrompts;

namespace {

constexpr int WatchTimeoutMs = 5000;

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

bool writeBytes(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

void edit(EditorWidget *editor, const QString &text)
{
    editor->selectAll();
    editor->replaceSelectedText(text);
}

void trigger(MainWindow &window, const char *actionName)
{
    auto *action = window.findChild<QAction *>(QLatin1String(actionName));
    QVERIFY2(action, actionName);
    action->trigger();
}

struct Fixture
{
    ScriptedPrompts *prompts = nullptr;
    std::unique_ptr<MainWindow> window;

    Fixture()
    {
        auto scripted = std::make_unique<ScriptedPrompts>();
        prompts = scripted.get();
        window = std::make_unique<MainWindow>(std::move(scripted));
    }
};

// Bez zdarzeń w oknie obserwacji: na pewno nie było pytania.
void waitPastDebounce()
{
    QTest::qWait(FileWatchService::DebounceMs * 4);
}

} // namespace

class TestExternalChanges : public QObject
{
    Q_OBJECT

private slots:
    void unmodifiedDocumentReloadsOnRequest();
    void keepThenSaveAsksAboutConflict();
    void conflictCanBeSavedAs();
    void localChangesGetConflictQuestion();
    void ownSaveDoesNotAsk();
    void deletedFileInformsAndProtectsClose();
    void deletionDuringQuestionIsNotLost();
    void recreatedFileIsWatchedAgain();
    void keptVersionIsNotAskedAgain();
};

void TestExternalChanges::deletionDuringQuestionIsNotLost()
{
    // Regresja W2: zdarzenie z czasu otwartego pytania nie może przepaść.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "jedyna kopia"));

    Fixture f;
    QVERIFY(f.window->openFile(path));
    f.prompts->duringExternalQuestion = [path] {
        QFile::remove(path);
        QTest::qWait(FileWatchService::DebounceMs * 4); // zdarzenia docierają „pod” dialogiem
    };
    QVERIFY(writeBytes(path, "zmiana tuż przed usunięciem"));
    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->externalQuestions, 1, WatchTimeoutMs); // „Zostaw”
    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->infos.size(), 1, WatchTimeoutMs);       // odłożone

    f.prompts->unsavedAnswers = {UnsavedChoice::Cancel};
    trigger(*f.window, "actionCloseTab");
    QCOMPARE(f.prompts->unsavedQuestions, 1);
    QCOMPARE(f.window->documentCount(), 1);
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("jedyna kopia"));
}

void TestExternalChanges::recreatedFileIsWatchedAgain()
{
    // Regresja W3: po usunięciu i odtworzeniu pliku obserwacja wraca.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "start"));

    Fixture f;
    QVERIFY(f.window->openFile(path));
    QVERIFY(QFile::remove(path));
    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->infos.size(), 1, WatchTimeoutMs);
    QTest::qWait(300);
    QVERIFY(writeBytes(path, "odtworzony przez inny program"));
    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->externalQuestions, 1, WatchTimeoutMs);
}

void TestExternalChanges::keptVersionIsNotAskedAgain()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "moja"));

    Fixture f;
    QVERIFY(f.window->openFile(path));
    QVERIFY(writeBytes(path, "obca wersja"));
    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->externalQuestions, 1, WatchTimeoutMs); // „Zostaw”

    // Ta sama obca treść jeszcze raz (np. touch) — bez kolejnego pytania.
    QVERIFY(writeBytes(path, "obca wersja"));
    waitPastDebounce();
    QCOMPARE(f.prompts->externalQuestions, 1);

    // Nowa obca treść — znowu pytanie.
    QVERIFY(writeBytes(path, "jeszcze inna wersja"));
    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->externalQuestions, 2, WatchTimeoutMs);
}

void TestExternalChanges::unmodifiedDocumentReloadsOnRequest()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "pierwsza\ndruga\n"));

    Fixture f;
    QVERIFY(f.window->openFile(path));
    f.prompts->externalAnswers = {ExternalChangeChoice::Reload};
    QVERIFY(writeBytes(path, "zmieniona przez git\n"));

    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->externalQuestions, 1, WatchTimeoutMs);
    QVERIFY(!f.prompts->lastExternalHadLocalChanges);
    EditorWidget *editor = f.window->currentEditor();
    QCOMPARE(editor->text(), QStringLiteral("zmieniona przez git\n"));
    QVERIFY(!editor->isModified());
}

void TestExternalChanges::keepThenSaveAsksAboutConflict()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "moja"));

    Fixture f;
    QVERIFY(f.window->openFile(path));
    QVERIFY(writeBytes(path, "cudza zmiana"));
    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->externalQuestions, 1, WatchTimeoutMs);
    EditorWidget *editor = f.window->currentEditor();
    QCOMPARE(editor->text(), QStringLiteral("moja")); // „Zostaw”

    edit(editor, QStringLiteral("moja poprawiona"));
    f.prompts->conflictAnswers = {SaveConflictChoice::Cancel};
    trigger(*f.window, "actionSave");
    QCOMPARE(f.prompts->conflictQuestions, 1);
    QCOMPARE(readBytes(path), QByteArray("cudza zmiana"));
    QVERIFY(editor->isModified());

    f.prompts->conflictAnswers = {SaveConflictChoice::Overwrite};
    trigger(*f.window, "actionSave");
    QCOMPARE(f.prompts->conflictQuestions, 2);
    QCOMPARE(readBytes(path), QByteArray("moja poprawiona"));
    QVERIFY(!editor->isModified());
}

void TestExternalChanges::conflictCanBeSavedAs()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    const QString other = dir.filePath(QStringLiteral("kopia.txt"));
    QVERIFY(writeBytes(path, "moja"));

    Fixture f;
    QVERIFY(f.window->openFile(path));
    QVERIFY(writeBytes(path, "cudza zmiana"));
    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->externalQuestions, 1, WatchTimeoutMs);

    edit(f.window->currentEditor(), QStringLiteral("moja wersja"));
    f.prompts->conflictAnswers = {SaveConflictChoice::SaveAs};
    f.prompts->savePaths = {other};
    trigger(*f.window, "actionSave");
    QCOMPARE(readBytes(other), QByteArray("moja wersja"));
    QCOMPARE(readBytes(path), QByteArray("cudza zmiana"));
}

void TestExternalChanges::localChangesGetConflictQuestion()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "start"));

    Fixture f;
    QVERIFY(f.window->openFile(path));
    edit(f.window->currentEditor(), QStringLiteral("niezapisana praca"));
    QVERIFY(writeBytes(path, "zmiana z zewnątrz"));

    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->externalQuestions, 1, WatchTimeoutMs);
    QVERIFY(f.prompts->lastExternalHadLocalChanges);
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("niezapisana praca"));
    QVERIFY(f.window->currentEditor()->isModified());
}

void TestExternalChanges::ownSaveDoesNotAsk()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "start"));

    Fixture f;
    QVERIFY(f.window->openFile(path));
    edit(f.window->currentEditor(), QStringLiteral("zapis 1"));
    trigger(*f.window, "actionSave");
    waitPastDebounce();
    edit(f.window->currentEditor(), QStringLiteral("zapis numer 2"));
    trigger(*f.window, "actionSave");
    waitPastDebounce();

    QCOMPARE(f.prompts->externalQuestions, 0);
    QCOMPARE(f.prompts->conflictQuestions, 0);
    QCOMPARE(readBytes(path), QByteArray("zapis numer 2"));

    // Po własnych zapisach obserwacja nadal działa.
    QVERIFY(writeBytes(path, "zmiana z innego programu"));
    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->externalQuestions, 1, WatchTimeoutMs);
}

void TestExternalChanges::deletedFileInformsAndProtectsClose()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "ważna treść"));

    Fixture f;
    QVERIFY(f.window->openFile(path));
    QVERIFY(QFile::remove(path));
    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->infos.size(), 1, WatchTimeoutMs);
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("ważna treść"));

    // Zamknięcie karty pyta o zapis, choć bufor nie był edytowany.
    f.prompts->unsavedAnswers = {UnsavedChoice::Cancel};
    trigger(*f.window, "actionCloseTab");
    QCOMPARE(f.prompts->unsavedQuestions, 1);
    QCOMPARE(f.window->documentCount(), 1);

    // „Zapisz wszystkie” obejmuje też dokument usuniętego pliku i go odtwarza.
    trigger(*f.window, "actionSaveAll");
    QCOMPARE(readBytes(path), QByteArray("ważna treść"));
    f.prompts->unsavedAnswers = {};
    trigger(*f.window, "actionCloseTab");
    QCOMPARE(f.window->documentCount(), 0);
}

QTEST_MAIN(TestExternalChanges)
#include "tst_external_changes.moc"
