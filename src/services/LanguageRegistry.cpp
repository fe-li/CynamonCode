#include "services/LanguageRegistry.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>

namespace cynamoncode::LanguageRegistry {

namespace {

const QHash<QString, LanguageId> &extensions()
{
    static const QHash<QString, LanguageId> map = {
        {QStringLiteral("c"), LanguageId::Cpp},       {QStringLiteral("h"), LanguageId::Cpp},
        {QStringLiteral("cc"), LanguageId::Cpp},      {QStringLiteral("cpp"), LanguageId::Cpp},
        {QStringLiteral("cxx"), LanguageId::Cpp},     {QStringLiteral("c++"), LanguageId::Cpp},
        {QStringLiteral("hh"), LanguageId::Cpp},      {QStringLiteral("hpp"), LanguageId::Cpp},
        {QStringLiteral("hxx"), LanguageId::Cpp},     {QStringLiteral("py"), LanguageId::Python},
        {QStringLiteral("pyw"), LanguageId::Python},  {QStringLiteral("pyi"), LanguageId::Python},
        {QStringLiteral("java"), LanguageId::Java},   {QStringLiteral("js"), LanguageId::JavaScript},
        {QStringLiteral("mjs"), LanguageId::JavaScript},
        {QStringLiteral("cjs"), LanguageId::JavaScript},
        {QStringLiteral("html"), LanguageId::Html},   {QStringLiteral("htm"), LanguageId::Html},
        {QStringLiteral("xhtml"), LanguageId::Html},  {QStringLiteral("css"), LanguageId::Css},
        {QStringLiteral("php"), LanguageId::Php},     {QStringLiteral("phtml"), LanguageId::Php},
        {QStringLiteral("sql"), LanguageId::Sql},     {QStringLiteral("json"), LanguageId::Json},
        {QStringLiteral("xml"), LanguageId::Xml},     {QStringLiteral("xsd"), LanguageId::Xml},
        {QStringLiteral("xsl"), LanguageId::Xml},     {QStringLiteral("xslt"), LanguageId::Xml},
        {QStringLiteral("svg"), LanguageId::Xml},     {QStringLiteral("sh"), LanguageId::Bash},
        {QStringLiteral("bash"), LanguageId::Bash},   {QStringLiteral("zsh"), LanguageId::Bash},
        {QStringLiteral("ksh"), LanguageId::Bash},
    };
    return map;
}

const QHash<QString, LanguageId> &fileNames()
{
    static const QHash<QString, LanguageId> map = {
        {QStringLiteral(".bashrc"), LanguageId::Bash},
        {QStringLiteral(".bash_profile"), LanguageId::Bash},
        {QStringLiteral(".bash_aliases"), LanguageId::Bash},
        {QStringLiteral(".bash_logout"), LanguageId::Bash},
        {QStringLiteral(".profile"), LanguageId::Bash},
        {QStringLiteral(".zshrc"), LanguageId::Bash},
    };
    return map;
}

} // namespace

const QList<LanguageId> &all()
{
    static const QList<LanguageId> languages = {
        LanguageId::Plain, LanguageId::Bash,       LanguageId::Cpp,  LanguageId::Css,
        LanguageId::Html,  LanguageId::Java,       LanguageId::JavaScript,
        LanguageId::Json,  LanguageId::Php,        LanguageId::Python,
        LanguageId::Sql,   LanguageId::Xml,
    };
    return languages;
}

QString key(LanguageId language)
{
    switch (language) {
    case LanguageId::Plain:
        return QStringLiteral("plain");
    case LanguageId::Cpp:
        return QStringLiteral("cpp");
    case LanguageId::Python:
        return QStringLiteral("python");
    case LanguageId::Java:
        return QStringLiteral("java");
    case LanguageId::JavaScript:
        return QStringLiteral("javascript");
    case LanguageId::Html:
        return QStringLiteral("html");
    case LanguageId::Css:
        return QStringLiteral("css");
    case LanguageId::Php:
        return QStringLiteral("php");
    case LanguageId::Sql:
        return QStringLiteral("sql");
    case LanguageId::Json:
        return QStringLiteral("json");
    case LanguageId::Xml:
        return QStringLiteral("xml");
    case LanguageId::Bash:
        return QStringLiteral("bash");
    }
    return QStringLiteral("plain");
}

QString displayName(LanguageId language)
{
    switch (language) {
    case LanguageId::Plain:
        return QCoreApplication::translate("LanguageRegistry", "Zwykły tekst");
    case LanguageId::Cpp:
        return QStringLiteral("C/C++");
    case LanguageId::Python:
        return QStringLiteral("Python");
    case LanguageId::Java:
        return QStringLiteral("Java");
    case LanguageId::JavaScript:
        return QStringLiteral("JavaScript");
    case LanguageId::Html:
        return QStringLiteral("HTML");
    case LanguageId::Css:
        return QStringLiteral("CSS");
    case LanguageId::Php:
        return QStringLiteral("PHP");
    case LanguageId::Sql:
        return QStringLiteral("SQL");
    case LanguageId::Json:
        return QStringLiteral("JSON");
    case LanguageId::Xml:
        return QStringLiteral("XML");
    case LanguageId::Bash:
        return QStringLiteral("Bash");
    }
    return QString();
}

LanguageId fromFileName(const QString &filePath)
{
    const QString name = QFileInfo(filePath).fileName().toLower();
    if (const auto it = fileNames().constFind(name); it != fileNames().constEnd())
        return *it;

    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0) // brak rozszerzenia albo sam plik ukryty bez rozszerzenia
        return LanguageId::Plain;
    return extensions().value(name.mid(dot + 1), LanguageId::Plain);
}

LanguageId fromShebang(const QString &firstLine)
{
    if (!firstLine.startsWith(QLatin1String("#!")))
        return LanguageId::Plain;

    // "#!/usr/bin/env -S python3 -u" → "python3"; "#!/bin/bash" → "bash".
    const QStringList parts =
        firstLine.mid(2).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    QString program;
    for (const QString &part : parts) {
        const QString base = part.section(QLatin1Char('/'), -1);
        if (base == QLatin1String("env") || part.startsWith(QLatin1Char('-')))
            continue;
        program = base;
        break;
    }

    static const QRegularExpression pythonName(QStringLiteral("^python[0-9.]*$"));
    if (pythonName.match(program).hasMatch())
        return LanguageId::Python;
    if (program == QLatin1String("bash") || program == QLatin1String("sh")
        || program == QLatin1String("zsh") || program == QLatin1String("ksh")
        || program == QLatin1String("dash"))
        return LanguageId::Bash;
    if (program == QLatin1String("node") || program == QLatin1String("nodejs"))
        return LanguageId::JavaScript;
    if (program == QLatin1String("php"))
        return LanguageId::Php;
    return LanguageId::Plain;
}

LanguageId detect(const QString &filePath, const QString &firstLine)
{
    const LanguageId byName = fromFileName(filePath);
    if (byName != LanguageId::Plain)
        return byName;
    return fromShebang(firstLine);
}

} // namespace cynamoncode::LanguageRegistry
