// Panel plików (D037): otwarcie folderu (także z menu), filtr nazw, otwarcie
// pliku z drzewa; foldery tylko się rozwijają, nic nie jest uruchamiane.

#include "TestHelpers.h"
#include "ui/FileTreeDock.h"

#include <QDir>
#include <QFileSystemModel>
#include <QTemporaryDir>
#include <QTreeView>

using namespace cynamoncode;
using namespace cynamoncode::testing;

namespace {

QStringList visibleNames(FileTreeDock *dock)
{
    QFileSystemModel *model = dock->model();
    const QModelIndex root = dock->view()->rootIndex();
    QStringList names;
    for (int row = 0; row < model->rowCount(root); ++row)
        names << model->fileName(model->index(row, 0, root));
    names.sort();
    return names;
}

} // namespace

class TestWorkspace : public QObject
{
    Q_OBJECT

private slots:
    void openFolderShowsTree();
    void filterHidesNonMatchingFiles();
    void activatingFileOpensItInEditor();
    void openFolderFromMenu();
};

void TestWorkspace::openFolderShowsTree()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("src")));
    QVERIFY(writeBytes(dir.filePath(QStringLiteral("README.md")), "#"));

    WindowFixture f;
    QVERIFY(f.window->fileTree()->isHidden());
    f.window->openFolder(dir.path());
    QVERIFY(!f.window->fileTree()->isHidden());
    QCOMPARE(f.window->workspaceFolder(), QDir(dir.path()).absolutePath());
    QTRY_COMPARE(visibleNames(f.window->fileTree()),
                 (QStringList{QStringLiteral("README.md"), QStringLiteral("src")}));
}

void TestWorkspace::filterHidesNonMatchingFiles()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("katalog")));
    QVERIFY(writeBytes(dir.filePath(QStringLiteral("main.py")), "x"));
    QVERIFY(writeBytes(dir.filePath(QStringLiteral("util.py")), "x"));
    QVERIFY(writeBytes(dir.filePath(QStringLiteral("notatki.txt")), "x"));

    WindowFixture f;
    f.window->openFolder(dir.path());
    QTRY_COMPARE(visibleNames(f.window->fileTree()).size(), 4);

    f.window->fileTree()->setFilter(QStringLiteral(".PY")); // bez rozróżniania wielkości liter
    QTRY_COMPARE(visibleNames(f.window->fileTree()),
                 (QStringList{QStringLiteral("katalog"), QStringLiteral("main.py"),
                              QStringLiteral("util.py")}));
    f.window->fileTree()->setFilter(QString());
    QTRY_COMPARE(visibleNames(f.window->fileTree()).size(), 4);
}

void TestWorkspace::activatingFileOpensItInEditor()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("katalog")));
    QVERIFY(writeBytes(dir.filePath(QStringLiteral("skrypt.sh")), "#!/bin/bash\necho ok\n"));

    WindowFixture f;
    f.window->openFolder(dir.path());
    FileTreeDock *tree = f.window->fileTree();
    QFileSystemModel *model = tree->model();
    QTRY_COMPARE(model->rowCount(tree->view()->rootIndex()), 2);

    const QModelIndex folder = model->index(dir.filePath(QStringLiteral("katalog")));
    emit tree->view()->activated(folder); // folder: nic się nie otwiera
    QCOMPARE(f.window->documentCount(), 1);

    const QModelIndex script = model->index(dir.filePath(QStringLiteral("skrypt.sh")));
    emit tree->view()->activated(script);
    QCOMPARE(f.window->documentCount(), 1); // zastąpił pusty „Nowy 1”
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("#!/bin/bash\necho ok\n"));
    QCOMPARE(int(f.window->currentEditor()->language()), int(LanguageId::Bash));
}

void TestWorkspace::openFolderFromMenu()
{
    QTemporaryDir dir;
    WindowFixture f;
    f.prompts->folderAnswer = dir.path();
    trigger(*f.window, "actionOpenFolder");
    QCOMPARE(f.window->workspaceFolder(), QDir(dir.path()).absolutePath());

    // Anulowany dialog niczego nie zmienia.
    f.prompts->folderAnswer.clear();
    trigger(*f.window, "actionOpenFolder");
    QCOMPARE(f.window->workspaceFolder(), QDir(dir.path()).absolutePath());
}

QTEST_MAIN(TestWorkspace)
#include "tst_workspace.moc"
