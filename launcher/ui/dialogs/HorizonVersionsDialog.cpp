// SPDX-License-Identifier: GPL-3.0-only
/*
 *  horizon-prism - Prism Launcher fork for Project Horizon
 *  Copyright (C) 2026 horizon-prism contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "HorizonVersionsDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

#include "minecraft/HorizonInstance.h"

namespace {
constexpr int TAG_ROLE = Qt::UserRole;

QString errorText(const QByteArray& stderrText, const QByteArray& stdoutText)
{
    auto text = QString::fromUtf8(stderrText).trimmed();
    if (text.isEmpty())
        text = QString::fromUtf8(stdoutText).trimmed();
    if (text.size() > 2000)
        text = text.right(2000).prepend(QStringLiteral("…\n"));
    return text;
}
}  // namespace

HorizonVersionsDialog::HorizonVersionsDialog(HorizonInstance* instance, QWidget* parent)
    : QDialog(parent), m_instance(instance)
{
    setWindowTitle(tr("Horizon versions"));
    resize(720, 420);

    auto* layout = new QVBoxLayout(this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({ tr("Version"), tr("Date"), tr("State") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_table, 1);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_status);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    layout->addWidget(m_progress);

    auto* buttons = new QHBoxLayout();
    m_refresh = new QPushButton(tr("Refresh"), this);
    m_checkUpdates = new QPushButton(tr("Check for updates"), this);
    m_install = new QPushButton(tr("Install"), this);
    m_remove = new QPushButton(tr("Remove"), this);
    m_use = new QPushButton(tr("Use for this instance"), this);
    buttons->addWidget(m_refresh);
    buttons->addWidget(m_checkUpdates);
    buttons->addStretch(1);
    buttons->addWidget(m_install);
    buttons->addWidget(m_remove);
    buttons->addWidget(m_use);
    layout->addLayout(buttons);

    auto* close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(close);

    connect(m_table, &QTableWidget::itemSelectionChanged, this, &HorizonVersionsDialog::updateButtons);
    connect(m_refresh, &QPushButton::clicked, this, &HorizonVersionsDialog::refresh);
    connect(m_checkUpdates, &QPushButton::clicked, this, &HorizonVersionsDialog::checkForUpdates);
    connect(m_install, &QPushButton::clicked, this, &HorizonVersionsDialog::installSelected);
    connect(m_remove, &QPushButton::clicked, this, &HorizonVersionsDialog::removeSelected);
    connect(m_use, &QPushButton::clicked, this, &HorizonVersionsDialog::useForInstance);

    // Offline first: what is on disk is enough to pick a version.
    m_installed.installed = m_instance->installedVersions();
    populate();

    if (m_instance->launcherCli().isEmpty()) {
        m_status->setText(tr("The Horizon launcher was not found, so installing and removing versions is unavailable. "
                             "Install Horizon, put horizon-launcher in PATH, or set \"launcherPath\" in horizon.json."));
    } else {
        m_status->setText(tr("Versions installed on this computer. Refresh lists the releases Horizon can install."));
    }
    updateButtons();
}

void HorizonVersionsDialog::populate()
{
    m_table->setRowCount(0);

    struct Row {
        QString tag;
        QString date;
        bool installed;
        bool available;
        bool prerelease;
    };
    QList<Row> rows;

    if (m_releases.isEmpty()) {
        for (const auto& tag : m_installed.installed)
            rows << Row{ tag, QString(), true, true, false };
    } else {
        for (const auto& release : m_releases)
            rows << Row{ release.tag, release.date, release.installed, release.availableHere, release.prerelease };
    }

    for (const auto& row : rows) {
        const int index = m_table->rowCount();
        m_table->insertRow(index);

        auto* tagItem = new QTableWidgetItem(row.tag);
        tagItem->setData(TAG_ROLE, row.tag);
        m_table->setItem(index, 0, tagItem);
        m_table->setItem(index, 1, new QTableWidgetItem(row.date));

        QStringList state;
        if (row.installed)
            state << tr("installed");
        else if (!row.available)
            state << tr("no build for this platform");
        else
            state << tr("available");
        if (row.prerelease)
            state << tr("prerelease");
        if (row.tag == m_installed.selected)
            state << tr("Horizon's default");
        if (row.tag == m_instance->clientVersion() || row.tag == m_instance->preparerVersion())
            state << tr("used by this instance");
        m_table->setItem(index, 2, new QTableWidgetItem(state.join(QStringLiteral(", "))));
    }

    m_table->resizeColumnToContents(0);
    m_table->resizeColumnToContents(1);
}

QString HorizonVersionsDialog::selectedTag() const
{
    const auto rows = m_table->selectionModel() ? m_table->selectionModel()->selectedRows() : QModelIndexList();
    if (rows.isEmpty())
        return {};
    const auto* item = m_table->item(rows.first().row(), 0);
    return item ? item->data(TAG_ROLE).toString() : QString();
}

bool HorizonVersionsDialog::isInUse(const QString& tag) const
{
    if (tag.isEmpty())
        return false;
    return tag == m_installed.selected || tag == m_instance->clientVersion() || tag == m_instance->preparerVersion();
}

void HorizonVersionsDialog::updateButtons()
{
    const auto tag = selectedTag();
    const bool hasCli = !m_instance->launcherCli().isEmpty();
    const bool installed = m_installed.installed.contains(tag) || std::any_of(m_releases.cbegin(), m_releases.cend(), [&tag](const HorizonRelease& r) { return r.tag == tag && r.installed; });
    const bool available = m_releases.isEmpty() || std::any_of(m_releases.cbegin(), m_releases.cend(), [&tag](const HorizonRelease& r) { return r.tag == tag && r.availableHere; });

    m_refresh->setEnabled(hasCli && !m_busy);
    m_checkUpdates->setEnabled(hasCli && !m_busy);
    m_install->setEnabled(hasCli && !m_busy && !tag.isEmpty() && !installed && available);
    m_remove->setEnabled(hasCli && !m_busy && !tag.isEmpty() && installed && !isInUse(tag));
    m_use->setEnabled(!m_busy && !tag.isEmpty() && installed);
}

void HorizonVersionsDialog::setBusy(bool busy, const QString& what)
{
    m_busy = busy;
    m_progress->setVisible(busy);
    if (busy)
        m_status->setText(what);
    updateButtons();
    m_table->setEnabled(!busy);
}

void HorizonVersionsDialog::runCli(const QStringList& args,
                                   const QString& what,
                                   const std::function<void(int, const QByteArray&, const QByteArray&)>& done)
{
    const auto cli = m_instance->launcherCli();
    if (cli.isEmpty()) {
        showFailure(what, QByteArrayLiteral("horizon-launcher was not found"));
        return;
    }

    setBusy(true, what);

    auto* process = new QProcess(this);
    process->setProgram(cli);
    process->setArguments(args);
    process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process, &QProcess::errorOccurred, this, [this, process, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        const auto text = tr("horizon-launcher failed to start: %1").arg(process->errorString());
        setBusy(false, QString());
        process->deleteLater();
        done(-1, {}, text.toUtf8());
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process, what, done](int exitCode, QProcess::ExitStatus status) {
                const auto out = process->readAllStandardOutput();
                const auto err = process->readAllStandardError();
                process->deleteLater();
                setBusy(false, QString());
                if (status == QProcess::CrashExit) {
                    done(-1, out, err);
                    return;
                }
                if (exitCode != 0) {
                    showFailure(what, errorText(err, out).toUtf8());
                    done(exitCode, out, err);
                    return;
                }
                done(exitCode, out, err);
            });

    process->start();
}

void HorizonVersionsDialog::showFailure(const QString& what, const QByteArray& stderrText)
{
    m_status->setText(tr("%1 failed.").arg(what));
    QMessageBox::warning(this, tr("Horizon versions"), tr("%1 failed:\n\n%2").arg(what, QString::fromUtf8(stderrText).trimmed()));
}

void HorizonVersionsDialog::refresh()
{
    runCli({ "versions", "--json" }, tr("Listing releases"), [this](int exitCode, const QByteArray& out, const QByteArray&) {
        if (exitCode != 0)
            return;
        QString error;
        const auto releases = HorizonRelease::listFromJson(out, &error);
        if (!error.isEmpty()) {
            showFailure(tr("Reading the release list"), error.toUtf8());
            return;
        }
        m_releases = releases;
        populate();
        updateButtons();
        m_status->setText(tr("%n release(s) listed.", "", m_releases.size()));

        runCli({ "versions", "--installed", "--json" }, tr("Listing installed versions"),
                [this](int exitCode, const QByteArray& out, const QByteArray&) {
                    if (exitCode != 0)
                        return;
                    QString error;
                    const auto installed = HorizonInstalledVersions::fromJson(out, &error);
                    if (!error.isEmpty()) {
                        showFailure(tr("Reading the installed versions"), error.toUtf8());
                        return;
                    }
                    m_installed = installed;
                    populate();
                    updateButtons();
                });
    });
}

void HorizonVersionsDialog::checkForUpdates()
{
    runCli({ "update", "--check", "--json" }, tr("Checking for updates"), [this](int exitCode, const QByteArray& out, const QByteArray&) {
        if (exitCode != 0)
            return;
        QString error;
        const auto updates = HorizonUpdates::fromJson(out, &error);
        if (!error.isEmpty()) {
            showFailure(tr("Checking for updates"), error.toUtf8());
            return;
        }
        m_updates = updates;

        QStringList lines;
        lines << (updates.hasHorizonUpdate() ? tr("Horizon %1 is available.").arg(updates.horizonTag) : tr("Horizon is up to date."));
        if (updates.hasLauncherUpdate())
            lines << tr("A newer Horizon launcher (%1) is available; this is %2. Update it from Horizon itself.")
                             .arg(updates.launcherVersion, updates.launcherCurrent);
        else if (!updates.launcherCurrent.isEmpty())
            lines << tr("The Horizon launcher is up to date (%1).").arg(updates.launcherCurrent);
        m_status->setText(lines.join(' '));
    });
}

void HorizonVersionsDialog::installSelected()
{
    const auto tag = selectedTag();
    if (tag.isEmpty())
        return;

    if (m_releases.isEmpty()) {
        // Without the release list we cannot tell whether a build exists here.
        refresh();
        return;
    }

    runCli({ "install", tag, "--json" }, tr("Installing %1").arg(tag), [this, tag](int exitCode, const QByteArray& out, const QByteArray&) {
        if (exitCode != 0)
            return;
        QString error;
        const auto result = HorizonInstallResult::fromJson(out, &error);
        if (!error.isEmpty()) {
            showFailure(tr("Installing %1").arg(tag), error.toUtf8());
            return;
        }
        m_installed.installed = m_instance->installedVersions();
        if (!m_installed.installed.contains(result.installed))
            m_installed.installed << result.installed;
        populate();
        updateButtons();
        m_status->setText(result.prepared ? tr("Installed %1. It already has prepared resources.").arg(result.installed)
                                          : tr("Installed %1. Prepare it before the first launch.").arg(result.installed));
    });
}

void HorizonVersionsDialog::removeSelected()
{
    const auto tag = selectedTag();
    if (tag.isEmpty() || isInUse(tag))
        return;

    const auto answer = QMessageBox::question(this, tr("Horizon versions"),
                                              tr("Remove Horizon %1 from this computer?\n\n"
                                                 "Its files are deleted. Instances pinned to it will need another version.")
                                                  .arg(tag),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    runCli({ "remove", tag }, tr("Removing %1").arg(tag), [this, tag](int exitCode, const QByteArray&, const QByteArray&) {
        if (exitCode != 0)
            return;
        m_installed.installed.removeAll(tag);
        m_installed.installed = m_instance->installedVersions();
        for (auto& release : m_releases) {
            if (release.tag == tag)
                release.installed = false;
        }
        populate();
        updateButtons();
        m_status->setText(tr("Removed %1.").arg(tag));
    });
}

void HorizonVersionsDialog::useForInstance()
{
    const auto tag = selectedTag();
    if (tag.isEmpty())
        return;
    m_chosen = tag;
    accept();
}
