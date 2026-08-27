#include "DemoWidgets.h"

#include "CalculationService.h"

#include <QDateTime>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QStringList>
#include <QTextEdit>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace a2ui {

namespace {

void initializeCard(QWidget *widget, QVBoxLayout *layout,
                    const QString &componentType, const QString &title)
{
    widget->setObjectName(QStringLiteral("businessCard"));
    widget->setProperty("componentType", componentType);
    widget->setProperty("hostPresentation", QStringLiteral("business-card-v1"));
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QLabel *heading = new QLabel(title, widget);
    heading->setObjectName(QStringLiteral("businessCardTitle"));
    heading->setProperty("componentType", componentType);
    layout->addWidget(heading);
}

} // namespace

Calculator::Calculator(QWidget *parent)
    : Calculator(static_cast<CalculationService *>(0), parent)
{
}

Calculator::Calculator(CalculationService *service, QWidget *parent)
    : QWidget(parent), service_(service), persistencePending_(false),
      input_(new QLineEdit(this)), status_(new QLabel(this))
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    initializeCard(this, outer, QStringLiteral("Calculator"), QStringLiteral("计算器"));
    input_->setObjectName(QStringLiteral("calculatorInput"));
    input_->setReadOnly(true);
    status_->setText(QStringLiteral("就绪"));
    outer->addWidget(input_);

    QGridLayout *buttons = new QGridLayout;
    const QStringList labels = QStringList()
        << QStringLiteral("7") << QStringLiteral("8") << QStringLiteral("9") << QStringLiteral("÷")
        << QStringLiteral("4") << QStringLiteral("5") << QStringLiteral("6") << QStringLiteral("×")
        << QStringLiteral("1") << QStringLiteral("2") << QStringLiteral("3") << QStringLiteral("-")
        << QStringLiteral("C") << QStringLiteral("0") << QStringLiteral("=") << QStringLiteral("+");
    for (int index = 0; index < labels.size(); ++index) {
        const QString label = labels.at(index);
        QPushButton *button = new QPushButton(label, this);
        buttons->addWidget(button, index / 4, index % 4);
        connect(button, &QPushButton::clicked, this, [this, label]() {
            pressButton(label);
        });
    }
    outer->addLayout(buttons);
    outer->addWidget(status_);

    if (service_) {
        connect(this, &Calculator::calculationSucceeded, this,
                [this](const QString &expression, double result) {
            persistencePending_ = true;
            status_->setText(QStringLiteral("正在保存"));
            if (!service_->createRecord(expression, result)) {
                persistencePending_ = false;
                status_->setText(QStringLiteral("保存失败"));
            }
        });
        connect(service_, &CalculationService::recordCreated, this,
                [this](const CalculationRecord &) {
            if (persistencePending_) {
                persistencePending_ = false;
                status_->setText(QStringLiteral("已持久化"));
            }
        });
        connect(service_, &CalculationService::requestFailed, this,
                [this](CalculationService::Operation operation, const QString &) {
            if (operation == CalculationService::CreateRecord && persistencePending_) {
                persistencePending_ = false;
                status_->setText(QStringLiteral("保存失败"));
            }
        });
    }
}

QString Calculator::inputText() const { return input_->text(); }
QString Calculator::statusText() const { return status_->text(); }
void Calculator::setInputText(const QString &text) { input_->setText(text); }

void Calculator::pressButton(const QString &label)
{
    const QStringList digits = QStringList()
        << QStringLiteral("0") << QStringLiteral("1") << QStringLiteral("2")
        << QStringLiteral("3") << QStringLiteral("4") << QStringLiteral("5")
        << QStringLiteral("6") << QStringLiteral("7") << QStringLiteral("8")
        << QStringLiteral("9");
    const QStringList operators = QStringList()
        << QStringLiteral("+") << QStringLiteral("-")
        << QStringLiteral("×") << QStringLiteral("÷");

    if (label == QStringLiteral("C")) {
        input_->clear();
        status_->setText(QStringLiteral("就绪"));
        return;
    }

    if (digits.contains(label)) {
        input_->setText(input_->text() + label);
        status_->setText(QStringLiteral("就绪"));
        return;
    }

    if (operators.contains(label)) {
        const QRegularExpression numberPattern(QStringLiteral("^-?[0-9]+(?:\\.[0-9]+)?$"));
        if (!numberPattern.match(input_->text()).hasMatch()) {
            status_->setText(QStringLiteral("非法输入"));
            return;
        }
        input_->setText(input_->text() + label);
        status_->setText(QStringLiteral("就绪"));
        return;
    }

    if (label != QStringLiteral("=")) {
        status_->setText(QStringLiteral("不支持的按钮"));
        return;
    }

    const QString expression = input_->text();
    const QRegularExpression expressionPattern(
        QStringLiteral("^(-?[0-9]+(?:\\.[0-9]+)?)([+\\-×÷])(-?[0-9]+(?:\\.[0-9]+)?)$"));
    const QRegularExpressionMatch match = expressionPattern.match(expression);
    if (!match.hasMatch()) {
        status_->setText(QStringLiteral("非法输入"));
        return;
    }

    bool leftOk = false;
    bool rightOk = false;
    const double left = match.captured(1).toDouble(&leftOk);
    const QString operation = match.captured(2);
    const double right = match.captured(3).toDouble(&rightOk);
    if (!leftOk || !rightOk) {
        status_->setText(QStringLiteral("非法输入"));
        return;
    }
    if (operation == QStringLiteral("÷") && right == 0.0) {
        status_->setText(QStringLiteral("除数不能为零"));
        return;
    }

    double result = 0.0;
    if (operation == QStringLiteral("+")) {
        result = left + right;
    } else if (operation == QStringLiteral("-")) {
        result = left - right;
    } else if (operation == QStringLiteral("×")) {
        result = left * right;
    } else {
        result = left / right;
    }

    input_->setText(QString::number(result, 'g', 15));
    status_->setText(QStringLiteral("计算成功"));
    emit calculationSucceeded(expression, result);
}

QSize Calculator::minimumSizeHint() const { return QSize(190, 260); }

CalculationHistory::CalculationHistory(QWidget *parent)
    : CalculationHistory(static_cast<CalculationService *>(0), parent)
{
}

CalculationHistory::CalculationHistory(CalculationService *service, QWidget *parent)
    : QWidget(parent), service_(service), records_(new QListWidget(this)),
      note_(new QLineEdit(this)), status_(new QLabel(this)), pollTimer_(new QTimer(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    initializeCard(this, layout, QStringLiteral("CalculationHistory"), QStringLiteral("计算历史"));
    QPushButton *refreshButton = new QPushButton(QStringLiteral("刷新"), this);
    QPushButton *saveNote = new QPushButton(QStringLiteral("保存备注"), this);
    QPushButton *deleteRecord = new QPushButton(QStringLiteral("删除"), this);
    QGridLayout *actions = new QGridLayout;
    actions->addWidget(refreshButton, 0, 0);
    actions->addWidget(saveNote, 0, 1);
    actions->addWidget(deleteRecord, 1, 0, 1, 2);
    layout->addWidget(records_);
    layout->addWidget(note_);
    layout->addLayout(actions);
    layout->addWidget(status_);
    status_->setText(QStringLiteral("未连接业务服务"));
    connect(refreshButton, &QPushButton::clicked, this, &CalculationHistory::refresh);
    connect(refreshButton, &QPushButton::clicked, this, &CalculationHistory::refreshRequested);
    connect(saveNote, &QPushButton::clicked, this, &CalculationHistory::saveSelectedNote);
    connect(deleteRecord, &QPushButton::clicked, this, &CalculationHistory::deleteSelectedRecord);
    connect(records_, &QListWidget::currentRowChanged, this, [this](int row) {
        note_->setText(row >= 0 && row < recordData_.size()
            ? recordData_.at(row).note : QString());
    });

    if (service_) {
        connect(service_, &CalculationService::recordsFetched,
                this, &CalculationHistory::applyRecords);
        connect(service_, &CalculationService::recordUpdated,
                this, [this](const CalculationRecord &) { refresh(); });
        connect(service_, &CalculationService::recordDeleted,
                this, [this](const QString &) { refresh(); });
        connect(service_, &CalculationService::requestFailed, this,
                [this](CalculationService::Operation operation, const QString &) {
            if (operation == CalculationService::FetchRecords
                || operation == CalculationService::UpdateNote
                || operation == CalculationService::DeleteRecord) {
                status_->setText(QStringLiteral("查询失败，保留最后有效记录"));
            }
        });
        connect(pollTimer_, &QTimer::timeout, this, &CalculationHistory::refresh);
        pollTimer_->start(2000);
        refresh();
    }
}

void CalculationHistory::setRecords(const QStringList &records)
{
    recordData_.clear();
    const int previousRow = records_->currentRow();
    records_->clear();
    records_->addItems(records);
    if (previousRow >= 0 && previousRow < records_->count()) {
        records_->setCurrentRow(previousRow);
    }
}

int CalculationHistory::recordCount() const { return records_->count(); }
int CalculationHistory::selectedRow() const { return records_->currentRow(); }
void CalculationHistory::selectRow(int row) { records_->setCurrentRow(row); }
void CalculationHistory::setNoteText(const QString &note) { note_->setText(note); }
QString CalculationHistory::noteText() const { return note_->text(); }
QString CalculationHistory::statusText() const { return status_->text(); }

void CalculationHistory::refresh()
{
    if (service_) {
        service_->fetchRecords(50);
    }
}

void CalculationHistory::saveSelectedNote()
{
    const int row = selectedRow();
    if (service_ && row >= 0 && row < recordData_.size()) {
        service_->updateNote(recordData_.at(row).id, note_->text());
    }
}

void CalculationHistory::deleteSelectedRecord()
{
    const int row = selectedRow();
    if (service_ && row >= 0 && row < recordData_.size()) {
        service_->deleteRecord(recordData_.at(row).id);
    }
}

void CalculationHistory::applyRecords(const QList<CalculationRecord> &records)
{
    const int previousRow = selectedRow();
    recordData_ = records;
    records_->clear();
    for (int index = 0; index < records.size(); ++index) {
        const CalculationRecord &record = records.at(index);
        records_->addItem(QStringLiteral("%1 = %2%3")
            .arg(record.expression)
            .arg(record.result)
            .arg(record.note.isEmpty() ? QString() : QStringLiteral(" — %1").arg(record.note)));
    }
    if (previousRow >= 0 && previousRow < records_->count()) {
        records_->setCurrentRow(previousRow);
    }
    status_->setText(QStringLiteral("已同步"));
}

QSize CalculationHistory::minimumSizeHint() const { return QSize(220, 240); }

CalculationStats::CalculationStats(QWidget *parent)
    : CalculationStats(static_cast<CalculationService *>(0), parent)
{
}

CalculationStats::CalculationStats(CalculationService *service, QWidget *parent)
    : QWidget(parent), service_(service), countValue_(0), countLabel_(new QLabel(this)),
      latestLabel_(new QLabel(this)), statusLabel_(new QLabel(this)), pollTimer_(new QTimer(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    initializeCard(this, layout, QStringLiteral("CalculationStats"), QStringLiteral("计算摘要"));
    layout->addWidget(countLabel_);
    layout->addWidget(latestLabel_);
    layout->addWidget(statusLabel_);
    setSummary(0, QString());
    statusLabel_->setText(service_ ? QStringLiteral("等待同步") : QStringLiteral("未连接业务服务"));
    if (service_) {
        connect(service_, &CalculationService::summaryFetched, this,
                [this](const CalculationSummary &summary) {
            const QString latest = summary.hasLatest
                ? QStringLiteral("%1 = %2").arg(summary.latest.expression).arg(summary.latest.result)
                : QString();
            setSummary(summary.count, latest);
            statusLabel_->setText(QStringLiteral("已同步"));
        });
        connect(service_, &CalculationService::requestFailed, this,
                [this](CalculationService::Operation operation, const QString &) {
            if (operation == CalculationService::FetchSummary) {
                statusLabel_->setText(QStringLiteral("摘要已过期"));
            }
        });
        connect(pollTimer_, &QTimer::timeout, this, &CalculationStats::refresh);
        pollTimer_->start(2000);
        refresh();
    }
}

void CalculationStats::setSummary(int count, const QString &latest)
{
    countValue_ = count;
    latestValue_ = latest;
    countLabel_->setText(QStringLiteral("记录数：%1").arg(count));
    latestLabel_->setText(latest.isEmpty() ? QStringLiteral("暂无记录") : latest);
}

int CalculationStats::count() const { return countValue_; }
QString CalculationStats::latest() const { return latestValue_; }
QString CalculationStats::statusText() const { return statusLabel_->text(); }

void CalculationStats::refresh()
{
    if (service_) {
        service_->fetchSummary();
    }
}

QSize CalculationStats::minimumSizeHint() const { return QSize(180, 140); }

Clock::Clock(QWidget *parent)
    : QWidget(parent), timeLabel_(new QLabel(this)), timer_(new QTimer(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    initializeCard(this, layout, QStringLiteral("Clock"), QStringLiteral("当前时间"));
    layout->addWidget(timeLabel_);
    connect(timer_, &QTimer::timeout, this, &Clock::updateTime);
    timer_->start(1000);
    updateTime();
}

bool Clock::isRunning() const { return timer_->isActive(); }
QSize Clock::minimumSizeHint() const { return QSize(180, 100); }

void Clock::updateTime()
{
    timeLabel_->setText(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

NotePad::NotePad(QWidget *parent)
    : QWidget(parent), editor_(new QTextEdit(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    initializeCard(this, layout, QStringLiteral("NotePad"), QStringLiteral("本地笔记"));
    editor_->setPlaceholderText(QStringLiteral("输入未提交的本地笔记…"));
    layout->addWidget(editor_);
}

QString NotePad::text() const { return editor_->toPlainText(); }
void NotePad::setText(const QString &text) { editor_->setPlainText(text); }
QSize NotePad::minimumSizeHint() const { return QSize(200, 180); }

} // namespace a2ui
