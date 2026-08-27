#include "HostShell.h"

#include "CompositionClient.h"
#include "SurfaceDocumentClient.h"
#include "SurfaceRenderer.h"

#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSplitter>
#include <QStringList>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QString chineseDiagnostic(const QString &code, const QString &message)
{
    if (code == QStringLiteral("unsupported_layout")) {
        return QStringLiteral("当前宿主只支持 Row 与 Column，已保留最后有效界面。");
    }
    if (code == QStringLiteral("timeout")) {
        return QStringLiteral("本地服务响应超时，已保留最后有效界面。");
    }
    if (code == QStringLiteral("invalid_surface_document")) {
        return QStringLiteral("DSL 文档未通过校验，已保留最后有效界面。");
    }
    if (code == QStringLiteral("llm_unavailable")) {
        return QStringLiteral("尚未配置可用的 Provider，当前界面保持不变。");
    }
    if (code == QStringLiteral("invalid_request")) {
        return QStringLiteral("请输入明确的编排要求后重试。");
    }
    if (code == QStringLiteral("configuration_error")) {
        return QStringLiteral("本地服务配置无效，当前界面保持不变。");
    }
    if (code == QStringLiteral("llm_provider_error")) {
        return QStringLiteral("Provider 调用失败，已保留最后有效界面。");
    }
    if (message.contains(QStringLiteral("timed out"), Qt::CaseInsensitive)) {
        return QStringLiteral("请求超时，已保留最后有效数据。");
    }
    return QStringLiteral("操作未完成，已保留最后有效界面；可展开查看原始诊断。");
}

QByteArray defaultSurfaceResource()
{
    QFile file(QStringLiteral(":/a2ui/default-surface.json"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

namespace a2ui {

HostShell::HostShell(const WidgetRegistry &registry, QWidget *parent)
    : QWidget(parent),
      statusSummary_(new QLabel(QStringLiteral("等待首次有效 Surface"), this)),
      diagnosticsToggle_(new QPushButton(QStringLiteral("展开诊断详情"), this)),
      promptInput_(new QLineEdit(this)),
      composeButton_(new QPushButton(QStringLiteral("开始编排"), this)),
      importButton_(new QPushButton(QStringLiteral("导入 DSL"), this)),
      exportButton_(new QPushButton(QStringLiteral("导出 DSL"), this)),
      restoreDefaultButton_(new QPushButton(QStringLiteral("恢复默认排版"), this)),
      progressLabel_(new QLabel(QStringLiteral("就绪"), this)),
      surfaceMetadata_(new QLabel(QStringLiteral("DYNAMIC SURFACE · 尚未加载"), this)),
      workspaceSplitter_(new QSplitter(Qt::Horizontal, this)),
      surfaceScrollArea_(new QScrollArea(this)),
      surfaceHost_(new QWidget),
      statusPanel_(new QPlainTextEdit(this)),
      renderer_(0),
      compositionClient_(0),
      surfaceDocumentClient_(0)
{
    setObjectName(QStringLiteral("hostShell"));
    setWindowTitle(QStringLiteral("受控界面工作台"));
    promptInput_->setPlaceholderText(QStringLiteral("例如：将计算器和历史记录左右排列"));
    promptInput_->setObjectName(QStringLiteral("promptInput"));
    composeButton_->setObjectName(QStringLiteral("composeButton"));
    importButton_->setObjectName(QStringLiteral("importSurfaceButton"));
    exportButton_->setObjectName(QStringLiteral("exportSurfaceButton"));
    restoreDefaultButton_->setObjectName(QStringLiteral("restoreDefaultButton"));
    exportButton_->setEnabled(false);
    progressLabel_->setObjectName(QStringLiteral("compositionProgress"));
    progressLabel_->setProperty("statusKind", QStringLiteral("ready"));
    statusSummary_->setObjectName(QStringLiteral("diagnosticSummary"));
    statusSummary_->setWordWrap(true);
    diagnosticsToggle_->setObjectName(QStringLiteral("diagnosticsToggle"));
    diagnosticsToggle_->setCheckable(true);
    surfaceMetadata_->setObjectName(QStringLiteral("surfaceMetadata"));

    surfaceScrollArea_->setObjectName(QStringLiteral("mainSurfaceScrollArea"));
    surfaceScrollArea_->setWidgetResizable(true);
    surfaceScrollArea_->setWidget(surfaceHost_);
    statusPanel_->setObjectName(QStringLiteral("statusPanel"));
    statusPanel_->setReadOnly(true);
    statusPanel_->setMaximumBlockCount(200);
    statusPanel_->setMaximumHeight(160);
    statusPanel_->setVisible(false);
    statusPanel_->setPlaceholderText(QStringLiteral("诊断信息"));

    QWidget *control = new QWidget(workspaceSplitter_);
    control->setObjectName(QStringLiteral("workspaceControlPane"));
    QLabel *title = new QLabel(QStringLiteral("受控界面工作台"), control);
    title->setObjectName(QStringLiteral("workspaceTitle"));
    QLabel *description = new QLabel(
        QStringLiteral("编排、校验与管理当前 SurfaceSpec"), control);
    description->setObjectName(QStringLiteral("consoleDescription"));
    description->setWordWrap(true);
    QVBoxLayout *controlLayout = new QVBoxLayout(control);
    controlLayout->addWidget(title);
    controlLayout->addWidget(description);
    controlLayout->addWidget(promptInput_);
    controlLayout->addWidget(composeButton_);
    controlLayout->addWidget(progressLabel_);
    QHBoxLayout *documentActions = new QHBoxLayout;
    documentActions->addWidget(importButton_);
    documentActions->addWidget(exportButton_);
    controlLayout->addLayout(documentActions);
    controlLayout->addWidget(restoreDefaultButton_);
    controlLayout->addStretch(1);
    controlLayout->addWidget(statusSummary_);
    controlLayout->addWidget(diagnosticsToggle_);
    controlLayout->addWidget(statusPanel_);

    QWidget *stage = new QWidget(workspaceSplitter_);
    stage->setObjectName(QStringLiteral("surfaceStage"));
    QVBoxLayout *stageLayout = new QVBoxLayout(stage);
    stageLayout->addWidget(surfaceMetadata_);
    stageLayout->addWidget(surfaceScrollArea_, 1);

    control->setMinimumWidth(260);
    stage->setMinimumWidth(360);
    workspaceSplitter_->addWidget(control);
    workspaceSplitter_->addWidget(stage);
    workspaceSplitter_->setStretchFactor(0, 0);
    workspaceSplitter_->setStretchFactor(1, 1);
    workspaceSplitter_->setSizes(QList<int>() << 280 << 620);
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(workspaceSplitter_);

    renderer_ = new SurfaceRenderer(surfaceHost_, &registry);
    surfaceHost_->layout()->setSizeConstraint(QLayout::SetMinAndMaxSize);
    const CompositionClientConfig config = CompositionClientConfig::fromApplication();
    compositionClient_ = new CompositionClient(config, renderer_, this);
    surfaceDocumentClient_ = new SurfaceDocumentClient(config, this);

    connect(diagnosticsToggle_, &QPushButton::toggled, this, [this](bool expanded) {
        statusPanel_->setVisible(expanded);
        diagnosticsToggle_->setText(expanded
            ? QStringLiteral("折叠诊断详情") : QStringLiteral("展开诊断详情"));
    });
    connect(composeButton_, &QPushButton::clicked, this, [this]() {
        if (currentSurfaceJson_.isEmpty()) {
            setConsoleState(QStringLiteral("error"), QStringLiteral("无法请求"));
            appendDiagnostic(QStringLiteral("请先应用一个初始界面。"),
                             QStringLiteral("missing_initial_surface"),
                             QStringLiteral("Current Surface is empty"));
            return;
        }
        setConsoleState(QStringLiteral("busy"), QStringLiteral("请求中…"));
        setDocumentButtonsEnabled(false);
        composeButton_->setEnabled(false);
        if (!compositionClient_->compose(promptInput_->text(), currentSurfaceJson_)) {
            setDocumentButtonsEnabled(true);
            composeButton_->setEnabled(true);
        }
    });
    connect(compositionClient_, &CompositionClient::compositionApplied,
            this, [this](const QByteArray &surfaceJson) {
        currentSurfaceJson_ = surfaceJson;
        updateSurfaceMetadata(surfaceJson, QStringLiteral("模型"), QStringLiteral("已应用"));
        setConsoleState(QStringLiteral("success"), QStringLiteral("编排成功"));
        setDocumentButtonsEnabled(true);
        composeButton_->setEnabled(true);
        appendDiagnostic(QStringLiteral("编排目标已校验并应用。"), QString(), QString());
    });
    connect(compositionClient_, &CompositionClient::compositionFailed,
            this, [this](const QString &code, const QString &message) {
        setConsoleState(QStringLiteral("error"), QStringLiteral("编排失败"));
        setDocumentButtonsEnabled(true);
        composeButton_->setEnabled(true);
        appendDiagnostic(chineseDiagnostic(code, message), code, message);
    });
    connect(surfaceDocumentClient_, &SurfaceDocumentClient::importValidated,
            this, [this](const QByteArray &surfaceJson) {
        applySurfaceWithSource(surfaceJson, QStringLiteral("导入"), QStringLiteral("导入成功"));
        setDocumentButtonsEnabled(true);
    });
    connect(surfaceDocumentClient_, &SurfaceDocumentClient::exportReady,
            this, [this](const QByteArray &bytes, const QString &) {
        QSaveFile file(pendingExportPath_);
        const bool opened = !pendingExportPath_.isEmpty() && file.open(QIODevice::WriteOnly);
        const bool written = opened && file.write(bytes) == bytes.size();
        const bool committed = written && file.commit();
        setDocumentButtonsEnabled(true);
        if (committed) {
            setConsoleState(QStringLiteral("success"), QStringLiteral("导出成功"));
            appendDiagnostic(QStringLiteral("当前 DSL 已原子写入目标文件。"), QString(), QString());
        } else {
            if (opened && !committed) {
                file.cancelWriting();
            }
            setConsoleState(QStringLiteral("error"), QStringLiteral("导出失败"));
            appendDiagnostic(QStringLiteral("目标文件无法完整写入，当前界面保持不变。"),
                             QStringLiteral("surface_export_write_failed"), file.errorString());
        }
        pendingExportPath_.clear();
    });
    connect(surfaceDocumentClient_, &SurfaceDocumentClient::defaultReady,
            this, [this](const QByteArray &surfaceJson) {
        applySurfaceWithSource(surfaceJson, QStringLiteral("默认"), QStringLiteral("已恢复默认"));
        setDocumentButtonsEnabled(true);
    });
    connect(surfaceDocumentClient_, &SurfaceDocumentClient::documentFailed,
            this, [this](const QString &operation, const QString &code, const QString &message) {
        setDocumentButtonsEnabled(true);
        setConsoleState(QStringLiteral("error"), operation == QStringLiteral("export")
                        ? QStringLiteral("导出失败") : QStringLiteral("文档操作失败"));
        appendDiagnostic(chineseDiagnostic(code, message), code, message);
        pendingExportPath_.clear();
    });
    connect(importButton_, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("导入 SurfaceSpec DSL"), QString(), QStringLiteral("JSON 文件 (*.json)"));
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024) {
            appendDiagnostic(QStringLiteral("无法读取 DSL，或文件超过 64 KiB。"),
                             QStringLiteral("invalid_surface_document"), file.errorString());
            return;
        }
        importSurfaceDocument(file.readAll());
    });
    connect(exportButton_, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出 SurfaceSpec DSL"), QStringLiteral("surface-main.json"),
            QStringLiteral("JSON 文件 (*.json)"));
        if (!path.isEmpty()) {
            exportCurrentSurface(path);
        }
    });
    connect(restoreDefaultButton_, &QPushButton::clicked,
            this, [this]() { restoreDefaultSurface(); });
    setTabOrder(promptInput_, composeButton_);
    setTabOrder(composeButton_, importButton_);
    setTabOrder(importButton_, exportButton_);
    setTabOrder(exportButton_, restoreDefaultButton_);
    setTabOrder(restoreDefaultButton_, diagnosticsToggle_);
}

HostShell::~HostShell() { delete renderer_; }

SurfaceApplyResult HostShell::applySurface(const QByteArray &surfaceJson)
{
    return applySurfaceWithSource(surfaceJson, QStringLiteral("默认"), QStringLiteral("界面已就绪"));
}

SurfaceApplyResult HostShell::applySurfaceWithSource(const QByteArray &surfaceJson,
                                                     const QString &source,
                                                     const QString &successText)
{
    const SurfaceApplyResult result = renderer_->apply(surfaceJson);
    if (result.applied) {
        currentSurfaceJson_ = QJsonDocument::fromJson(surfaceJson).toJson(QJsonDocument::Compact);
        updateSurfaceMetadata(currentSurfaceJson_, source, QStringLiteral("已应用"));
        setConsoleState(QStringLiteral("success"), successText);
        exportButton_->setEnabled(true);
        appendDiagnostic(QStringLiteral("界面已成功应用。"), QString(), QString());
    } else {
        setConsoleState(QStringLiteral("error"), QStringLiteral("界面无效"));
        appendDiagnostic(QStringLiteral("界面校验失败，已保留最后有效界面。"),
                         QStringLiteral("invalid_surface"),
                         result.diagnostics.join(QStringLiteral("\n")));
    }
    return result;
}

bool HostShell::importSurfaceDocument(const QByteArray &document)
{
    setConsoleState(QStringLiteral("busy"), QStringLiteral("正在校验导入…"));
    setDocumentButtonsEnabled(false);
    return surfaceDocumentClient_->importDocument(document);
}

bool HostShell::exportCurrentSurface(const QString &targetPath)
{
    if (targetPath.isEmpty() || currentSurfaceJson_.isEmpty()) {
        return false;
    }
    pendingExportPath_ = targetPath;
    setConsoleState(QStringLiteral("busy"), QStringLiteral("正在复核导出…"));
    setDocumentButtonsEnabled(false);
    return surfaceDocumentClient_->exportDocument(currentSurfaceJson_);
}

bool HostShell::restoreDefaultSurface()
{
    const QByteArray document = defaultSurfaceResource();
    if (document.isEmpty()) {
        appendDiagnostic(QStringLiteral("默认排版资源不可用，当前界面保持不变。"),
                         QStringLiteral("default_surface_unavailable"), QString());
        return false;
    }
    return applySurfaceWithSource(document, QStringLiteral("默认"),
                                  QStringLiteral("已恢复默认")).applied;
}

void HostShell::updateSurfaceMetadata(const QByteArray &surfaceJson,
                                      const QString &source, const QString &state)
{
    const QJsonObject document = QJsonDocument::fromJson(surfaceJson).object();
    surfaceMetadata_->setText(QStringLiteral("DYNAMIC SURFACE · %1 · %2 nodes · 来源：%3 · %4")
        .arg(document.value(QStringLiteral("surfaceId")).toString())
        .arg(document.value(QStringLiteral("nodes")).toArray().size())
        .arg(source, state));
}

void HostShell::setDocumentButtonsEnabled(bool enabled)
{
    importButton_->setEnabled(enabled);
    restoreDefaultButton_->setEnabled(enabled);
    exportButton_->setEnabled(enabled && !currentSurfaceJson_.isEmpty());
}

QLineEdit *HostShell::promptInput() const { return promptInput_; }
QPushButton *HostShell::composeButton() const { return composeButton_; }
QLabel *HostShell::progressLabel() const { return progressLabel_; }
QLabel *HostShell::statusSummary() const { return statusSummary_; }
QPushButton *HostShell::diagnosticsToggle() const { return diagnosticsToggle_; }
QScrollArea *HostShell::surfaceScrollArea() const { return surfaceScrollArea_; }
QWidget *HostShell::surfaceHost() const { return surfaceHost_; }
QPlainTextEdit *HostShell::statusPanel() const { return statusPanel_; }
QPushButton *HostShell::importButton() const { return importButton_; }
QPushButton *HostShell::exportButton() const { return exportButton_; }
QPushButton *HostShell::restoreDefaultButton() const { return restoreDefaultButton_; }
QSplitter *HostShell::workspaceSplitter() const { return workspaceSplitter_; }
QLabel *HostShell::surfaceMetadata() const { return surfaceMetadata_; }
QByteArray HostShell::currentSurfaceJson() const { return currentSurfaceJson_; }

void HostShell::setConsoleState(const QString &kind, const QString &text)
{
    progressLabel_->setProperty("statusKind", kind);
    progressLabel_->setText(text);
    progressLabel_->style()->unpolish(progressLabel_);
    progressLabel_->style()->polish(progressLabel_);
}

void HostShell::appendDiagnostic(const QString &summary, const QString &code,
                                 const QString &rawDiagnostic)
{
    statusSummary_->setText(summary);
    QStringList details;
    details << summary;
    if (!code.isEmpty()) {
        details << QStringLiteral("错误码：%1").arg(code);
    }
    if (!rawDiagnostic.isEmpty()) {
        details << QStringLiteral("原始诊断：%1").arg(rawDiagnostic);
    }
    statusPanel_->appendPlainText(details.join(QStringLiteral("\n")));
    diagnosticsToggle_->setProperty("hasDetails", !statusPanel_->toPlainText().isEmpty());
}

} // namespace a2ui
