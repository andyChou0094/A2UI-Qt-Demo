#ifndef A2UI_HOST_SHELL_H
#define A2UI_HOST_SHELL_H

#include "SurfaceRenderer.h"

#include <QByteArray>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSplitter;

namespace a2ui {

class SurfaceRenderer;
class CompositionClient;
class SurfaceDocumentClient;
class WidgetRegistry;

class HostShell : public QWidget
{
    Q_OBJECT
public:
    explicit HostShell(const WidgetRegistry &registry, QWidget *parent = 0);
    ~HostShell();

    SurfaceApplyResult applySurface(const QByteArray &surfaceJson);

    QLineEdit *promptInput() const;
    QPushButton *composeButton() const;
    QLabel *progressLabel() const;
    QLabel *statusSummary() const;
    QPushButton *diagnosticsToggle() const;
    QScrollArea *surfaceScrollArea() const;
    QWidget *surfaceHost() const;
    QPlainTextEdit *statusPanel() const;
    QPushButton *importButton() const;
    QPushButton *exportButton() const;
    QPushButton *restoreDefaultButton() const;
    QSplitter *workspaceSplitter() const;
    QLabel *surfaceMetadata() const;
    QByteArray currentSurfaceJson() const;
    bool importSurfaceDocument(const QByteArray &document);
    bool exportCurrentSurface(const QString &targetPath);
    bool restoreDefaultSurface();

private:
    void setConsoleState(const QString &kind, const QString &text);
    void appendDiagnostic(const QString &summary, const QString &code,
                          const QString &rawDiagnostic);
    SurfaceApplyResult applySurfaceWithSource(const QByteArray &surfaceJson,
                                              const QString &source,
                                              const QString &successText);
    void updateSurfaceMetadata(const QByteArray &surfaceJson,
                               const QString &source,
                               const QString &state);
    void setDocumentButtonsEnabled(bool enabled);

    QLabel *statusSummary_;
    QPushButton *diagnosticsToggle_;
    QLineEdit *promptInput_;
    QPushButton *composeButton_;
    QPushButton *importButton_;
    QPushButton *exportButton_;
    QPushButton *restoreDefaultButton_;
    QLabel *progressLabel_;
    QLabel *surfaceMetadata_;
    QSplitter *workspaceSplitter_;
    QScrollArea *surfaceScrollArea_;
    QWidget *surfaceHost_;
    QPlainTextEdit *statusPanel_;
    SurfaceRenderer *renderer_;
    CompositionClient *compositionClient_;
    SurfaceDocumentClient *surfaceDocumentClient_;
    QByteArray currentSurfaceJson_;
    QString pendingExportPath_;
};

} // namespace a2ui

#endif
