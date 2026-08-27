#ifndef A2UI_TEST_SOURCE_ROOT_H
#define A2UI_TEST_SOURCE_ROOT_H

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace a2ui_test {

inline bool isSourceRoot(const QString &candidate)
{
    const QDir root(candidate);
    return root.exists(QStringLiteral("CMakeLists.txt"))
        && root.exists(QStringLiteral("shared/fixtures/surface-spec"))
        && root.exists(QStringLiteral("shared/schema/surface-spec-v0.schema.json"));
}

inline QString discoverUpward(QString start)
{
    QFileInfo info(start);
    QDir directory(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
    while (true) {
        if (isSourceRoot(directory.absolutePath())) {
            return directory.canonicalPath();
        }
        const QString before = directory.absolutePath();
        if (!directory.cdUp() || directory.absolutePath() == before) {
            return QString();
        }
    }
}

inline QString sourceRoot()
{
    const QByteArray explicitRoot = qgetenv("A2UI_TEST_SOURCE_ROOT");
    if (!explicitRoot.isEmpty() && isSourceRoot(QString::fromLocal8Bit(explicitRoot))) {
        return QFileInfo(QString::fromLocal8Bit(explicitRoot)).canonicalFilePath();
    }
    const QStringList starts = QStringList()
        << QDir::currentPath()
        << QCoreApplication::applicationDirPath();
    for (QStringList::const_iterator it = starts.constBegin(); it != starts.constEnd(); ++it) {
        const QString found = discoverUpward(*it);
        if (!found.isEmpty()) {
            return found;
        }
    }
#ifdef A2UI_SOURCE_DIR
    const QString compiled = QString::fromLocal8Bit(A2UI_SOURCE_DIR);
    if (isSourceRoot(compiled)) {
        return QFileInfo(compiled).canonicalFilePath();
    }
#endif
    return QString();
}

inline QString path(const QString &relative)
{
    const QString root = sourceRoot();
    return root.isEmpty() ? QString() : QDir(root).filePath(relative);
}

} // namespace a2ui_test

#endif
