#include "mainwindow.h"
#include "previewwidget.h"
#include "preferencesdialog.h"
#include "assistantxmlreader.h"
#include "settingsconstants.h"
#include "filecache.h"
#include "recentdocuments.h"
#include "utils.h"
#include "textedit.h"

#include <QtGui>
#include <QtSvg>
#include <QtSingleApplication>
#include <QScrollArea>

namespace {
const int ASSISTANT_ITEM_DATA_ROLE = Qt::UserRole;
const int ASSISTANT_ITEM_NOTES_ROLE = Qt::UserRole + 1;

const int MAX_RECENT_DOCUMENT_SIZE = 10;
const int STATUSBAR_TIMEOUT = 3000; // in miliseconds
const QString TITLE_FORMAT_STRING = "%1[*] - %2";
const char *EXPORT_TO_MENU_FORMAT_STRING = QT_TRANSLATE_NOOP("MainWindow", "Export to %1");
const char *EXPORT_TO_LABEL_FORMAT_STRING = QT_TRANSLATE_NOOP("MainWindow", "Export to: %1");
const char *AUTOREFRESH_STATUS_LABEL = QT_TRANSLATE_NOOP("MainWindow", "Auto-refresh");
const char *CACHE_SIZE_FORMAT_STRING = QT_TRANSLATE_NOOP("MainWindow", "Cache: %1");
const QSize ASSISTANT_ICON_SIZE(128, 128);

QIcon iconFromSvg(QSize size, const QString& path)
{
    QPixmap pixmap(size);
    QPainter painter(&pixmap);
    const QRect bounding_rect(QPoint(0, 0), size);

    if (!path.isEmpty()) {
        painter.setRenderHint(QPainter::Antialiasing, true);

        painter.setBrush(QBrush(Qt::white, Qt::SolidPattern));
        painter.setPen(Qt::NoPen);
        painter.drawRect(bounding_rect);

        QSvgRenderer svg(path);
        QSize target_size = svg.defaultSize();
        target_size.scale(size, Qt::KeepAspectRatio);
        QRect target_rect = QRect(QPoint(0, 0), target_size);
        target_rect.translate(bounding_rect.center() - target_rect.center());
        svg.render(&painter, target_rect);
    } else {
        painter.setBrush(QBrush(Qt::white, Qt::SolidPattern));
        painter.setPen(Qt::NoPen);
        painter.drawRect(bounding_rect);

        const int margin = 5;
        QRect target_rect = bounding_rect.adjusted(margin, margin, -margin, -margin);
        painter.setPen(Qt::SolidLine);
        painter.drawRect(target_rect);
        painter.drawLine(target_rect.topLeft(), target_rect.bottomRight() + QPoint(1, 1));
        painter.drawLine(target_rect.bottomLeft() + QPoint(0, 1), target_rect.topRight() + QPoint(1, 0));
    }

    QIcon icon;
    icon.addPixmap(pixmap);
    return icon;
}

QListWidget* newAssistantListWidget(const QSize& icon_size, QWidget* parent)
{
    QListWidget* view = new QListWidget(parent);
    view->setUniformItemSizes(true);
    view->setMovement(QListView::Static);
    view->setResizeMode(QListView::Adjust);
    view->setIconSize(icon_size);
    view->setViewMode(QListView::IconMode);
    return view;
}

} // namespace {}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_hasValidPaths(false)
    , m_process(0)
    , m_currentImageFormat(SvgFormat)
    , m_needsRefresh(false)
{
    setWindowTitle(TITLE_FORMAT_STRING
                   .arg("")
                   .arg(qApp->applicationName())
                   );

    setWindowIcon(QIcon(":/icon32.png"));

    m_cache = new FileCache(0, this);

    m_recentDocuments = new RecentDocuments(MAX_RECENT_DOCUMENT_SIZE, this);
    connect(m_recentDocuments, SIGNAL(recentDocument(QString)), this, SLOT(onRecentDocumentsActionTriggered(QString)));

    m_autoRefreshTimer = new QTimer(this);
    connect(m_autoRefreshTimer, SIGNAL(timeout()), this, SLOT(refresh()));

    m_imageFormatNames[SvgFormat] = "svg";
    m_imageFormatNames[PngFormat] = "png";

    m_imageWidget = new PreviewWidget(this);

    m_imageWidgetScrollArea = new QScrollArea;
    m_imageWidgetScrollArea->setWidget(m_imageWidget);
    m_imageWidgetScrollArea->setAlignment(Qt::AlignCenter);
    m_imageWidgetScrollArea->setWidgetResizable(true);
    setCentralWidget(m_imageWidgetScrollArea);

    createDockWindows();
    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    setUnifiedTitleAndToolBarOnMac(true);

    m_assistantInsertSignalMapper = new QSignalMapper(this);
    connect(m_assistantInsertSignalMapper, SIGNAL(mapped(QWidget*)),
            this, SLOT(onAssistantItemInsert(QWidget*)));

    readSettings();

    QtSingleApplication* single_app = qobject_cast<QtSingleApplication*>(qApp);
    if (single_app) {
        single_app->setActivationWindow(this);
        connect(single_app, SIGNAL(messageReceived(QString)),
                this, SLOT(onSingleApplicationReceivedMessage(QString)));
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::newDocument()
{
    if (!maybeSave()) {
        return;
    }

    m_documentPath.clear();
    m_exportPath.clear();
    m_cachedImage.clear();
    m_exportImageAction->setText(tr(EXPORT_TO_MENU_FORMAT_STRING).arg(""));
    m_exportPathLabel->setText(tr(EXPORT_TO_LABEL_FORMAT_STRING).arg(""));
    m_exportPathLabel->setEnabled(false);

    QString text = "@startuml\n\nclass Foo\n\n@enduml";
    m_editor->setPlainText(text);
    setWindowTitle(TITLE_FORMAT_STRING
                   .arg(tr("Untitled"))
                   .arg(qApp->applicationName())
                   );
    setWindowModified(false);
    refresh();

    enableUndoRedoActions();
}

void MainWindow::copyImage()
{
    QPixmap pixmap;
    pixmap.loadFromData(m_cachedImage);
    QApplication::clipboard()->setPixmap(pixmap);
    qDebug() << "Image copy into Clipboard";
}

void MainWindow::undo()
{
    QTextDocument *document = m_editor->document();
    document->undo();
    enableUndoRedoActions();
}

void MainWindow::redo()
{
    QTextDocument *document = m_editor->document();
    document->redo();
    enableUndoRedoActions();
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About %1").arg(qApp->applicationName()),
                       tr(
                           "The <i>%1</i> allows simple edit and preview of UML "
                           "diagrams generated with <i>%2</i>.<br>"
                           "<br>"
                           "%2, %3 and %4 must be installed before "
                           "using the editor.<br>"
                           "<br>"
                           "Copyright (c) 2012-2017 - Ioan Călin Borcoman"
                           )
                       .arg(qApp->applicationName())
                       .arg("PlantUML")
                       .arg("Java")
                       .arg("Graphiz")
                       );
}

QString MainWindow::makeKeyForDocument(QByteArray current_document)
{
    QString key = QString("%1.%2")
            .arg(QString::fromUtf8(QCryptographicHash::hash(current_document, QCryptographicHash::Md5).toHex()))
            .arg(m_imageFormatNames[m_currentImageFormat])
            ;

    return key;
}

bool MainWindow::refreshFromCache()
{
    if (m_useCache) {
        QByteArray current_document = m_editor->toPlainText().toUtf8().trimmed();
        if (current_document.isEmpty()) {
            qDebug() << "empty document. skipping...";
            return true;
        }

        switch(m_currentImageFormat) {
        case SvgFormat:
            m_imageWidget->setMode(PreviewWidget::SvgMode);
            break;
        case PngFormat:
            m_imageWidget->setMode(PreviewWidget::PngMode);
            break;
        }

        QString key = makeKeyForDocument(current_document);
        // try the cache first
        const FileCacheItem* item = qobject_cast<const FileCacheItem*>(m_cache->item(key));
        if (item) {
            QFile file(item->path());
            if (file.open(QFile::ReadOnly)) {
                QByteArray cache_image = file.readAll();
                if (cache_image.size()) {
                    m_cachedImage = cache_image;
                    m_imageWidget->load(m_cachedImage);
                    statusBar()->showMessage(tr("Cache hit: %1").arg(key), STATUSBAR_TIMEOUT);
                    m_needsRefresh = false;
                    return true;
                }
            }
        }
    }

    return false;
}

void MainWindow::refresh(bool forced)
{
    if (m_process) {
        qDebug() << "still processing previous refresh. skipping...";
        return;
    }

    if (!m_needsRefresh && !forced) {
        return;
    }

    if (!m_hasValidPaths) {
        qDebug() << "Please configure paths for Java and PlantUML. Aborting...";
        statusBar()->showMessage(tr("Java and/or PlantUML not found. Please set them correctly in the \"Preferences\" dialog!"));
        return;
    }

    if (!forced && refreshFromCache()) {
        return;
    }

    QByteArray current_document = m_editor->toPlainText().toUtf8().trimmed();
    if (current_document.isEmpty()) {
        qDebug() << "empty document. skipping...";
        return;
    }
    m_needsRefresh = false;

    switch(m_currentImageFormat) {
    case SvgFormat:
        m_imageWidget->setMode(PreviewWidget::SvgMode);
        break;
    case PngFormat:
        m_imageWidget->setMode(PreviewWidget::PngMode);
        break;
    }

    QString key = makeKeyForDocument(current_document);

    statusBar()->showMessage(tr("Refreshing..."));

    QStringList arguments;

    arguments
            << "-jar" << m_plantUmlPath
            << QString("-t%1").arg(m_imageFormatNames[m_currentImageFormat]);
    if (m_useCustomGraphiz)
        arguments << "-graphvizdot" << m_graphizPath;
    arguments << "-charset" << "UTF-8" << "-pipe";

    m_lastKey = key;
    m_process = new QProcess(this);

    QFileInfo fi(m_documentPath);
    m_process->setWorkingDirectory(fi.absolutePath());

    m_process->start(m_javaPath, arguments);
    if (!m_process->waitForStarted()) {
        qDebug() << "refresh subprocess failed to start";
        return;
    }

    connect(m_process, SIGNAL(finished(int)), this, SLOT(refreshFinished()));

    m_process->write(current_document);
    m_process->closeWriteChannel();
}

void MainWindow::updateCacheSizeInfo()
{
    m_cacheSizeLabel->setText(m_useCache ?
                                  tr(CACHE_SIZE_FORMAT_STRING).arg(cacheSizeToString(m_cache->totalCost())) :
                                  tr("NO CACHE"));
}

void MainWindow::focusAssistant()
{
    QListWidget* widget = qobject_cast<QListWidget*>(m_assistantToolBox->currentWidget());
    if (widget) {
        widget->setFocus();
        if (widget->selectedItems().count() == 0) {
            widget->setCurrentItem(widget->itemAt(0, 0));
        }
    }
}

void MainWindow::refreshFinished()
{
    m_cachedImage = m_process->readAll();
    m_imageWidget->load(m_cachedImage);
    m_process->deleteLater();
    m_process = 0;

    if (m_useCache && m_cache) {
        m_cache->addItem(m_cachedImage, m_lastKey,
                         [](const QString& path,
                            const QString& key,
                            int cost,
                            const QDateTime& date_time,
                            QObject* parent
                            ) { return new FileCacheItem(path, key, cost, date_time, parent); });
        updateCacheSizeInfo();
    }
    statusBar()->showMessage(tr("Refreshed"), STATUSBAR_TIMEOUT);
}

void MainWindow::changeImageFormat()
{
    ImageFormat new_format;
    if (m_pngPreviewAction->isChecked()) {
        new_format = PngFormat;
    } else {
        new_format = SvgFormat;
    }

    if (new_format != m_currentImageFormat) {
        m_currentImageFormat = new_format;
        m_needsRefresh = true;
        m_currentImageFormatLabel->setText(m_imageFormatNames[m_currentImageFormat].toUpper());
        refresh();
    }
}

void MainWindow::onAutoRefreshActionToggled(bool state)
{
    if (state) {
        refresh();
        m_autoRefreshTimer->start();
    } else {
        m_autoRefreshTimer->stop();
    }
    m_autoRefreshLabel->setEnabled(state);
}

void MainWindow::onEditorChanged()
{
    if (!refreshFromCache()) m_needsRefresh = true;

    setWindowModified(true);

    enableUndoRedoActions();
}

void MainWindow::onRefreshActionTriggered()
{
    m_needsRefresh = true;
    refresh(true);
}

void MainWindow::onPreferencesActionTriggered()
{
    writeSettings();
    PreferencesDialog dialog(m_cache, this);
    dialog.readSettings();
    dialog.exec();

    if (dialog.result() == QDialog::Accepted) {
        dialog.writeSettings();
        readSettings(true);
    }
}

void MainWindow::onOpenDocumentActionTriggered()
{
    openDocument("");
}

void MainWindow::onSaveActionTriggered()
{
    saveDocument(m_documentPath);
    if (m_refreshOnSave)
        onRefreshActionTriggered();
}

void MainWindow::onSaveAsActionTriggered()
{
    saveDocument("");
}

void MainWindow::onExportImageActionTriggered()
{
    exportImage(m_exportPath);
}

void MainWindow::onExportAsImageActionTriggered()
{
    exportImage("");
}

void MainWindow::onRecentDocumentsActionTriggered(const QString &path)
{
    openDocument(path);
}

void MainWindow::onAssistanItemDoubleClicked(QListWidgetItem *item)
{
    insertAssistantCode(item->data(Qt::UserRole).toString());
    m_editor->setFocus(); // force focus to move to the editor
}

void MainWindow::onSingleApplicationReceivedMessage(const QString &message)
{
    // the message is a file to open
    QtSingleApplication* single_app = qobject_cast<QtSingleApplication*>(qApp);
    if (single_app) {
        single_app->activateWindow();
        qDebug() << "single instance activated";
    }

    if (!message.isEmpty()) {
        qDebug() << "received request to open " << message << "from another instance";
        openDocument(message);
    }
}

void MainWindow::onAssistantFocus()
{
    focusAssistant();
}

void MainWindow::onAssistantItemInsert(QWidget *widget)
{
    QListWidget* list_widget = qobject_cast<QListWidget*>(widget);
    if (list_widget) {
        onAssistanItemDoubleClicked(list_widget->currentItem());
    }
}

void MainWindow::onNextAssistant()
{
    m_assistantToolBox->setCurrentIndex((m_assistantToolBox->currentIndex() + 1) % m_assistantToolBox->count());
}

void MainWindow::onPrevAssistant()
{
    const int count = m_assistantToolBox->count();
    m_assistantToolBox->setCurrentIndex((count + m_assistantToolBox->currentIndex() - 1) % count);
}

void MainWindow::onAssistantItemSelectionChanged()
{
    QListWidget* widget = qobject_cast<QListWidget*>(m_assistantToolBox->currentWidget());
    if (widget) {
        QListWidgetItem* item = widget->currentItem();
        if (item) {
            QString notes = item->data(ASSISTANT_ITEM_NOTES_ROLE).toString();
            m_assistantPreviewNotes->setText(notes.isEmpty() ?
                                                 tr("Code:") :
                                                 tr("Notes:<br>%1<br>Code:").arg(notes));
            m_assistantCodePreview->setPlainText(item->data(ASSISTANT_ITEM_DATA_ROLE).toString());
        }
    }
}

void MainWindow::onCurrentAssistantChanged(int /*index*/)
{
    focusAssistant();
    onAssistantItemSelectionChanged(); // make sure we don't show stale info
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        writeSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

bool MainWindow::maybeSave()
{
    if (m_editor->document()->isModified()) {
        QMessageBox::StandardButton ret;
        ret = QMessageBox::warning(this, qApp->applicationName(),
                                   tr("The document has been modified.\n"
                                      "Do you want to save your changes?"),
                                   QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (ret == QMessageBox::Save)
            return saveDocument(m_documentPath);
        else if (ret == QMessageBox::Cancel)
            return false;
    }
    return true;
}

void MainWindow::readSettings(bool reload)
{
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    const QString DEFAULT_CACHE_PATH = QDesktopServices::storageLocation(QDesktopServices::CacheLocation);
#else
    const QString DEFAULT_CACHE_PATH = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#endif

    QSettings settings;

    settings.beginGroup(SETTINGS_MAIN_SECTION);

    m_useCustomJava = settings.value(SETTINGS_USE_CUSTOM_JAVA, SETTINGS_USE_CUSTOM_JAVA_DEFAULT).toBool();
    m_customJavaPath = settings.value(SETTINGS_CUSTOM_JAVA_PATH, SETTINGS_CUSTOM_JAVA_PATH_DEFAULT).toString();
    m_javaPath = m_useCustomJava ? m_customJavaPath : SETTINGS_CUSTOM_JAVA_PATH_DEFAULT;

    m_useCustomPlantUml = settings.value(SETTINGS_USE_CUSTOM_PLANTUML, SETTINGS_USE_CUSTOM_PLANTUML_DEFAULT).toBool();
    m_customPlantUmlPath = settings.value(SETTINGS_CUSTOM_PLANTUML_PATH, SETTINGS_CUSTOM_PLANTUML_PATH_DEFAULT).toString();
    m_plantUmlPath = m_useCustomPlantUml ? m_customPlantUmlPath : SETTINGS_CUSTOM_PLANTUML_PATH_DEFAULT;

    m_useCustomGraphiz = settings.value(SETTINGS_USE_CUSTOM_GRAPHIZ, SETTINGS_USE_CUSTOM_GRAPHIZ_DEFAULT).toBool();
    m_customGraphizPath = settings.value(SETTINGS_CUSTOM_GRAPHIZ_PATH, SETTINGS_CUSTOM_GRAPHIZ_PATH_DEFAULT).toString();
    m_graphizPath = m_useCustomGraphiz ? m_customGraphizPath : SETTINGS_CUSTOM_GRAPHIZ_PATH_DEFAULT;

    checkPaths();

    m_useCache = settings.value(SETTINGS_USE_CACHE, SETTINGS_USE_CACHE_DEFAULT).toBool();
    m_useCustomCache = settings.value(SETTINGS_USE_CUSTOM_CACHE, SETTINGS_USE_CUSTOM_CACHE_DEFAULT).toBool();
    m_customCachePath = settings.value(SETTINGS_CUSTOM_CACHE_PATH, DEFAULT_CACHE_PATH).toString();
    m_cacheMaxSize = settings.value(SETTINGS_CACHE_MAX_SIZE, SETTINGS_CACHE_MAX_SIZE_DEFAULT).toInt();
    m_cachePath = m_useCustomCache ? m_customCachePath : DEFAULT_CACHE_PATH;

    m_cache->setMaxCost(m_cacheMaxSize);
    m_cache->setPath(m_cachePath, [](const QString& path,
                                     const QString& key,
                                     int cost,
                                     const QDateTime& date_time,
                                     QObject* parent
                                     ) { return new FileCacheItem(path, key, cost, date_time, parent); });

    reloadAssistantXml(settings.value(SETTINGS_ASSISTANT_XML_PATH).toString());

    const bool autorefresh_enabled = settings.value(SETTINGS_AUTOREFRESH_ENABLED, false).toBool();
    m_autoRefreshAction->setChecked(autorefresh_enabled);
    m_autoRefreshTimer->setInterval(settings.value(SETTINGS_AUTOREFRESH_TIMEOUT, SETTINGS_AUTOREFRESH_TIMEOUT_DEFAULT).toInt());
    if (autorefresh_enabled) {
        m_autoRefreshTimer->start();
    }
    m_autoRefreshLabel->setEnabled(autorefresh_enabled);

    m_autoSaveImageAction->setChecked(settings.value(SETTINGS_AUTOSAVE_IMAGE_ENABLED, SETTINGS_AUTOSAVE_IMAGE_ENABLED_DEFAULT).toBool());

    if (!reload) {
        restoreGeometry(settings.value(SETTINGS_GEOMETRY).toByteArray());
        restoreState(settings.value(SETTINGS_WINDOW_STATE).toByteArray());
    }

    m_showMainToolbarAction->setChecked(m_mainToolBar->isVisibleTo(this)); // NOTE: works even if the current window is not yet displayed
    connect(m_showMainToolbarAction, SIGNAL(toggled(bool)), m_mainToolBar, SLOT(setVisible(bool)));

    const bool show_statusbar = settings.value(SETTINGS_SHOW_STATUSBAR, true).toBool();
    m_showStatusBarAction->setChecked(show_statusbar);
    statusBar()->setVisible(show_statusbar);
    connect(m_showStatusBarAction, SIGNAL(toggled(bool)), statusBar(), SLOT(setVisible(bool)));

    m_currentImageFormat = m_imageFormatNames.key(settings.value(SETTINGS_IMAGE_FORMAT, m_imageFormatNames[SvgFormat]).toString());
    if (m_currentImageFormat == SvgFormat) {
        m_svgPreviewAction->setChecked(true);
    } else if (m_currentImageFormat == PngFormat) {
        m_pngPreviewAction->setChecked(true);
    }
    m_currentImageFormatLabel->setText(m_imageFormatNames[m_currentImageFormat].toUpper());

    m_lastDir = settings.value(SETTINGS_EDITOR_LAST_DIR, SETTINGS_EDITOR_LAST_DIR_DEFAULT).toString();

    settings.endGroup();

    settings.beginGroup(SETTINGS_EDITOR_SECTION);

    QFont defaultFont;
    QFont editorFont;
    editorFont.fromString(settings.value(SETTINGS_EDITOR_FONT,
                                         defaultFont.toString()).toString());
    m_editor->setFont(editorFont);
    m_editor->setAutoIndent(settings.value(SETTINGS_EDITOR_INDENT, SETTINGS_EDITOR_INDENT_DEFAULT).toBool());
    m_editor->setIndentWithSpace(settings.value(SETTINGS_EDITOR_INDENT_WITH_SPACE,
                                                SETTINGS_EDITOR_INDENT_WITH_SPACE_DEFAULT).toBool());
    m_editor->setIndentSize(settings.value(SETTINGS_EDITOR_INDENT_SIZE,
                                           SETTINGS_EDITOR_INDENT_SIZE_DEFAULT).toInt());

    m_refreshOnSave = settings.value(SETTINGS_EDITOR_REFRESH_ON_SAVE, SETTINGS_EDITOR_REFRESH_ON_SAVE_DEFAULT).toBool();

    settings.endGroup();

    m_recentDocuments->readFromSettings(settings, SETTINGS_RECENT_DOCUMENTS_SECTION);
    updateCacheSizeInfo();
}

void MainWindow::writeSettings()
{
    QSettings settings;

    settings.beginGroup(SETTINGS_MAIN_SECTION);

    settings.setValue(SETTINGS_USE_CUSTOM_JAVA, m_useCustomJava);
    settings.setValue(SETTINGS_CUSTOM_JAVA_PATH, m_customJavaPath);

    settings.setValue(SETTINGS_USE_CUSTOM_PLANTUML, m_useCustomPlantUml);
    settings.setValue(SETTINGS_CUSTOM_PLANTUML_PATH, m_customPlantUmlPath);

    settings.setValue(SETTINGS_USE_CUSTOM_GRAPHIZ, m_useCustomGraphiz);
    settings.setValue(SETTINGS_CUSTOM_GRAPHIZ_PATH, m_customGraphizPath);

    settings.setValue(SETTINGS_USE_CACHE, m_useCache);
    settings.setValue(SETTINGS_USE_CUSTOM_CACHE, m_useCustomCache);
    settings.setValue(SETTINGS_CUSTOM_CACHE_PATH, m_customCachePath);
    settings.setValue(SETTINGS_CACHE_MAX_SIZE, m_cacheMaxSize);

    settings.setValue(SETTINGS_ASSISTANT_XML_PATH, m_assistantXmlPath);

    settings.setValue(SETTINGS_AUTOREFRESH_TIMEOUT, m_autoRefreshTimer->interval());

    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
    settings.setValue(SETTINGS_WINDOW_STATE, saveState());
    settings.setValue(SETTINGS_SHOW_STATUSBAR, m_showStatusBarAction->isChecked());
    settings.setValue(SETTINGS_AUTOREFRESH_ENABLED, m_autoRefreshAction->isChecked());
    settings.setValue(SETTINGS_AUTOSAVE_IMAGE_ENABLED, m_autoSaveImageAction->isChecked());
    settings.setValue(SETTINGS_IMAGE_FORMAT, m_imageFormatNames[m_currentImageFormat]);

    settings.setValue(SETTINGS_EDITOR_LAST_DIR, m_lastDir);

    settings.endGroup();

    m_recentDocuments->writeToSettings(settings, SETTINGS_RECENT_DOCUMENTS_SECTION);
}

void MainWindow::openDocument(const QString &name)
{
    if (!maybeSave()) {
        return;
    }

    QString tmp_name = name;

    if (tmp_name.isEmpty() || QFileInfo(tmp_name).exists() == false) {
        tmp_name = QFileDialog::getOpenFileName(this,
                                                tr("Select a file to open"),
                                                m_lastDir,
                                                "PlantUML (*.plantuml);; All Files (*.*)"
                                                );
        if (tmp_name.isEmpty()) {
            return;
        } else {
            QFileInfo fi(tmp_name);
            m_lastDir = fi.absolutePath();
        }
    }

    QFile file(tmp_name);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    m_editor->setPlainText(QString::fromUtf8(file.readAll()));
    setWindowModified(false);
    m_documentPath = tmp_name;
    setWindowTitle(TITLE_FORMAT_STRING
                   .arg(QFileInfo(tmp_name).fileName())
                   .arg(qApp->applicationName())
                   );
    m_needsRefresh = true;
    refresh();
    m_recentDocuments->accessing(tmp_name);
}

bool MainWindow::saveDocument(const QString &name)
{
    QString file_path = name;
    if (file_path.isEmpty()) {
        file_path = QFileDialog::getSaveFileName(this,
                                                tr("Select where to store the document"),
                                                m_lastDir,
                                                "PlantUML (*.plantuml);; All Files (*.*)"
                                                );
        if (file_path.isEmpty()) {
            return false;
        } else {
            QFileInfo fi(file_path);
            m_lastDir = fi.absolutePath();
        }
    }

    qDebug() << "saving document in:" << file_path;
    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(m_editor->toPlainText().toUtf8());
    file.close();
    m_documentPath = file_path;
    setWindowTitle(TITLE_FORMAT_STRING
                   .arg(QFileInfo(file_path).fileName())
                   .arg(qApp->applicationName())
                   );
    statusBar()->showMessage(tr("Document save in %1").arg(file_path), STATUSBAR_TIMEOUT);
    m_recentDocuments->accessing(file_path);

    if (m_autoSaveImageAction->isChecked()) {
        QFileInfo info(file_path);
        QString image_path = QString("%1/%2.%3")
                .arg(info.absolutePath())
                .arg(info.baseName())
                .arg(m_imageFormatNames[m_currentImageFormat])
                ;
        qDebug() << "saving image in:   " << image_path;
        QFile image(image_path);
        if (!image.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        image.write(m_cachedImage);
        image.close();
    }
    m_editor->document()->setModified(false);
    setWindowModified(false);
    return true;
}

void MainWindow::exportImage(const QString &name)
{
    if (m_cachedImage.isEmpty()) {
        qDebug() << "no image to export. aborting...";
        return;
    }

	QFileInfo finfo(m_documentPath);
	const QString docPathWithBaseFilename = finfo.absolutePath() + QDir::separator() + finfo.baseName();

    QString tmp_name = name;
    if (tmp_name.isEmpty()) {
        tmp_name = QFileDialog::getSaveFileName(this,
                                                tr("Select where to export the image"),
												docPathWithBaseFilename,
                                                "Image (*.svg *.png);; All Files (*.*)"
                                                );
        if (tmp_name.isEmpty()) {
            return;
        }
    }

    qDebug() << "exporting image in:" << tmp_name;

    QFile file(tmp_name);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(m_cachedImage);
    m_exportImageAction->setText(tr(EXPORT_TO_MENU_FORMAT_STRING).arg(tmp_name));
    m_exportPath = tmp_name;
    QString short_tmp_name = QFileInfo(tmp_name).fileName();
    statusBar()->showMessage(tr("Image exported in %1").arg(short_tmp_name), STATUSBAR_TIMEOUT);
    m_exportPathLabel->setText(tr(EXPORT_TO_LABEL_FORMAT_STRING).arg(short_tmp_name));
    m_exportPathLabel->setEnabled(true);
}

void MainWindow::createActions()
{
    // File menu
    m_newDocumentAction = new QAction(QIcon::fromTheme("document-new"), tr("&New"), this);
    m_newDocumentAction->setShortcut(QKeySequence::New);
    connect(m_newDocumentAction, SIGNAL(triggered()), this, SLOT(newDocument()));

    m_openDocumentAction = new QAction(QIcon::fromTheme("document-open"), tr("&Open"), this);
    m_openDocumentAction->setShortcuts(QKeySequence::Open);
    connect(m_openDocumentAction, SIGNAL(triggered()), this, SLOT(onOpenDocumentActionTriggered()));

    m_saveDocumentAction = new QAction(QIcon::fromTheme("document-save"), tr("&Save"), this);
    m_saveDocumentAction->setShortcuts(QKeySequence::Save);
    connect(m_saveDocumentAction, SIGNAL(triggered()), this, SLOT(onSaveActionTriggered()));

    m_saveAsDocumentAction = new QAction(QIcon::fromTheme("document-save-as"), tr("Save As..."), this);
    m_saveAsDocumentAction->setShortcuts(QKeySequence::SaveAs);
    connect(m_saveAsDocumentAction, SIGNAL(triggered()), this, SLOT(onSaveAsActionTriggered()));

    m_exportImageAction = new QAction(tr(EXPORT_TO_MENU_FORMAT_STRING).arg(""), this);
    m_exportImageAction->setShortcut(Qt::CTRL + Qt::Key_E);
    connect(m_exportImageAction, SIGNAL(triggered()), this, SLOT(onExportImageActionTriggered()));

    m_exportAsImageAction = new QAction(tr("Export as ..."), this);
    m_exportAsImageAction->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_E);
    connect(m_exportAsImageAction, SIGNAL(triggered()), this, SLOT(onExportAsImageActionTriggered()));

    m_quitAction = new QAction(QIcon::fromTheme("application-exit"), tr("&Quit"), this);
    m_quitAction->setShortcuts(QKeySequence::Quit);
    m_quitAction->setStatusTip(tr("Quit the application"));
    connect(m_quitAction, SIGNAL(triggered()), this, SLOT(close()));

    // Edit menu
    m_undoAction = new QAction(QIcon::fromTheme("edit-undo"), tr("&Undo"), this);
    m_undoAction->setShortcuts(QKeySequence::Undo);
    connect(m_undoAction, SIGNAL(triggered()), this, SLOT(undo()));

    m_redoAction = new QAction(QIcon::fromTheme("edit-redo"), tr("&Redo"), this);
    m_redoAction->setShortcuts(QKeySequence::Redo);
    connect(m_redoAction, SIGNAL(triggered()), this, SLOT(redo()));

    m_copyImageAction = new QAction(QIcon::fromTheme("copy"), tr("&Copy Image"), this);
    m_copyImageAction->setShortcuts(QList<QKeySequence>()
                                    << QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C)
                                    << QKeySequence::Copy
                                   );
    connect(m_copyImageAction, SIGNAL(triggered()), this, SLOT(copyImage()) );

    // Tools menu
    m_pngPreviewAction = new QAction(tr("PNG"), this);
    m_pngPreviewAction->setCheckable(true);
    m_pngPreviewAction->setStatusTip(tr("Tell PlantUML to produce PNG output"));
    connect(m_pngPreviewAction, SIGNAL(toggled(bool)), this, SLOT(changeImageFormat()));

    m_svgPreviewAction = new QAction(tr("SVG"), this);
    m_svgPreviewAction->setCheckable(true);
    m_svgPreviewAction->setStatusTip(tr("Tell PlantUML to produce SVG output"));
    connect(m_svgPreviewAction, SIGNAL(toggled(bool)), this, SLOT(changeImageFormat()));

    QActionGroup* output_action_group = new QActionGroup(this);
    output_action_group->setExclusive(true);
    output_action_group->addAction(m_pngPreviewAction);
    output_action_group->addAction(m_svgPreviewAction);
    m_svgPreviewAction->setChecked(true);

    m_refreshAction = new QAction(QIcon::fromTheme("view-refresh"), tr("Refresh"), this);
    m_refreshAction->setShortcuts(QKeySequence::Refresh);
    m_refreshAction->setStatusTip(tr("Call PlantUML to regenerate the UML image"));
    connect(m_refreshAction, SIGNAL(triggered()), this, SLOT(onRefreshActionTriggered()));

    m_autoRefreshAction = new QAction(tr("Auto-Refresh"), this);
    m_autoRefreshAction->setCheckable(true);

    m_autoSaveImageAction = new QAction(tr("Auto-Save image"), this);
    m_autoSaveImageAction->setCheckable(true);
    connect(m_autoRefreshAction, SIGNAL(toggled(bool)), this, SLOT(onAutoRefreshActionToggled(bool)));

    // Settings menu
    m_showMainToolbarAction = new QAction(tr("Show toolbar"), this);
    m_showMainToolbarAction->setCheckable(true);

    m_showStatusBarAction = new QAction(tr("Show statusbar"), this);
    m_showStatusBarAction->setCheckable(true);

    m_preferencesAction = new QAction(QIcon::fromTheme("preferences-other"), tr("Preferences"), this);
    connect(m_preferencesAction, SIGNAL(triggered()), this, SLOT(onPreferencesActionTriggered()));

    // Help menu
    m_aboutAction = new QAction(QIcon::fromTheme("help-about"), tr("&About"), this);
    m_aboutAction->setStatusTip(tr("Show the application's About box"));
    connect(m_aboutAction, SIGNAL(triggered()), this, SLOT(about()));

    m_aboutQtAction = new QAction(tr("About &Qt"), this);
    m_aboutQtAction->setStatusTip(tr("Show the Qt library's About box"));
    connect(m_aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

    // focus actions
    QAction* focus_action = new QAction(this);
    focus_action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_0));
    connect(focus_action, SIGNAL(triggered()), m_editor, SLOT(setFocus()));
    this->addAction(focus_action);

    focus_action = new QAction(this);
    focus_action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_1));
    connect(focus_action, SIGNAL(triggered()), this, SLOT(onAssistantFocus()));
    this->addAction(focus_action);

    // assistant actions
    QAction* navigation_action = new QAction(this);
    navigation_action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Down));
    connect(navigation_action, SIGNAL(triggered()), this, SLOT(onNextAssistant()));
    addAction(navigation_action);

    navigation_action = new QAction(this);
    navigation_action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Up));
    connect(navigation_action, SIGNAL(triggered()), this, SLOT(onPrevAssistant()));
    addAction(navigation_action);

    // zoom action
    m_zoomInAction = new QAction(QIcon::fromTheme("zoom-in"), tr("Zoom In"), this);
    connect(m_zoomInAction, SIGNAL(triggered()), m_imageWidget, SLOT(zoomIn()));
    m_zoomOutAction = new QAction(QIcon::fromTheme("zoom-out"), tr("Zoom Out"), this);
    connect(m_zoomOutAction, SIGNAL(triggered()), m_imageWidget, SLOT(zoomOut()));
    m_zoomOriginalAction = new QAction(QIcon::fromTheme("zoom-original"), tr("1:1"), this);
    connect(m_zoomOriginalAction, SIGNAL(triggered()), m_imageWidget, SLOT(zoomOriginal()));
}

void MainWindow::createMenus()
{
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileMenu->addAction(m_newDocumentAction);
    m_fileMenu->addAction(m_openDocumentAction);
    m_fileMenu->addAction(m_saveDocumentAction);
    m_fileMenu->addAction(m_saveAsDocumentAction);
    m_fileMenu->addSeparator();
    QMenu *recent_documents_submenu = m_fileMenu->addMenu(tr("Recent Documents"));
    recent_documents_submenu->addActions(m_recentDocuments->actions());
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exportImageAction);
    m_fileMenu->addAction(m_exportAsImageAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_quitAction);

    m_editMenu = menuBar()->addMenu(tr("&Edit"));
    m_editMenu->addAction(m_undoAction);
    m_editMenu->addAction(m_redoAction);
    m_editMenu->addAction(m_copyImageAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_refreshAction);

    m_settingsMenu = menuBar()->addMenu(tr("&Settings"));
    m_settingsMenu->addAction(m_showMainToolbarAction);
    m_settingsMenu->addAction(m_showStatusBarAction);
    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_showAssistantDockAction);
    m_settingsMenu->addAction(m_showAssistantInfoDockAction);
    m_settingsMenu->addAction(m_showEditorDockAction);
    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_pngPreviewAction);
    m_settingsMenu->addAction(m_svgPreviewAction);
    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_autoRefreshAction);
    m_settingsMenu->addAction(m_autoSaveImageAction);
    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_preferencesAction);

    m_zoomMenu = menuBar()->addMenu(tr("&Zoom"));
    addZoomActions(m_zoomMenu);

    menuBar()->addSeparator();

    m_helpMenu = menuBar()->addMenu(tr("&Help"));
    m_helpMenu->addAction(m_aboutAction);
    m_helpMenu->addAction(m_aboutQtAction);
}

void MainWindow::createToolBars()
{
    m_mainToolBar = addToolBar(tr("MainToolbar"));
    m_mainToolBar->setObjectName("main_toolbar");
    m_mainToolBar->addAction(m_newDocumentAction);
    m_mainToolBar->addAction(m_openDocumentAction);
    m_mainToolBar->addAction(m_saveDocumentAction);
    m_mainToolBar->addAction(m_saveAsDocumentAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_showAssistantDockAction);
    m_mainToolBar->addAction(m_showAssistantInfoDockAction);
    m_mainToolBar->addAction(m_showEditorDockAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_undoAction);
    m_mainToolBar->addAction(m_redoAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_refreshAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_preferencesAction);

    m_zoomToolBar = addToolBar(tr("ZoomToolbar"));
    m_zoomToolBar->setObjectName("zoom_toolbar");
    addZoomActions(m_zoomToolBar);
}

void MainWindow::createStatusBar()
{
    m_exportPathLabel = new QLabel(this);
    m_exportPathLabel->setMinimumWidth(200);
    m_exportPathLabel->setText(tr(EXPORT_TO_LABEL_FORMAT_STRING).arg(""));
    m_exportPathLabel->setEnabled(false);

    m_currentImageFormatLabel = new QLabel(this);

    QFontMetrics font_metrics(m_exportPathLabel->font());
    m_cacheSizeLabel = new QLabel(this);
    m_cacheSizeLabel->setMinimumWidth(font_metrics.width(tr(CACHE_SIZE_FORMAT_STRING).arg("#.## Mb")));

    m_autoRefreshLabel = new QLabel(this);
    m_autoRefreshLabel->setText(tr(AUTOREFRESH_STATUS_LABEL));

#ifdef Q_WS_X11
    const int label_fram_style = QFrame::Panel | QFrame::Sunken;
    m_exportPathLabel->setFrameStyle(label_fram_style);
    m_currentImageFormatLabel->setFrameStyle(label_fram_style);
    m_cacheSizeLabel->setFrameStyle(label_fram_style);
    m_autoRefreshLabel->setFrameStyle(label_fram_style);
#endif

    statusBar()->addPermanentWidget(m_exportPathLabel);
    statusBar()->addPermanentWidget(m_cacheSizeLabel);
    statusBar()->addPermanentWidget(m_autoRefreshLabel);
    statusBar()->addPermanentWidget(m_currentImageFormatLabel);

    statusBar()->showMessage(tr("Ready"), STATUSBAR_TIMEOUT);
}

void MainWindow::createDockWindows()
{
    QDockWidget *dock = new QDockWidget(tr("Text Editor"), this);
    m_editor = new TextEdit(dock);
    connect(m_editor->document(), SIGNAL(contentsChanged()), this, SLOT(onEditorChanged()));
    dock->setWidget(m_editor);
    dock->setObjectName("text_editor");
    addDockWidget(Qt::RightDockWidgetArea, dock);

    m_showEditorDockAction = dock->toggleViewAction();
    m_showEditorDockAction->setIconVisibleInMenu(false);
    m_showEditorDockAction->setStatusTip(tr("Show or hide the document editor dock"));
    m_showEditorDockAction->setIcon(QIcon::fromTheme("accessories-text-editor"));
    m_showEditorDockAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_0));

    dock = new QDockWidget(tr("Assistant"), this);
    m_assistantToolBox = new QToolBox(dock);
    dock->setWidget(m_assistantToolBox);
    dock->setObjectName("assistant");
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    connect(m_assistantToolBox, SIGNAL(currentChanged(int)),
            this, SLOT(onCurrentAssistantChanged(int)));

    m_showAssistantDockAction = dock->toggleViewAction();
    m_showAssistantDockAction->setIconVisibleInMenu(false);
    m_showAssistantDockAction->setStatusTip(tr("Show or hide the assistant dock"));
#if !defined(Q_WS_WIN) // BUG: icons are not displayed when cross-linking
    m_showAssistantDockAction->setIcon(QIcon(":/assistant.svg"));
#endif
    m_showAssistantDockAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_1));

    dock = new QDockWidget(tr("Assistant Info"), this);
    QWidget* widget = new QWidget(dock);
    m_assistantPreviewNotes = new QLabel(widget);
    m_assistantPreviewNotes->setText(tr("Code:"));
    m_assistantCodePreview = new QTextEdit(widget);
    m_assistantCodePreview->setReadOnly(true);
    QBoxLayout* assistant_info_layout = new QBoxLayout(QBoxLayout::TopToBottom, widget);
    assistant_info_layout->addWidget(m_assistantPreviewNotes);
    assistant_info_layout->addWidget(m_assistantCodePreview);
    widget->setLayout(assistant_info_layout);
    dock->setWidget(widget);
    dock->setObjectName("assistant_info");
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    m_showAssistantInfoDockAction = dock->toggleViewAction();
    m_showAssistantInfoDockAction->setIconVisibleInMenu(false);
    m_showAssistantInfoDockAction->setStatusTip(tr("Show or hide the assistant info dock"));
#if !defined(Q_WS_WIN) // BUG: icons are not displayed when cross-linking
    m_showAssistantInfoDockAction->setIcon(QIcon(":/assistant-info.svg"));
#endif
    m_showAssistantInfoDockAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_2));
}

void MainWindow::enableUndoRedoActions()
{
    QTextDocument *document = m_editor->document();
    m_undoAction->setEnabled(document->isUndoAvailable());
    m_redoAction->setEnabled(document->isRedoAvailable());
}

void MainWindow::addZoomActions(QWidget *widget)
{
    widget->addAction(m_zoomInAction);
    widget->addAction(m_zoomOutAction);
    widget->addAction(m_zoomOriginalAction);
}

void MainWindow::checkPaths()
{
    m_hasValidPaths = QFileInfo(m_javaPath).exists() &&
            QFileInfo(m_plantUmlPath).exists();
}

void MainWindow::reloadAssistantXml(const QString &path)
{
    if (m_assistantXmlPath != path) {
        m_assistantXmlPath = path;

        foreach (QListWidget* widget, m_assistantWidgets) {
            widget->deleteLater();
        }
        m_assistantWidgets.clear();

        if (m_assistantXmlPath.isEmpty()) {
            qDebug() << "No assistant defined.";
        } else {
            AssistantXmlReader reader;
            reader.readFile(m_assistantXmlPath);

            QAction* action_insert = new QAction(this);
            action_insert->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Enter));

            for (int i = 0; i < reader.size(); ++i) {
                const Assistant* assistant = reader.assistant(i);
                QListWidget* view = newAssistantListWidget(ASSISTANT_ICON_SIZE, this);
                for (int j = 0; j < assistant->size(); ++j) {
                    const AssistantItem* assistantItem = assistant->item(j);
                    QListWidgetItem* listWidgetItem =
                            new QListWidgetItem(iconFromSvg(ASSISTANT_ICON_SIZE, assistantItem->icon()),
                                                assistantItem->name(), view);
                    listWidgetItem->setData(ASSISTANT_ITEM_DATA_ROLE, assistantItem->data());
                    listWidgetItem->setData(ASSISTANT_ITEM_NOTES_ROLE, assistantItem->notes());
                }
                m_assistantToolBox->addItem(view, assistant->name());
                connect(view, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
                        this, SLOT(onAssistanItemDoubleClicked(QListWidgetItem*)));
                connect(view, SIGNAL(itemSelectionChanged()),
                        this, SLOT(onAssistantItemSelectionChanged()));

                QAction* action = new QAction(this);
                action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return));
                m_assistantInsertSignalMapper->setMapping(action, view);
                connect(action, SIGNAL(triggered()),
                        m_assistantInsertSignalMapper, SLOT(map()));
                view->addAction(action);

                m_assistantWidgets << view;
            }
        }
    }
}

void MainWindow::insertAssistantCode(const QString &code)
{
    if (code.isEmpty())
        return;

    QTextCursor cursor = m_editor->textCursor();
    if (!cursor.isNull()) {
        cursor.beginEditBlock();
        cursor.removeSelectedText();
        cursor.insertText(code);
        cursor.endEditBlock();
    }
}
