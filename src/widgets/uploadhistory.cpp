#include "uploadhistory.h"
#include "./ui_uploadhistory.h"
#include "src/tools/imgupload/imguploadermanager.h"
#include "src/utils/confighandler.h"
#include "src/utils/history.h"
#include "uploadlineitem.h"

#include <QDateTime>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QPixmap>
const int IMAGES_BATCH_SIZE = 1;
void scaleThumbnail(QPixmap& pixmap)
{
    if (pixmap.height() / HISTORYPIXMAP_MAX_PREVIEW_HEIGHT >=
        pixmap.width() / HISTORYPIXMAP_MAX_PREVIEW_WIDTH) {
        pixmap = pixmap.scaledToHeight(HISTORYPIXMAP_MAX_PREVIEW_HEIGHT,
                                       Qt::SmoothTransformation);
    } else {
        pixmap = pixmap.scaledToWidth(HISTORYPIXMAP_MAX_PREVIEW_WIDTH,
                                      Qt::SmoothTransformation);
    }
}

void clearHistoryLayout(QLayout* layout)
{
    while (layout->count() != 0) {
        delete layout->takeAt(0);
    }
}

UploadHistory::UploadHistory(QWidget* parent)
  : QWidget(parent)
  , ui(new Ui::UploadHistory)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(QDesktopWidget().availableGeometry(this).size() * 0.5);
}
void UploadHistory::loadHistory()
{
    clearHistoryLayout(ui->historyContainer);

    History history = History();
    QList<QString> allHistoryFiles = history.history(); // Fetch all files

    // Respect the "Latest Uploads Max Size" limit
    int maxSize = ConfigHandler().uploadHistoryMax();
    if (allHistoryFiles.size() > maxSize) {
        // Move excess items to secondary storage
        int excessCount = allHistoryFiles.size() - maxSize;
        secondaryStorage.append(allHistoryFiles.mid(0, excessCount));
        historyFiles = allHistoryFiles.mid(excessCount);
    } else {
        historyFiles = allHistoryFiles;
    }

    currentBatchStartIndex = 0;

    if (historyFiles.isEmpty()) {
        setEmptyMessage();
    } else {
        loadNextBatch();
    }

    // Add "Load More" button
    auto* loadMoreButton = new QPushButton(tr("Load More"), this);
    connect(loadMoreButton, &QPushButton::clicked, this, [=]() {
        loadNextBatch(); // Fetch more items
    });
    ui->historyContainer->addWidget(loadMoreButton);
}
void UploadHistory::loadNextBatch()
{
    int batchEndIndex = std::min(currentBatchStartIndex + IMAGES_BATCH_SIZE,
                                 historyFiles.size());

    for (int i = currentBatchStartIndex; i < batchEndIndex; ++i) {
        addLine(history.path(), historyFiles[i]);
    }

    currentBatchStartIndex = batchEndIndex;

    // If current history is exhausted, load from secondary storage
    if (currentBatchStartIndex >= historyFiles.size() && !secondaryStorage.isEmpty()) {
        // Move items from secondary storage to current history
        int itemsToMove = std::min(IMAGES_BATCH_SIZE, secondaryStorage.size());
        historyFiles.append(secondaryStorage.mid(0, itemsToMove));
        secondaryStorage = secondaryStorage.mid(itemsToMove);

        // Reset the batch index to continue loading
        currentBatchStartIndex = historyFiles.size() - itemsToMove;
    }

    // Hide "Load More" button if everything is loaded
    if (currentBatchStartIndex >= historyFiles.size() && secondaryStorage.isEmpty()) {
        QPushButton* loadMoreButton = findChild<QPushButton*>("loadMoreButton");
        if (loadMoreButton) {
            loadMoreButton->hide();
        }
    }
}
void UploadHistory::setEmptyMessage()
{
    auto* buttonEmpty = new QPushButton;
    buttonEmpty->setText(tr("Screenshots history is empty"));
    buttonEmpty->setMinimumSize(1, HISTORYPIXMAP_MAX_PREVIEW_HEIGHT);
    connect(buttonEmpty, &QPushButton::clicked, this, [=]() { this->close(); });
    ui->historyContainer->addWidget(buttonEmpty);
}

void UploadHistory::addLine(const QString& path, const QString& fileName)
{
    QString fullFileName = path + fileName;

    History history;
    HistoryFileName unpackFileName = history.unpackFileName(fileName);

    QString url = ImgUploaderManager(this).url() + unpackFileName.file;

    // load pixmap
    QPixmap pixmap;
    pixmap.load(fullFileName, "png");
    scaleThumbnail(pixmap);

    // get file info
    auto fileInfo = QFileInfo(fullFileName);
    QString lastModified =
      fileInfo.lastModified().toString("yyyy-MM-dd\nhh:mm:ss");

    auto* line = new UploadLineItem(
      this, pixmap, lastModified, url, fullFileName, unpackFileName);

    connect(line, &UploadLineItem::requestedDeletion, this, [=]() {
        if (ui->historyContainer->count() <= 1) {
            setEmptyMessage();
        }
        delete line;
    });

    ui->historyContainer->addWidget(line);
}

UploadHistory::~UploadHistory()
{
    delete ui;
}
