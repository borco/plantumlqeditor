#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QProcess>

namespace Ui {
class PreferencesDialog;
}

class FileCache;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(FileCache* file_cache, QWidget *parent = 0);
    ~PreferencesDialog();

    void readSettings();
    void writeSettings();

private slots:
    void onRejected();
    void on_customJavaPathEdit_textEdited(const QString &);
    void on_customJavaPathButton_clicked();
    void on_customPlantUmlEdit_textEdited(const QString &);
    void on_customPlantUmlButton_clicked();
    void on_assistantXmlButton_clicked();
    void on_customGraphizEdit_textEdited(const QString &);
    void on_customGraphizButton_clicked();
    void on_customCacheEdit_textEdited(const QString &);
    void on_customCacheButton_clicked();
    void on_clearCacheButton_clicked();
    void on_checkExternalPrograms_clicked();

private:
    void checkExternalPrograms();
    bool analyzeTestCall(QProcess& process, int& processResult, QStringList& log);

    Ui::PreferencesDialog *m_ui;
    FileCache* m_fileCache;
};

#endif // PREFERENCESDIALOG_H
