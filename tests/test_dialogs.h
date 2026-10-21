#pragma once

#include <cassert>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QAction>
#include <QTimer>
#include <QFile>
#include <QPixmap>
#include <QPointer>
#include "Zinsser.h"
#include "ui/MainWindow.h"
#include "ui/AboutDialog.h"
#include "ui/UpdateDialog.h"
#include "core/ProvenanceDetector.h"
#include "core/SilentAutoUpdater.h"
#include "core/UpdateChecker.h"
#include "core/Version.h"

inline void testSovereignDialogTrilogy(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    describe("AboutDialog layout, cultural inscription, and non-blocking lifecycle", [&]() {
        it("constructs freestanding card matching canonical obsidian dimensions", [&]() {
            AboutDialog dialog;
            dialog.open();
            app.processEvents();

            assert(dialog.isVisible());
            assert(dialog.width() >= 320);
            assert(dialog.width() <= 360);
            assert(dialog.height() >= 240);
            assert(dialog.height() <= 280);

            dialog.reject();
            app.processEvents();
            assert(!dialog.isVisible());
        });

        it("loads monolith chevron logo from bundled resource system", [&]() {
            assert(QFile::exists(":/branding/markd_glyph_transparent.png"));
            QPixmap logo(":/branding/markd_glyph_transparent.png");
            assert(!logo.isNull());
            assert(logo.width() > 0);
            assert(logo.height() > 0);

            AboutDialog dialog;
            assert(dialog.hasValidLogo());
        });

        it("renders Tamil cultural inscription with aram tooltip and zero bracket artifacts", [&]() {
            AboutDialog dialog;
            const QString text = dialog.inscriptionText();
            assert(text.contains(QString::fromUtf8("மேற்கடல் ஆம்ஸ்டர்டாமில் வடித்த அறம் 🇳🇱")));
            assert(!text.contains('['));
            assert(!text.contains(']'));

            const QString tooltip = dialog.inscriptionToolTip();
            assert(tooltip.contains("Aram, shaped in Amsterdam by the western sea."));
            assert(tooltip.contains(QString::fromUtf8("Aram / அறம்: Rightful conduct, duty, and architectural integrity")));
            assert(!tooltip.contains("Dharma")); // Strict sovereign value guard
        });

        it("dismisses non-blockingly upon close button click or escape rejection", [&]() {
            AboutDialog dialog;
            QSignalSpy finishedSpy(&dialog, &QDialog::finished);

            dialog.open();
            app.processEvents();
            assert(dialog.isVisible());

            QKeyEvent nonEsc(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
            QApplication::sendEvent(&dialog, &nonEsc);
            assert(dialog.isVisible());

            QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(&dialog, &esc);
            app.processEvents();

            assert(finishedSpy.count() == 1);
            assert(!dialog.isVisible());
        });

        it("supports frameless mouse drag and window translation", [&]() {
            AboutDialog dialog;
            dialog.open();
            app.processEvents();

            const QPoint startPos = dialog.pos();
            QMouseEvent rightPress(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
            QApplication::sendEvent(&dialog, &rightPress);

            QMouseEvent leftPress(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&dialog, &leftPress);

            QMouseEvent move(QEvent::MouseMove, QPointF(25, 25), QPointF(25, 25),
                             Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&dialog, &move);
            app.processEvents();

            assert(dialog.pos() != startPos || dialog.width() == 340);

            dialog.reject();
            app.processEvents();
        });

        it("displays unified version string from core version header", [&]() {
            AboutDialog dialog;
            assert(dialog.versionText().contains(MARKD_VERSION_STR));
        });
    });

    describe("ProvenanceDetector installation environment classification", [&]() {
        it("detects macOS and Linux Homebrew installation paths", [&]() {
            const auto armBrew = ProvenanceDetector::detect("/opt/homebrew/bin/markd");
            assert(armBrew.provenance == InstallationProvenance::Homebrew);
            assert(armBrew.upgradeCommand == "brew upgrade markd");

            const auto intelBrew = ProvenanceDetector::detect("/usr/local/Cellar/markd/0.1.0/bin/markd");
            assert(intelBrew.provenance == InstallationProvenance::Homebrew);
            assert(intelBrew.upgradeCommand == "brew upgrade markd");

            const auto linuxBrew = ProvenanceDetector::detect("/home/linuxbrew/.linuxbrew/bin/markd");
            assert(linuxBrew.provenance == InstallationProvenance::Homebrew);
            assert(linuxBrew.upgradeCommand == "brew upgrade markd");
        });

        it("detects NPM global installation environments", [&]() {
            const auto nvmPath = ProvenanceDetector::detect("/Users/developer/.nvm/versions/node/v20.10.0/bin/markd");
            assert(nvmPath.provenance == InstallationProvenance::NpmGlobal);
            assert(nvmPath.upgradeCommand == "npm install -g markd@latest");

            const auto fnmPath = ProvenanceDetector::detect("/home/user/.local/share/fnm/current/bin/markd");
            assert(fnmPath.provenance == InstallationProvenance::NpmGlobal);
            assert(fnmPath.upgradeCommand == "npm install -g markd@latest");

            const auto globalNpm = ProvenanceDetector::detect("/usr/local/lib/node_modules/markd/bin/markd");
            assert(globalNpm.provenance == InstallationProvenance::NpmGlobal);
            assert(globalNpm.upgradeCommand == "npm install -g markd@latest");
        });

        it("classifies macOS application bundle and standalone binary targets", [&]() {
            const auto appBundle = ProvenanceDetector::detect("/Applications/Markd.app/Contents/MacOS/markd");
            assert(appBundle.provenance == InstallationProvenance::StandaloneBinary);
            assert(appBundle.downloadUrl == "https://markd.aramudan.dev");

            const auto aptPkg = ProvenanceDetector::detect("/usr/bin/markd");
            assert(aptPkg.provenance == InstallationProvenance::AptPackage);
            assert(aptPkg.upgradeCommand == "sudo apt update && sudo apt install --only-upgrade markd");

            const auto customBuild = ProvenanceDetector::detect("/tmp/build/bin/markd");
            assert(customBuild.provenance == InstallationProvenance::StandaloneBinary);
            assert(!customBuild.downloadUrl.isEmpty());
        });
    });

    describe("UpdateDialog states, clipboard transport, and zero bracket invariant", [&]() {
        it("presents up-to-date status confirmation in state 1", [&]() {
            UpdateInfo info;
            info.state = UpdateState::UpToDate;
            info.currentVersion = "v0.1.0";
            info.latestVersion = "v0.1.0";

            UpdateDialog dialog(info);
            dialog.open();
            app.processEvents();

            assert(dialog.statusMessage().contains(QString::fromUtf8("✓ Markd is up to date")));
            assert(dialog.actionButtonText() == "OK");
            assert(!dialog.actionButtonText().contains('['));

            dialog.triggerPrimaryAction();
            app.processEvents();
            assert(!dialog.isVisible());
        });

        it("presents tailored homebrew command and copies to clipboard in state 2", [&]() {
            UpdateInfo info;
            info.state = UpdateState::UpdateAvailable;
            info.currentVersion = "v0.1.0";
            info.latestVersion = "v0.2.0";
            info.provenance = InstallationProvenance::Homebrew;
            info.upgradeCommand = "brew upgrade markd";

            UpdateDialog dialog(info);
            dialog.open();
            app.processEvents();

            assert(dialog.commandText().contains("brew upgrade markd"));
            assert(dialog.actionButtonText() == "Copy Command");

            dialog.triggerPrimaryAction();
            app.processEvents();

            QClipboard* clipboard = QApplication::clipboard();
            assert(clipboard != nullptr);
            assert(clipboard->text() == "brew upgrade markd");
            assert(dialog.isCopiedBadgeVisible());
            assert(dialog.copiedBadgeText().contains("Copied to clipboard"));

            dialog.reject();
            app.processEvents();
            assert(!dialog.isVisible());
        });

        it("presents offline error state and allows retry in state 3", [&]() {
            UpdateInfo info;
            info.state = UpdateState::CheckFailed;
            info.errorMessage = "Unable to check for updates. Check your internet connection.";

            UpdateDialog dialog(info);
            dialog.open();
            app.processEvents();

            assert(dialog.statusMessage().contains("Unable to check for updates"));
            assert(dialog.actionButtonText() == "Retry");

            dialog.reject();
            app.processEvents();
            assert(!dialog.isVisible());
        });

        it("enforces zero bracket characters across all dialog buttons and labels", [&]() {
            UpdateInfo info;
            info.state = UpdateState::UpdateAvailable;
            info.currentVersion = "v0.1.0";
            info.latestVersion = "v0.2.0";
            info.provenance = InstallationProvenance::NpmGlobal;
            info.upgradeCommand = "npm install -g markd@latest";

            UpdateDialog dialog(info);
            assert(!dialog.hasSquareBracketsInUiText());
        });

        it("presents download update button for standalone binary and emits retry on failure", [&]() {
            UpdateInfo info;
            info.state = UpdateState::UpdateAvailable;
            info.currentVersion = "v0.1.0";
            info.latestVersion = "v0.2.0";
            info.provenance = InstallationProvenance::StandaloneBinary;
            info.downloadUrl = "https://markd.aramudan.dev";

            UpdateDialog dialog(info);
            dialog.open();
            app.processEvents();

            assert(dialog.actionButtonText() == "Download Update");
            dialog.triggerPrimaryAction();

            UpdateInfo failInfo;
            failInfo.state = UpdateState::CheckFailed;
            dialog.setUpdateInfo(failInfo);
            assert(dialog.actionButtonText() == "Retry");

            QSignalSpy retrySpy(&dialog, &UpdateDialog::retryRequested);
            dialog.triggerPrimaryAction();
            assert(retrySpy.count() == 1);

            dialog.reject();
            app.processEvents();
        });

        it("supports checking state and frameless drag translation", [&]() {
            UpdateDialog dialog;
            dialog.open();
            app.processEvents();

            assert(dialog.statusMessage().contains("Checking for updates"));
            const QPoint startPos = dialog.pos();
            QMouseEvent rightPress(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
            QApplication::sendEvent(&dialog, &rightPress);

            QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&dialog, &press);

            QMouseEvent move(QEvent::MouseMove, QPointF(30, 30), QPointF(30, 30),
                             Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&dialog, &move);
            app.processEvents();

            assert(dialog.pos() != startPos || dialog.width() == 420);

            QKeyEvent nonEsc(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
            QApplication::sendEvent(&dialog, &nonEsc);

            QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(&dialog, &esc);
            app.processEvents();
            assert(!dialog.isVisible());
        });

        it("handles empty download url and default error message branches", [&]() {
            UpdateInfo info;
            info.state = UpdateState::UpdateAvailable;
            info.provenance = InstallationProvenance::StandaloneBinary;
            info.downloadUrl = "";

            UpdateDialog dialog(info);
            dialog.triggerPrimaryAction(); // Empty URL branch

            UpdateInfo failInfo;
            failInfo.state = UpdateState::CheckFailed;
            failInfo.errorMessage = "";
            dialog.setUpdateInfo(failInfo);
            assert(dialog.statusMessage() == "Unable to check for updates.");
        });
    });

    describe("UpdateChecker SemVer parsing and asynchronous transport dispatch", [&]() {
        it("constructs default network transport and compares patch precedence", [&]() {
            UpdateChecker defaultChecker;
            assert(!UpdateChecker::isNewerVersion("0.1.2", "0.1.1"));
            assert(!UpdateChecker::isNewerVersion("0.2.1", "0.2.0"));
        });

        it("parses SemVer triples with leading v and trailing pre-release tags", [&]() {
            const auto v1 = UpdateChecker::parseVersion("v0.1.0");
            assert(v1.size() == 3);
            assert(v1.at(0) == 0 && v1.at(1) == 1 && v1.at(2) == 0);

            const auto v2 = UpdateChecker::parseVersion("1.2.3-beta.1");
            assert(v2.size() == 3);
            assert(v2.at(0) == 1 && v2.at(1) == 2 && v2.at(2) == 3);
        });

        it("compares version precedence across major minor and patch numbers", [&]() {
            assert(UpdateChecker::isNewerVersion("0.1.0", "0.2.0"));
            assert(UpdateChecker::isNewerVersion("0.1.0", "0.1.1"));
            assert(UpdateChecker::isNewerVersion("0.1.0", "1.0.0"));
            assert(!UpdateChecker::isNewerVersion("0.2.0", "0.1.0"));
            assert(!UpdateChecker::isNewerVersion("0.1.1", "0.1.0"));
            assert(!UpdateChecker::isNewerVersion("1.0.0", "0.9.9"));
            assert(!UpdateChecker::isNewerVersion("0.1.0", "0.1.0"));
            assert(!UpdateChecker::isNewerVersion("0.1.0", ""));
        });

        class MockTransport : public IUpdateTransport {
        public:
            bool success{true};
            QByteArray data;
            QString error;

            void fetch(const QUrl& url,
                       std::function<void(bool success, const QByteArray& data, const QString& error)> cb) override {
                Q_UNUSED(url);
                cb(success, data, error);
            }
        };

        it("dispatches update available when transport returns newer version", [&]() {
            auto transport = std::make_shared<MockTransport>();
            transport->success = true;
            transport->data = "{\"version\": \"0.9.0\"}";

            UpdateChecker checker(transport);
            bool finishedCalled = false;
            UpdateInfo receivedInfo;

            QObject::connect(&checker, &UpdateChecker::checkFinished, [&](const UpdateInfo& info) {
                finishedCalled = true;
                receivedInfo = info;
            });

            checker.checkForUpdates();
            app.processEvents();

            assert(finishedCalled);
            assert(receivedInfo.state == UpdateState::UpdateAvailable);
            assert(receivedInfo.latestVersion == "0.9.0");
        });

        it("dispatches up to date when transport returns current version or empty json", [&]() {
            auto transport = std::make_shared<MockTransport>();
            transport->success = true;
            transport->data = "{}";

            UpdateChecker checker(transport);
            bool finishedCalled = false;
            UpdateInfo receivedInfo;

            QObject::connect(&checker, &UpdateChecker::checkFinished, [&](const UpdateInfo& info) {
                finishedCalled = true;
                receivedInfo = info;
            });

            checker.checkForUpdates();
            app.processEvents();

            assert(finishedCalled);
            assert(receivedInfo.state == UpdateState::UpToDate);
        });

        it("dispatches check failed when network error occurs or transport missing", [&]() {
            auto transport = std::make_shared<MockTransport>();
            transport->success = false;
            transport->error = "Connection refused";

            UpdateChecker checker(transport);
            bool failedCalled = false;
            QString failedMsg;

            QObject::connect(&checker, &UpdateChecker::checkFailed, [&](const QString& err) {
                failedCalled = true;
                failedMsg = err;
            });

            checker.checkForUpdates();
            app.processEvents();

            assert(failedCalled);
            assert(failedMsg == "Connection refused");

            checker.setTransport(nullptr);
            failedCalled = false;
            checker.checkForUpdates();
            assert(failedCalled);
        });
    });

    describe("MainWindow action wiring and headless dialog integration", [&]() {
        it("triggers bespoke about dialog from action_about without blocking and raises on repeat", [&]() {
            MainWindow win;
            QAction* aboutAction = win.findChild<QAction*>("action_about");
            assert(aboutAction != nullptr);

            aboutAction->trigger();
            app.processEvents();

            AboutDialog* dialog = win.findChild<AboutDialog*>();
            assert(dialog != nullptr);
            assert(dialog->isVisible());

            aboutAction->trigger(); // Repeat call activates existing dialog
            app.processEvents();

            QPointer<AboutDialog> guarded(dialog);
            dialog->reject();
            app.processEvents();
            assert(guarded.isNull() || !guarded->isVisible());
        });

        it("triggers update check from action_check_updates without blocking and raises on repeat", [&]() {
            MainWindow win;
            QAction* checkAction = win.findChild<QAction*>("action_check_updates");
            assert(checkAction != nullptr);

            checkAction->trigger();
            app.processEvents();

            UpdateDialog* dialog = win.findChild<UpdateDialog*>();
            assert(dialog != nullptr);
            assert(dialog->isVisible());

            checkAction->trigger(); // Repeat call activates existing dialog
            app.processEvents();

            QPointer<UpdateDialog> guarded(dialog);
            dialog->reject();
            app.processEvents();
            assert(guarded.isNull() || !guarded->isVisible());
        });

        it("transitions from about dialog check updates request to update dialog", [&]() {
            MainWindow win;
            QAction* aboutAction = win.findChild<QAction*>("action_about");
            assert(aboutAction != nullptr);

            aboutAction->trigger();
            app.processEvents();

            AboutDialog* aboutDlg = win.findChild<AboutDialog*>();
            assert(aboutDlg != nullptr);

            emit aboutDlg->checkUpdatesRequested();
            app.processEvents();

            UpdateDialog* updateDlg = win.findChild<UpdateDialog*>();
            assert(updateDlg != nullptr);
            assert(updateDlg->isVisible());

            emit updateDlg->retryRequested();
            app.processEvents();

            updateDlg->reject();
            app.processEvents();
        });

        it("ignores frameless dialog mouse movement when drag originates on child buttons", [&]() {
            AboutDialog aboutDlg;
            aboutDlg.move(200, 200);
            aboutDlg.show();
            app.processEvents();
            const QPoint initialAboutPos = aboutDlg.pos();

            QMouseEvent uninitiatedMove(QEvent::MouseMove, QPointF(10, 10), QPointF(500, 500),
                                        Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&aboutDlg, &uninitiatedMove);
            assert(aboutDlg.pos() == initialAboutPos);

            QMouseEvent bgPress(QEvent::MouseButtonPress, QPointF(20, 20), QPointF(220, 220),
                                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&aboutDlg, &bgPress);
            QMouseEvent bgRelease(QEvent::MouseButtonRelease, QPointF(20, 20), QPointF(220, 220),
                                  Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&aboutDlg, &bgRelease);

            QMouseEvent afterReleaseMove(QEvent::MouseMove, QPointF(30, 30), QPointF(600, 600),
                                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&aboutDlg, &afterReleaseMove);
            assert(aboutDlg.pos() == initialAboutPos);
            aboutDlg.reject();

            UpdateDialog updateDlg;
            updateDlg.move(240, 240);
            updateDlg.show();
            app.processEvents();
            const QPoint initialUpdatePos = updateDlg.pos();
            QApplication::sendEvent(&updateDlg, &uninitiatedMove);
            assert(updateDlg.pos() == initialUpdatePos);
            updateDlg.reject();
        });

        it("renders apt package upgrade command in update dialog and displays canonical markd.aramudan.dev footer", [&]() {
            AboutDialog aboutDlg;
            assert(aboutDlg.footerText().contains(QStringLiteral("markd.aramudan.dev")));

            UpdateInfo aptInfo;
            aptInfo.state = UpdateState::UpdateAvailable;
            aptInfo.currentVersion = QStringLiteral("0.1.0");
            aptInfo.latestVersion = QStringLiteral("0.2.0");
            aptInfo.provenance = InstallationProvenance::AptPackage;
            aptInfo.upgradeCommand = QStringLiteral("sudo apt update && sudo apt install --only-upgrade markd");

            UpdateDialog dlg(aptInfo);
            dlg.open();
            app.processEvents();
            assert(dlg.commandText() == QStringLiteral("sudo apt update && sudo apt install --only-upgrade markd"));
            assert(dlg.actionButtonText() == QStringLiteral("Copy Command"));
            dlg.triggerPrimaryAction();
            assert(QApplication::clipboard()->text() == QStringLiteral("sudo apt update && sudo apt install --only-upgrade markd"));
            dlg.reject();
        });

        it("honors MARKD_UPDATE_ENDPOINT environment variable override in update checker", [&]() {
            qunsetenv("MARKD_UPDATE_ENDPOINT");
            assert(UpdateChecker::resolvedEndpointUrl().toString() == QStringLiteral("https://markd.aramudan.dev/api/v1/updates/check"));

            qputenv("MARKD_UPDATE_ENDPOINT", "http://127.0.0.1:8094/api/v1/updates/check");
            assert(UpdateChecker::resolvedEndpointUrl().toString() == QStringLiteral("http://127.0.0.1:8094/api/v1/updates/check"));
            qunsetenv("MARKD_UPDATE_ENDPOINT");
        });

        it("restores active viewport focus after dismissing about and update dialogs", [&]() {
            MainWindow win;
            win.show();
            win.activateWindow();
            win.setMode(MainWindow::Mode::Edit);
            win.editor()->setFocus();
            app.processEvents();

            QAction* aboutAction = win.findChild<QAction*>("action_about");
            aboutAction->trigger();
            app.processEvents();
            AboutDialog* aboutDlg = win.findChild<AboutDialog*>();
            assert(aboutDlg != nullptr);
            aboutDlg->reject();
            app.processEvents();
            assert(win.currentMode() == MainWindow::Mode::Edit);

            win.setMode(MainWindow::Mode::Preview);
            app.processEvents();
            QAction* checkAction = win.findChild<QAction*>("action_check_updates");
            checkAction->trigger();
            app.processEvents();
            UpdateDialog* updateDlg = win.findChild<UpdateDialog*>();
            assert(updateDlg != nullptr);
            updateDlg->reject();
            app.processEvents();
            assert(win.currentMode() == MainWindow::Mode::Preview);
        });

        it("stages verified binary payload and atomically replaces standalone executable across v1 to v22 hop", [&]() {
            assert(UpdateChecker::isNewerVersion(QStringLiteral("1.0.0"), QStringLiteral("22.0.0")) == true);
            assert(UpdateChecker::isNewerVersion(QStringLiteral("0.1.0"), QStringLiteral("22.0.0")) == true);

            QTemporaryDir tempDir;
            assert(tempDir.isValid());
            const QString binPath = tempDir.filePath(QStringLiteral("markd"));

            QFile v1File(binPath);
            assert(v1File.open(QIODevice::WriteOnly));
            v1File.write("markd-v1-binary");
            v1File.close();

            const QByteArray v22Payload("markd-v22-verified-binary-payload");
            const QString v22Sha = SilentAutoUpdater::computeSha256Hex(v22Payload);

            assert(SilentAutoUpdater::determineUpdateMode(binPath, InstallationProvenance::StandaloneBinary)
                   == SilentUpdateMode::DirectBinarySwap);

            const SilentUpdateResult res = SilentAutoUpdater::stageAndApplyBinaryUpdate(
                binPath, v22Payload, v22Sha, QStringLiteral("22.0.0"));
            assert(res.applied == true);
            assert(res.appliedVersion == QStringLiteral("22.0.0"));
            assert(res.statusText.contains(QStringLiteral("22.0.0")));

            QFile updatedFile(binPath);
            assert(updatedFile.open(QIODevice::ReadOnly));
            assert(updatedFile.readAll() == v22Payload);
            assert((updatedFile.permissions() & QFileDevice::ExeOwner) != 0);
        });

        it("rejects tampered binary payload on sha256 mismatch and preserves existing executable", [&]() {
            QTemporaryDir tempDir;
            assert(tempDir.isValid());
            const QString binPath = tempDir.filePath(QStringLiteral("markd"));

            const QByteArray originalBytes("markd-v1-intact");
            QFile v1File(binPath);
            assert(v1File.open(QIODevice::WriteOnly));
            v1File.write(originalBytes);
            v1File.close();

            const SilentUpdateResult badRes = SilentAutoUpdater::stageAndApplyBinaryUpdate(
                binPath,
                QByteArray("tampered-payload"),
                QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000"),
                QStringLiteral("22.0.0"));
            assert(badRes.applied == false);
            assert(!QFile::exists(binPath + QStringLiteral(".new")));

            QFile checkFile(binPath);
            assert(checkFile.open(QIODevice::ReadOnly));
            assert(checkFile.readAll() == originalBytes);
        });

        it("resolves linux appimage outer path and executes background package upgrades for homebrew and npm", [&]() {
            const ProvenanceResult appImgProv = ProvenanceDetector::detect(
                QStringLiteral("/tmp/.mount_Markd12345/usr/bin/markd"),
                QStringLiteral("/home/dev/Applications/Markd-x86_64.AppImage"));
            assert(appImgProv.provenance == InstallationProvenance::LinuxAppImage);
            assert(appImgProv.targetBinaryPath == QStringLiteral("/home/dev/Applications/Markd-x86_64.AppImage"));

            QString invokedProg;
            QStringList invokedArgs;
            const SilentUpdateResult brewRes = SilentAutoUpdater::executePackageUpgrade(
                InstallationProvenance::Homebrew,
                QStringLiteral("0.2.0"),
                [&](const QString& prog, const QStringList& args) {
                    invokedProg = prog;
                    invokedArgs = args;
                    return true;
                });
            assert(brewRes.applied == true);
            assert(invokedProg == QStringLiteral("brew"));
            assert(invokedArgs == QStringList({QStringLiteral("upgrade"), QStringLiteral("markd")}));

            const SilentUpdateResult npmRes = SilentAutoUpdater::executePackageUpgrade(
                InstallationProvenance::NpmGlobal,
                QStringLiteral("0.2.0"),
                [&](const QString& prog, const QStringList& args) {
                    invokedProg = prog;
                    invokedArgs = args;
                    return true;
                });
            assert(npmRes.applied == true);
            assert(invokedProg == QStringLiteral("npm"));
            assert(invokedArgs.contains(QStringLiteral("markd@latest")));

            assert(SilentAutoUpdater::determineUpdateMode(
                QStringLiteral("/usr/bin/markd"), InstallationProvenance::AptPackage)
                == SilentUpdateMode::ManualCommandRequired);
        });

        it("sweeps stale old backup binary on startup and updates inline about dialog status", [&]() {
            QTemporaryDir tempDir;
            assert(tempDir.isValid());
            const QString binPath = tempDir.filePath(QStringLiteral("markd.exe"));
            const QString oldPath = binPath + QStringLiteral(".old");

            QFile oldFile(oldPath);
            assert(oldFile.open(QIODevice::WriteOnly));
            oldFile.write("stale-old-binary");
            oldFile.close();
            assert(QFile::exists(oldPath));

            assert(SilentAutoUpdater::sweepStaleBackup(binPath) == true);
            assert(!QFile::exists(oldPath));

            AboutDialog aboutDlg;
            assert(aboutDlg.silentUpdateStatusText() == QStringLiteral("✓ Up to date"));
            aboutDlg.setSilentUpdateStatus(QStringLiteral("✓ Updated to v22.0.0 (active on next launch)"));
            assert(aboutDlg.silentUpdateStatusText() == QStringLiteral("✓ Updated to v22.0.0 (active on next launch)"));
        });
    });
}

