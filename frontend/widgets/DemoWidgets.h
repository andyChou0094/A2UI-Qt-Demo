#ifndef A2UI_DEMO_WIDGETS_H
#define A2UI_DEMO_WIDGETS_H

#include <QString>
#include <QStringList>
#include <QWidget>

#include "CalculationRecord.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSize;
class QTextEdit;
class QTimer;

namespace a2ui {

class CalculationService;

class Calculator : public QWidget
{
    Q_OBJECT
public:
    explicit Calculator(QWidget *parent = 0);
    Calculator(CalculationService *service, QWidget *parent);
    QString inputText() const;
    QString statusText() const;
    void setInputText(const QString &text);
    void pressButton(const QString &label);
    QSize minimumSizeHint() const override;

signals:
    void calculationSucceeded(const QString &expression, double result);

private:
    CalculationService *service_;
    bool persistencePending_;
    QLineEdit *input_;
    QLabel *status_;
};

class CalculationHistory : public QWidget
{
    Q_OBJECT
public:
    explicit CalculationHistory(QWidget *parent = 0);
    CalculationHistory(CalculationService *service, QWidget *parent);
    void setRecords(const QStringList &records);
    int recordCount() const;
    int selectedRow() const;
    void selectRow(int row);
    void setNoteText(const QString &note);
    QString noteText() const;
    QString statusText() const;
    void refresh();
    void saveSelectedNote();
    void deleteSelectedRecord();
    QSize minimumSizeHint() const override;

signals:
    void refreshRequested();

private:
    void applyRecords(const QList<CalculationRecord> &records);

    CalculationService *service_;
    QList<CalculationRecord> recordData_;
    QListWidget *records_;
    QLineEdit *note_;
    QLabel *status_;
    QTimer *pollTimer_;
};

class CalculationStats : public QWidget
{
    Q_OBJECT
public:
    explicit CalculationStats(QWidget *parent = 0);
    CalculationStats(CalculationService *service, QWidget *parent);
    void setSummary(int count, const QString &latest);
    int count() const;
    QString latest() const;
    QString statusText() const;
    void refresh();
    QSize minimumSizeHint() const override;

private:
    CalculationService *service_;
    int countValue_;
    QString latestValue_;
    QLabel *countLabel_;
    QLabel *latestLabel_;
    QLabel *statusLabel_;
    QTimer *pollTimer_;
};

class Clock : public QWidget
{
    Q_OBJECT
public:
    explicit Clock(QWidget *parent = 0);
    bool isRunning() const;
    QSize minimumSizeHint() const override;

private slots:
    void updateTime();

private:
    QLabel *timeLabel_;
    QTimer *timer_;
};

class NotePad : public QWidget
{
    Q_OBJECT
public:
    explicit NotePad(QWidget *parent = 0);
    QString text() const;
    void setText(const QString &text);
    QSize minimumSizeHint() const override;

private:
    QTextEdit *editor_;
};

} // namespace a2ui

#endif
