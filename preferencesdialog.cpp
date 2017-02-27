#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include "ui_logdialog.h"

#include "utils.h"
#include "filecache.h"
#include "settingsconstants.h"

#include <QDebug>
#include <QFileDialog>
#include <QSettings>
#include <QDesktopServices>
#include <QMessageBox>
#include <QProcess>

PreferencesDialog::PreferencesDialog(FileCache* file_cache, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::PreferencesDialog)
    , m_fileCache(file_cache)
{
    m_ui->setupUi(this);

    m_ui->defaultJavaRadio->setText(tr("Default (%1)").arg(SETTINGS_CUSTOM_JAVA_PATH_DEFAULT));
    m_ui->defaultPlatUmlRadio->setText(tr("Default (%1)").arg(SETTINGS_CUSTOM_PLANTUML_PATH_DEFAULT));
    m_ui->defaultGraphizRadio->setText(tr("Default (%1)").arg(SETTINGS_CUSTOM_GRAPHIZ_PATH_DEFAULT));

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    m_ui->defaultCacheRadio->setText(tr("Default (%1)").arg(QDesktopServices::storageLocation(QDesktopServices::CacheLocation)));
#else
    m_ui->defaultCacheRadio->setText(tr("Default (%1)").arg(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
#endif

    if (m_fileCache) {
        m_ui->cacheCurrentSizeLabel->setText(cacheSizeToString(m_fileCache->totalCost()));
    }

    connect(this, SIGNAL(rejected()), this, SLOT(onRejected()));
    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(onCurrentTabChanged()));
    onCurrentTabChanged();

#if QT_VERSION > QT_VERSION_CHECK(5,2,0)
    QSizePolicy policy = m_ui->checkExternalPrograms->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    m_ui->checkExternalPrograms->setSizePolicy(policy);
#endif
}

PreferencesDialog::~PreferencesDialog()
{
    delete m_ui;
}

void PreferencesDialog::readSettings()
{
    QSettings settings;

    settings.beginGroup(SETTINGS_MAIN_SECTION);

    if (settings.value(SETTINGS_USE_CUSTOM_JAVA, SETTINGS_USE_CUSTOM_JAVA_DEFAULT).toBool())
        m_ui->customJavaRadio->setChecked(true);
    else
        m_ui->defaultJavaRadio->setChecked(true);
    m_ui->customJavaPathEdit->setText(settings.value(SETTINGS_CUSTOM_JAVA_PATH, SETTINGS_CUSTOM_JAVA_PATH_DEFAULT).toString());

    if (settings.value(SETTINGS_USE_CUSTOM_PLANTUML, SETTINGS_USE_CUSTOM_PLANTUML_DEFAULT).toBool())
        m_ui->customPlantUmlRadio->setChecked(true);
    else
        m_ui->defaultPlatUmlRadio->setChecked(true);
    m_ui->customPlantUmlEdit->setText(settings.value(SETTINGS_CUSTOM_PLANTUML_PATH, SETTINGS_CUSTOM_PLANTUML_PATH_DEFAULT).toString());

    if (settings.value(SETTINGS_USE_CUSTOM_GRAPHIZ, SETTINGS_USE_CUSTOM_GRAPHIZ_DEFAULT).toBool())
        m_ui->customGraphizRadio->setChecked(true);
    else
        m_ui->defaultGraphizRadio->setChecked(true);
    m_ui->customGraphizEdit->setText(settings.value(SETTINGS_CUSTOM_GRAPHIZ_PATH).toString());

    m_ui->autoRefreshSpin->setValue(settings.value(SETTINGS_AUTOREFRESH_TIMEOUT).toInt() / TIMEOUT_SCALE);
    m_ui->assistantXmlEdit->setText(settings.value(SETTINGS_ASSISTANT_XML_PATH).toString());

    m_ui->cacheGroupBox->setChecked(settings.value(SETTINGS_USE_CACHE, SETTINGS_USE_CACHE_DEFAULT).toBool());
    if (settings.value(SETTINGS_USE_CUSTOM_CACHE, SETTINGS_USE_CUSTOM_CACHE_DEFAULT).toBool())
        m_ui->customCacheRadio->setChecked(true);
    else
        m_ui->defaultCacheRadio->setChecked(true);
    m_ui->customCacheEdit->setText(settings.value(SETTINGS_CUSTOM_CACHE_PATH).toString());
    m_ui->cacheMaxSize->setValue(int(settings.value(SETTINGS_CACHE_MAX_SIZE, SETTINGS_CACHE_MAX_SIZE_DEFAULT).toInt() / CACHE_SCALE));

    settings.endGroup();

    settings.beginGroup(SETTINGS_EDITOR_SECTION);

    QFont defaultFont;
    QFont editorFont;
    editorFont.fromString(settings.value(SETTINGS_EDITOR_FONT, defaultFont.toString()).toString());
    m_ui->editorFontComboBox->setCurrentFont(editorFont);
    m_ui->editorFontSizeSpinBox->setValue(editorFont.pointSize());

    m_ui->useSpacesInsteadTabsCheckBox->setChecked(settings.value(SETTINGS_EDITOR_INDENT_WITH_SPACE,
                                                                  SETTINGS_EDITOR_INDENT_WITH_SPACE_DEFAULT).toBool());

    m_ui->indentSizeSpinBox->setValue(settings.value(SETTINGS_EDITOR_INDENT_SIZE,
                                                     SETTINGS_EDITOR_INDENT_SIZE_DEFAULT).toInt());

    m_ui->autoIndentCheckBox->setChecked(settings.value(SETTINGS_EDITOR_INDENT, SETTINGS_EDITOR_INDENT_DEFAULT).toBool());
    m_ui->refreshOnSaveCheckBox->setChecked(settings.value(SETTINGS_EDITOR_REFRESH_ON_SAVE, SETTINGS_EDITOR_REFRESH_ON_SAVE_DEFAULT).toBool());

    settings.endGroup();


    settings.beginGroup(SETTINGS_PREFERENCES_SECTION);
    restoreGeometry(settings.value(SETTINGS_GEOMETRY).toByteArray());
    settings.endGroup();
}

void PreferencesDialog::writeSettings()
{
    QSettings settings;

    settings.beginGroup(SETTINGS_MAIN_SECTION);

    settings.setValue(SETTINGS_USE_CUSTOM_JAVA, m_ui->customJavaRadio->isChecked());
    settings.setValue(SETTINGS_CUSTOM_JAVA_PATH, m_ui->customJavaPathEdit->text());

    settings.setValue(SETTINGS_USE_CUSTOM_PLANTUML, m_ui->customPlantUmlRadio->isChecked());
    settings.setValue(SETTINGS_CUSTOM_PLANTUML_PATH, m_ui->customPlantUmlEdit->text());

    settings.setValue(SETTINGS_USE_CUSTOM_GRAPHIZ, m_ui->customGraphizRadio->isChecked());
    settings.setValue(SETTINGS_CUSTOM_GRAPHIZ_PATH, m_ui->customGraphizEdit->text());

    settings.setValue(SETTINGS_AUTOREFRESH_TIMEOUT, m_ui->autoRefreshSpin->value() * TIMEOUT_SCALE);
    settings.setValue(SETTINGS_ASSISTANT_XML_PATH, m_ui->assistantXmlEdit->text());

    settings.setValue(SETTINGS_USE_CACHE, m_ui->cacheGroupBox->isChecked());
    settings.setValue(SETTINGS_USE_CUSTOM_CACHE, m_ui->customCacheRadio->isChecked());
    settings.setValue(SETTINGS_CUSTOM_CACHE_PATH, m_ui->customCacheEdit->text());
    settings.setValue(SETTINGS_CACHE_MAX_SIZE, m_ui->cacheMaxSize->value() * CACHE_SCALE);

    settings.endGroup();

    settings.beginGroup(SETTINGS_EDITOR_SECTION);

    QFont editorFont = m_ui->editorFontComboBox->currentFont();
    editorFont.setPointSize(m_ui->editorFontSizeSpinBox->value());
    settings.setValue(SETTINGS_EDITOR_FONT, editorFont.toString());
    settings.setValue(SETTINGS_EDITOR_INDENT, m_ui->autoIndentCheckBox->isChecked());
    settings.setValue(SETTINGS_EDITOR_INDENT_SIZE, m_ui->indentSizeSpinBox->value());
    settings.setValue(SETTINGS_EDITOR_INDENT_WITH_SPACE, m_ui->useSpacesInsteadTabsCheckBox->isChecked());
    settings.setValue(SETTINGS_EDITOR_REFRESH_ON_SAVE, m_ui->refreshOnSaveCheckBox->isChecked());

    settings.endGroup();

    settings.beginGroup(SETTINGS_PREFERENCES_SECTION);
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
    settings.endGroup();
}

void PreferencesDialog::onRejected()
{
    QSettings settings;
    settings.beginGroup(SETTINGS_PREFERENCES_SECTION);
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
    settings.endGroup();
}

void PreferencesDialog::on_customJavaPathEdit_textEdited(const QString &)
{
    m_ui->customJavaRadio->setChecked(true);
}

void PreferencesDialog::on_customJavaPathButton_clicked()
{
    QString file_name = QFileDialog::getOpenFileName(this,
                                                    tr("Select Java executable"),
                                                    m_ui->customJavaPathEdit->text());
    if (!file_name.isEmpty()) {
        m_ui->customJavaPathEdit->setText(file_name);
        m_ui->customJavaRadio->setChecked(true);
    }
}

void PreferencesDialog::on_customPlantUmlEdit_textEdited(const QString &)
{
    m_ui->customPlantUmlRadio->setChecked(true);
}

void PreferencesDialog::on_customPlantUmlButton_clicked()
{
    QString file_name = QFileDialog::getOpenFileName(this,
                                                    tr("Select PlantUML jar"),
                                                    m_ui->customPlantUmlEdit->text());
    if (!file_name.isEmpty()) {
        m_ui->customPlantUmlEdit->setText(file_name);
        m_ui->customPlantUmlRadio->setChecked(true);
    }
}

void PreferencesDialog::on_assistantXmlButton_clicked()
{
    QString file_name = QFileDialog::getOpenFileName(this,
                                                     tr("Select Assistant XML file"),
                                                     m_ui->assistantXmlEdit->text(),
                                                     tr("XML (*.xml);;All Files (*.*)"));
    if (!file_name.isEmpty()) {
        m_ui->assistantXmlEdit->setText(file_name);
    }
}

void PreferencesDialog::on_customGraphizEdit_textEdited(const QString &)
{
    m_ui->customGraphizRadio->setChecked(true);
}

void PreferencesDialog::on_customGraphizButton_clicked()
{
    QString file_name = QFileDialog::getOpenFileName(this,
                                                     tr("Select Dot (from Graphiz) executable"),
                                                     m_ui->customGraphizEdit->text());
    if (!file_name.isEmpty()) {
        m_ui->customGraphizEdit->setText(file_name);
        m_ui->customGraphizRadio->setChecked(true);
    }
}

void PreferencesDialog::on_customCacheEdit_textEdited(const QString &)
{
    m_ui->customCacheRadio->setChecked(true);
}

void PreferencesDialog::on_customCacheButton_clicked()
{
    QString dir_name = QFileDialog::getExistingDirectory(this,
                                                         tr("Select new cache location"),
                                                         m_ui->customCacheEdit->text());
    if (!dir_name.isEmpty()) {
        m_ui->customCacheEdit->setText(dir_name);
        m_ui->customCacheRadio->setChecked(true);
    }
}

void PreferencesDialog::on_clearCacheButton_clicked()
{
    if (m_fileCache) {
        m_fileCache->clearFromDisk();
        m_ui->cacheCurrentSizeLabel->setText(cacheSizeToString(m_fileCache->totalCost()));
    }
}

void PreferencesDialog::on_checkExternalPrograms_clicked()
{
    checkExternalPrograms();
}

void PreferencesDialog::onCurrentTabChanged()
{
    m_ui->checkExternalPrograms->setVisible(m_ui->tabWidget->currentWidget() == m_ui->generalTab);
}

void PreferencesDialog::checkExternalPrograms()
{
    // Get external programs' current paths.
    QString javaPath = m_ui->customJavaRadio->isChecked()
            ? m_ui->customJavaPathEdit->text()
            : SETTINGS_CUSTOM_JAVA_PATH_DEFAULT;

    QString plantUmlPath = m_ui->customPlantUmlRadio->isChecked()
            ? m_ui->customPlantUmlEdit->text()
            : SETTINGS_CUSTOM_PLANTUML_PATH_DEFAULT;

    QString graphizPath = m_ui->customGraphizRadio->isChecked()
            ? m_ui->customGraphizEdit->text()
            : SETTINGS_CUSTOM_GRAPHIZ_PATH_DEFAULT;

    QString invalidPathLog = ("<font color=\"red\">invalid path</font>");

    QString log;
    log += tr("Testing Java executable <tt>%1</tt>: ").arg(javaPath);

    bool isJavaOk = false;
    if (!QFileInfo(javaPath).exists()) {
        log += invalidPathLog;
    } else {
        isJavaOk = checkExternalProgram(javaPath, QStringList() << "-version", log);
    }

    log.append("<p>");
    log.append(tr("Testing graphiz/dot <tt>%1</tt>: ").arg(graphizPath));
    bool isGraphizOk = false;
    if (!QFileInfo(graphizPath).exists()) {
        log += invalidPathLog;
    } else {
        isGraphizOk = checkExternalProgram(graphizPath, QStringList() << "-V", log, ".*dot - graphviz version.*");
    }

    if (isJavaOk && isGraphizOk) {
        log.append("<p>");
        log.append(tr("Testing PlantUML jar <tt>%1</tt>: ").arg(plantUmlPath));

        if (!QFileInfo(plantUmlPath).exists()) {
            log += invalidPathLog;
        } else {
            checkExternalProgram(javaPath,
                                 QStringList()
                                 << "-jar"
                                 << plantUmlPath
                                 << "-graphvizdot"
                                 << graphizPath
                                 << "-testdot",
                                 log,
                                 ".*Installation seems OK. File generation OK$");
        }
    }


    if (!log.isEmpty()) {
        QDialog logDialog(this);
        Ui::LogDialog ui;

        ui.setupUi(&logDialog);
        ui.logViewer->setText(log);
        logDialog.exec();
    }
}

bool PreferencesDialog::checkExternalProgram(const QString &path, const QStringList &arguments, QString &log, const QString& validator)
{
    QProcess process;
    process.start(path, arguments);
    process.waitForFinished();

    QString output = process.readAllStandardOutput() + process.readAllStandardError();

    if (process.exitCode() == 0) {
        bool validationOk = true;

        if (!validator.isEmpty()) {
            QRegExp exp(validator);
            QString outputCopy(output);
            outputCopy = outputCopy.replace("\n", " ").trimmed();
            if (!exp.exactMatch(outputCopy)) {
                validationOk = false;
            }
        }

        if (validationOk) {
            log += tr("<b>OK</b>");
            return true;
        }
    }

    log += tr("<font color=\"red\">FAILED");
    log += "<pre>";
    log += output;
    log += "</pre></font>";
    return false;
}
