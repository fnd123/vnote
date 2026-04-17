#include "exportcontroller.h"

#include <algorithm>

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>
#include <exception>

#include <core/exception.h>
#include <core/servicelocator.h>
#include <core/services/filetypecoreservice.h>
#include <core/services/notebookcoreservice.h>
#include <export/exporter.h>
#include <utils/fileutils.h>

using namespace vnotex;

namespace {

struct ExportChildEntry {
  QString m_name;
  bool m_folder = false;
};

QCollator newExportCollator() {
  QCollator collator;
  collator.setNumericMode(true);
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  return collator;
}

QVector<ExportChildEntry> sortedChildEntries(const QJsonObject &p_children) {
  QVector<ExportChildEntry> entries;

  const auto fileArray = p_children.value(QStringLiteral("files")).toArray();
  entries.reserve(fileArray.size());
  for (const auto &fileValue : fileArray) {
    const auto name = fileValue.toObject().value(QStringLiteral("name")).toString();
    if (!name.isEmpty()) {
      entries.append(ExportChildEntry{name, false});
    }
  }

  const auto folderArray = p_children.value(QStringLiteral("folders")).toArray();
  entries.reserve(entries.size() + folderArray.size());
  for (const auto &folderValue : folderArray) {
    const auto name = folderValue.toObject().value(QStringLiteral("name")).toString();
    if (!name.isEmpty()) {
      entries.append(ExportChildEntry{name, true});
    }
  }

  auto collator = newExportCollator();
  std::sort(entries.begin(), entries.end(), [&collator](const ExportChildEntry &p_lhs,
                                                        const ExportChildEntry &p_rhs) {
    const int result = collator.compare(p_lhs.m_name, p_rhs.m_name);
    if (result != 0) {
      return result < 0;
    }

    return p_lhs.m_folder && !p_rhs.m_folder;
  });

  return entries;
}

QList<NodeIdentifier> sortedNodeIds(QList<NodeIdentifier> p_nodeIds) {
  auto collator = newExportCollator();
  std::sort(p_nodeIds.begin(), p_nodeIds.end(), [&collator](const NodeIdentifier &p_lhs,
                                                            const NodeIdentifier &p_rhs) {
    const int pathResult = collator.compare(p_lhs.relativePath, p_rhs.relativePath);
    if (pathResult != 0) {
      return pathResult < 0;
    }

    return collator.compare(p_lhs.notebookId, p_rhs.notebookId) < 0;
  });

  return p_nodeIds;
}

void appendFolderHeading(const QString &p_title, int p_level, QVector<ExportFileInfo> &p_files) {
  const auto title = p_title.trimmed();
  if (title.isEmpty()) {
    return;
  }

  ExportFileInfo info;
  info.isSectionHeading = true;
  info.sectionTitle = title;
  info.sectionLevel = qBound(1, p_level, 6);
  p_files.append(info);
}

QString normalizedAbsolutePath(const QString &p_path) {
  if (p_path.isEmpty()) {
    return QString();
  }

  const QFileInfo info(p_path);
  const auto path = info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
  return QDir::cleanPath(path);
}

bool isSamePath(const QString &p_lhs, const QString &p_rhs) {
  const auto lhs = normalizedAbsolutePath(p_lhs);
  const auto rhs = normalizedAbsolutePath(p_rhs);
  return !lhs.isEmpty() && lhs == rhs;
}

bool isPathUnderDirectory(const QString &p_path, const QString &p_dir) {
  const auto path = normalizedAbsolutePath(p_path);
  const auto dir = normalizedAbsolutePath(p_dir);
  if (path.isEmpty() || dir.isEmpty()) {
    return false;
  }

  return path == dir || path.startsWith(dir + QLatin1Char('/'));
}

QString excludedOutputDirForSource(const QString &p_outputDir, const QString &p_sourceRootDir) {
  if (p_outputDir.isEmpty() || p_sourceRootDir.isEmpty() || isSamePath(p_outputDir, p_sourceRootDir)) {
    return QString();
  }

  return normalizedAbsolutePath(p_outputDir);
}

} // namespace

ExportController::ExportController(ServiceLocator &p_services, QObject *p_parent)
    : ExportController(p_services, nullptr, p_parent) {}

ExportController::ExportController(ServiceLocator &p_services, QWidget *p_widgetParent,
                                   QObject *p_parentObject)
    : QObject(p_parentObject), m_services(p_services), m_widgetParent(p_widgetParent) {}

void ExportController::doExport(const ExportOption &p_option, const ExportContext &p_context) {
  if (m_isExporting) {
    emit logRequested(tr("Export is already in progress."));
    return;
  }

  m_isExporting = true;

  QStringList outputFiles;

  do {
    auto *notebookService = m_services.get<NotebookCoreService>();
    if (!notebookService) {
      emit logRequested(tr("NotebookCoreService not available."));
      break;
    }

    auto *exporter = ensureExporter();
    if (!exporter) {
      emit logRequested(tr("Failed to create exporter."));
      break;
    }

    const bool includeFolderHeadings =
        p_option.m_targetFormat == ExportFormat::PDF && p_option.m_pdfOption.m_allInOne;

    try {
      switch (p_option.m_source) {
      case ExportSource::CurrentBuffer: {
        if (!p_context.currentNodeId.isValid() && p_context.bufferPath.isEmpty()) {
          emit logRequested(tr("No current buffer available for export."));
          break;
        }

        QString relativePath;
        QString filePath;
        QString attachmentFolderPath;
        if (p_context.currentNodeId.isValid()) {
          relativePath = normalizedRelativePath(p_context.currentNodeId.relativePath);
          filePath =
              notebookService->buildAbsolutePath(p_context.currentNodeId.notebookId, relativePath);
          attachmentFolderPath =
              p_option.m_exportAttachments
                  ? notebookService->getAttachmentsFolder(p_context.currentNodeId.notebookId,
                                                          relativePath)
                  : QString();
        } else {
          filePath = p_context.bufferPath;
        }

        if (filePath.isEmpty()) {
          emit logRequested(tr("Failed to resolve current buffer path."));
          break;
        }

        QString fileName = p_context.bufferName;
        if (fileName.isEmpty()) {
          fileName = QFileInfo(filePath).fileName();
        }

        const auto outputFile = exporter->doExportFile(
            p_option, p_context.bufferContent, filePath, fileName,
            QFileInfo(filePath).absolutePath(), attachmentFolderPath,
            isMarkdownFile(filePath));
        if (!outputFile.isEmpty()) {
          outputFiles.append(outputFile);
        }
        break;
      }

      case ExportSource::CurrentNote: {
        if (!p_context.currentNodeId.isValid()) {
          emit logRequested(tr("No current note available for export."));
          break;
        }

        const auto relativePath = normalizedRelativePath(p_context.currentNodeId.relativePath);
        const auto filePath =
            notebookService->buildAbsolutePath(p_context.currentNodeId.notebookId, relativePath);
        if (filePath.isEmpty()) {
          emit logRequested(tr("Failed to resolve current note path."));
          break;
        }

        const auto outputFile = exporter->doExportFile(
            p_option, FileUtils::readTextFile(filePath), filePath, QFileInfo(filePath).fileName(),
            QFileInfo(filePath).absolutePath(),
            p_option.m_exportAttachments ? notebookService->getAttachmentsFolder(
                                               p_context.currentNodeId.notebookId, relativePath)
                                         : QString(),
            isMarkdownFile(filePath));
        if (!outputFile.isEmpty()) {
          outputFiles.append(outputFile);
        }
        break;
      }

      case ExportSource::CurrentFolder: {
        if (!p_context.currentFolderId.isValid()) {
          emit logRequested(tr("No current folder available for export."));
          break;
        }

        const auto relativePath = normalizedRelativePath(p_context.currentFolderId.relativePath);
        QVector<ExportFileInfo> collectedFiles;
        const auto sourceRoot =
            notebookService->buildAbsolutePath(p_context.currentFolderId.notebookId, relativePath);
        const auto excludedOutputDir = excludedOutputDirForSource(p_option.m_outputDir, sourceRoot);
        collectExportFiles(p_context.currentFolderId.notebookId, relativePath, p_option.m_recursive,
                           p_option.m_exportAttachments, includeFolderHeadings, 2,
                           excludedOutputDir, collectedFiles);

        QVector<ExportFileInfo> files;
        if (includeFolderHeadings && !collectedFiles.isEmpty()) {
          appendFolderHeading(folderBatchName(p_context.currentFolderId.notebookId, relativePath),
                              1, files);
        }
        files += collectedFiles;
        outputFiles = exporter->doExportBatch(
            p_option, files, folderBatchName(p_context.currentFolderId.notebookId, relativePath));
        break;
      }

      case ExportSource::SelectedNodes: {
        if (p_context.selectedNodeIds.isEmpty()) {
          emit logRequested(tr("No selected nodes available for export."));
          break;
        }

        QVector<ExportFileInfo> files;
        for (const auto &nodeId : sortedNodeIds(p_context.selectedNodeIds)) {
          if (!nodeId.isValid()) {
            continue;
          }

          const auto relativePath = normalizedRelativePath(nodeId.relativePath);
          const auto filePath = notebookService->buildAbsolutePath(nodeId.notebookId, relativePath);
          if (filePath.isEmpty()) {
            emit logRequested(tr("Failed to resolve file path for (%1).").arg(relativePath));
            continue;
          }

          if (QFileInfo(filePath).isDir()) {
            QVector<ExportFileInfo> collectedFiles;
            const auto excludedOutputDir = excludedOutputDirForSource(p_option.m_outputDir, filePath);
            collectExportFiles(nodeId.notebookId, relativePath, p_option.m_recursive,
                               p_option.m_exportAttachments, includeFolderHeadings, 2,
                               excludedOutputDir, collectedFiles);
            if (includeFolderHeadings && !collectedFiles.isEmpty()) {
              appendFolderHeading(folderBatchName(nodeId.notebookId, relativePath), 1, files);
            }
            files += collectedFiles;
            continue;
          }

          if (includeFolderHeadings && !isMarkdownFile(filePath)) {
            continue;
          }

          ExportFileInfo info;
          info.filePath = filePath;
          info.fileName = QFileInfo(filePath).fileName();
          info.resourcePath = QFileInfo(filePath).absolutePath();
          info.attachmentFolderPath =
              p_option.m_exportAttachments
                  ? notebookService->getAttachmentsFolder(nodeId.notebookId, relativePath)
                  : QString();
          info.isMarkdown = isMarkdownFile(filePath);
          files.append(info);
        }

        outputFiles = exporter->doExportBatch(p_option, files, tr("selected_export"));
        break;
      }

      case ExportSource::CurrentNotebook: {
        QString notebookId = p_context.notebookId;
        if (notebookId.isEmpty()) {
          notebookId = p_context.currentNodeId.notebookId;
        }

        if (notebookId.isEmpty()) {
          emit logRequested(tr("No current notebook available for export."));
          break;
        }

        QVector<ExportFileInfo> collectedFiles;
        const auto sourceRoot = notebookService->buildAbsolutePath(notebookId, QString());
        const auto excludedOutputDir = excludedOutputDirForSource(p_option.m_outputDir, sourceRoot);
        collectExportFiles(notebookId, QStringLiteral("."), p_option.m_recursive,
                           p_option.m_exportAttachments, includeFolderHeadings, 2,
                           excludedOutputDir, collectedFiles);

        QVector<ExportFileInfo> files;
        if (includeFolderHeadings && !collectedFiles.isEmpty()) {
          appendFolderHeading(notebookBatchName(notebookId), 1, files);
        }
        files += collectedFiles;
        outputFiles = exporter->doExportBatch(p_option, files, notebookBatchName(notebookId));
        break;
      }

      default:
        emit logRequested(tr("Unsupported export source."));
        break;
      }
    } catch (const Exception &p_e) {
      emit logRequested(QString::fromUtf8(p_e.what()));
    } catch (const std::exception &p_e) {
      emit logRequested(QString::fromUtf8(p_e.what()));
    }
  } while (false);

  m_isExporting = false;
  emit exportFinished(outputFiles);
}

void ExportController::stop() {
  if (m_exporter) {
    m_exporter->stop();
  }
}

bool ExportController::isExporting() const { return m_isExporting; }

Exporter *ExportController::ensureExporter() {
  if (!m_exporter) {
    m_exporter = new Exporter(m_services, m_widgetParent.data());
    connect(m_exporter, &Exporter::progressUpdated, this, &ExportController::progressUpdated);
    connect(m_exporter, &Exporter::logRequested, this, &ExportController::logRequested);
  }

  return m_exporter;
}

void ExportController::collectExportFiles(const QString &p_notebookId, const QString &p_folderPath,
                                          bool p_recursive, bool p_exportAttachments,
                                          bool p_includeFolderHeadings, int p_folderHeadingLevel,
                                          const QString &p_excludedOutputDir,
                                          QVector<ExportFileInfo> &p_files) {
  auto *notebookService = m_services.get<NotebookCoreService>();
  if (!notebookService) {
    emit logRequested(tr("NotebookCoreService not available."));
    return;
  }

  const auto folderPath = normalizedRelativePath(p_folderPath);
  const auto children = notebookService->listFolderChildren(p_notebookId, folderPath);

  const auto entries = sortedChildEntries(children);
  for (const auto &entry : entries) {
    const auto relativePath =
        folderPath.isEmpty() ? entry.m_name : folderPath + QLatin1Char('/') + entry.m_name;
    const auto path = notebookService->buildAbsolutePath(p_notebookId, relativePath);
    if (isPathUnderDirectory(path, p_excludedOutputDir)) {
      continue;
    }

    if (entry.m_folder) {
      if (!p_recursive) {
        continue;
      }

      if (p_includeFolderHeadings) {
        QVector<ExportFileInfo> childFiles;
        collectExportFiles(p_notebookId, relativePath, p_recursive, p_exportAttachments,
                           p_includeFolderHeadings, p_folderHeadingLevel + 1,
                           p_excludedOutputDir, childFiles);
        if (!childFiles.isEmpty()) {
          appendFolderHeading(entry.m_name, p_folderHeadingLevel, p_files);
          p_files += childFiles;
        }
      } else {
        collectExportFiles(p_notebookId, relativePath, p_recursive, p_exportAttachments,
                           p_includeFolderHeadings, p_folderHeadingLevel + 1,
                           p_excludedOutputDir, p_files);
      }
      continue;
    }

    if (path.isEmpty()) {
      emit logRequested(tr("Failed to resolve file path for (%1).").arg(relativePath));
      continue;
    }

    const bool isMarkdown = isMarkdownFile(path);
    if (p_includeFolderHeadings && !isMarkdown) {
      continue;
    }

    ExportFileInfo info;
    info.filePath = path;
    info.fileName = entry.m_name;
    info.resourcePath = QFileInfo(path).absolutePath();
    info.attachmentFolderPath =
        p_exportAttachments ? notebookService->getAttachmentsFolder(p_notebookId, relativePath)
                            : QString();
    info.isMarkdown = isMarkdown;
    p_files.append(info);
  }
}

bool ExportController::isMarkdownFile(const QString &p_filePath) const {
  auto *fileTypeService = m_services.get<FileTypeCoreService>();
  if (!fileTypeService) {
    return false;
  }

  const auto suffix = QFileInfo(p_filePath).suffix().toLower();
  return fileTypeService->getFileTypeBySuffix(suffix).isMarkdown();
}

QString ExportController::normalizedRelativePath(const QString &p_relativePath) const {
  return p_relativePath == QStringLiteral(".") ? QString() : p_relativePath;
}

QString ExportController::notebookBatchName(const QString &p_notebookId) const {
  auto *notebookService = m_services.get<NotebookCoreService>();
  if (!notebookService) {
    return p_notebookId;
  }

  const auto config = notebookService->getNotebookConfig(p_notebookId);
  const auto rootFolder = config.value(QStringLiteral("rootFolder")).toString();
  const auto name = QFileInfo(rootFolder).fileName();
  return name.isEmpty() ? p_notebookId : name;
}

QString ExportController::folderBatchName(const QString &p_notebookId,
                                          const QString &p_folderPath) const {
  const auto folderPath = normalizedRelativePath(p_folderPath);
  if (folderPath.isEmpty()) {
    return notebookBatchName(p_notebookId);
  }

  const auto name = QFileInfo(folderPath).fileName();
  return name.isEmpty() ? notebookBatchName(p_notebookId) : name;
}
