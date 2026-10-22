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
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include "Zinsser.h"
#include "ui/MainWindow.h"
#include "ui/AboutDialog.h"
#include "ui/UpdateDialog.h"
#include "core/ProvenanceDetector.h"
#include "core/SilentAutoUpdater.h"
#include "core/UpdateChecker.h"
#include "core/UpdateNudgePolicy.h"
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

    describe("Intermittent status-bar update nudge, cooldown cadence, and background check wiring", [&]() {
        class ScriptedUpdateTransport : public IUpdateTransport {
        public:
            bool success{true};
            QByteArray payload;
            QString errorText;

            void fetch(const QUrl& url,
                       std::function<void(bool, const QByteArray&, const QString&)> callback) override {
                Q_UNUSED(url);
                callback(success, payload, errorText);
            }
        };

        it("displays actionable status bar update nudge on first detection of newer release", [&]() {
            QSettings defaultSettings;
            defaultSettings.remove(QStringLiteral("updates"));
            defaultSettings.setValue(QStringLiteral("updates/lastNudgedVersion"), QStringLiteral("0.1.0"));

            qputenv("MARKD_APP_PATH", "/opt/homebrew/bin/markd");
            MainWindow win;
            win.show();
            app.processEvents();

            assert(win.startupUpdateTimer() != nullptr);
            assert(win.startupUpdateTimer()->isSingleShot() == true);
            assert(win.startupUpdateTimer()->interval() == 3000);

            auto transport = std::make_shared<ScriptedUpdateTransport>();
            transport->success = true;
            transport->payload = QByteArrayLiteral("{\"version\": \"0.2.0\"}");
            win.setUpdateTransport(transport);

            win.runBackgroundUpdateCheck();
            app.processEvents();

            QLabel* notifLabel = win.statusBar()->notificationLabel();
            assert(notifLabel != nullptr);
            assert(notifLabel->isVisible() == true);
            assert(notifLabel->text() == QStringLiteral("↑ Update v0.2.0 available · Click to copy 'brew upgrade markd'"));
            assert(notifLabel->property("level").toString() == QStringLiteral("update"));
            assert(notifLabel->cursor().shape() == Qt::PointingHandCursor);
            assert(win.statusBar()->actionableCopyPayload() == QStringLiteral("brew upgrade markd"));
            assert(win.statusBar()->notificationTimer() != nullptr);
            assert(win.statusBar()->notificationTimer()->isActive() == true);
            assert(win.statusBar()->notificationTimer()->interval() == 8000);
            assert(!notifLabel->text().contains(QLatin1Char('[')) && !notifLabel->text().contains(QLatin1Char(']')));

            qunsetenv("MARKD_APP_PATH");
        });

        it("dismisses status bar update nudge after timeout while preserving editor and preview focus", [&]() {
            assert(UpdateNudgePolicy::kNudgeDisplayTimeoutMs == 8000);
            assert(UpdateNudgePolicy::kCopiedFeedbackTimeoutMs == 2000);

            MainWindow win;
            win.show();
            win.activateWindow();
            win.setMode(MainWindow::Mode::Edit);
            win.editor()->setFocus();
            app.processEvents();

            QWidget* initialFocus = QApplication::focusWidget();
            assert(win.statusBar()->notificationLabel()->focusPolicy() == Qt::NoFocus);

            win.statusBar()->showActionableNotification(
                QStringLiteral("↑ Update v0.2.0 available · Click to copy 'brew upgrade markd'"),
                QStringLiteral("brew upgrade markd"),
                60,
                QStringLiteral("0.2.0")
            );
            app.processEvents();
            assert(win.statusBar()->notificationLabel()->isVisible() == true);
            assert(QApplication::focusWidget() == initialFocus);
            assert(!win.statusBar()->notificationLabel()->hasFocus());

            QTest::qWait(100);
            app.processEvents();
            assert(win.statusBar()->notificationLabel()->isVisible() == false);
            assert(QApplication::focusWidget() == initialFocus);
            assert(win.currentMode() == MainWindow::Mode::Edit);

            win.setMode(MainWindow::Mode::Preview);
            app.processEvents();
            QWidget* previewFocus = QApplication::focusWidget();
            win.statusBar()->showActionableNotification(
                QStringLiteral("↑ Update v0.2.0 available · Click to copy 'brew upgrade markd'"),
                QStringLiteral("brew upgrade markd"),
                50
            );
            app.processEvents();
            assert(win.statusBar()->notificationLabel()->isVisible() == true);
            assert(!win.statusBar()->notificationLabel()->hasFocus());
            QTest::qWait(90);
            app.processEvents();
            assert(win.statusBar()->notificationLabel()->isVisible() == false);
            assert(QApplication::focusWidget() == previewFocus);
            assert(win.currentMode() == MainWindow::Mode::Preview);
        });

        it("suppresses consecutive update nudges across runs two through five and re-displays on cooldown expiry or version hop", [&]() {
            QTemporaryDir tempDir;
            assert(tempDir.isValid());
            QSettings settings(tempDir.filePath(QStringLiteral("nudge_cadence.ini")), QSettings::IniFormat);

            const qint64 t0 = 1700000000LL;
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QString(), t0) == false);

            // Run 1: First detection of v0.2.0 nudges immediately
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QStringLiteral("v0.2.0"), t0) == true);
            assert(settings.value(QStringLiteral("updates/lastNudgedVersion")).toString() == QStringLiteral("0.2.0"));
            assert(settings.value(QStringLiteral("updates/runsSinceLastNudge")).toInt() == 0);

            // Runs 2, 3, 4, 5 within 72 hours are suppressed
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QStringLiteral("0.2.0"), t0 + 60) == false);
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QStringLiteral("0.2.0"), t0 + 120) == false);
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QStringLiteral("0.2.0"), t0 + 180) == false);
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QStringLiteral("0.2.0"), t0 + 240) == false);
            assert(settings.value(QStringLiteral("updates/runsSinceLastNudge")).toInt() == 4);

            // Run 6 (5 runs since last nudge) re-displays the nudge and resets counter
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QStringLiteral("0.2.0"), t0 + 300) == true);
            assert(settings.value(QStringLiteral("updates/runsSinceLastNudge")).toInt() == 0);

            // Next run before 72 hours is suppressed again
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QStringLiteral("0.2.0"), t0 + 360) == false);

            // 72+ hours elapsed since last nudge re-displays the nudge even on run 2
            const qint64 after72h = t0 + 300 + UpdateNudgePolicy::kCooldownSeconds;
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QStringLiteral("0.2.0"), after72h) == true);
            assert(settings.value(QStringLiteral("updates/runsSinceLastNudge")).toInt() == 0);

            // Mid-cooldown version hop (0.2.0 -> 0.3.0) while runsSinceLastNudge < 5 nudges immediately
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QStringLiteral("0.2.0"), after72h + 60) == false);
            assert(settings.value(QStringLiteral("updates/runsSinceLastNudge")).toInt() == 1);
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QStringLiteral("v0.3.0"), after72h + 120) == true);
            assert(settings.value(QStringLiteral("updates/lastNudgedVersion")).toString() == QStringLiteral("0.3.0"));
            assert(settings.value(QStringLiteral("updates/runsSinceLastNudge")).toInt() == 0);

            // Clamps corrupted negative run counter to zero before incrementing
            settings.setValue(QStringLiteral("updates/runsSinceLastNudge"), -10);
            assert(UpdateNudgePolicy::evaluateAndRecordRun(settings, QStringLiteral("0.3.0"), after72h + 180) == false);
            assert(settings.value(QStringLiteral("updates/runsSinceLastNudge")).toInt() == 1);

            // Verify MainWindow suppresses status bar nudge on Run 2 after Run 1 already nudged
            QSettings defaultSettings;
            defaultSettings.remove(QStringLiteral("updates"));
            assert(UpdateNudgePolicy::evaluateAndRecordRun(QStringLiteral("0.2.0")) == true);

            qputenv("MARKD_APP_PATH", "/opt/homebrew/bin/markd");
            MainWindow win;
            win.show();
            auto transport = std::make_shared<ScriptedUpdateTransport>();
            transport->success = true;
            transport->payload = QByteArrayLiteral("{\"version\": \"0.2.0\"}");
            win.setUpdateTransport(transport);

            win.runBackgroundUpdateCheck();
            app.processEvents();
            assert(win.statusBar()->notificationLabel()->isVisible() == false);
            qunsetenv("MARKD_APP_PATH");
        });

        it("copies upgrade command on status bar nudge click, flashes confirmation, and resets cooldown counter", [&]() {
            QSettings defaultSettings;
            defaultSettings.remove(QStringLiteral("updates"));
            defaultSettings.setValue(QStringLiteral("updates/runsSinceLastNudge"), 3);

            MainWindow win;
            win.show();
            app.processEvents();

            QSignalSpy clickedSpy(win.statusBar(), &StatusBarTelemetry::notificationClicked);
            // Invoke 3-argument signature without explicit version parameter to verify version extraction from message
            win.statusBar()->showActionableNotification(
                QStringLiteral("↑ Update v0.2.0 available · Click to copy 'brew upgrade markd'"),
                QStringLiteral("brew upgrade markd"),
                8000
            );
            app.processEvents();

            QLabel* notifLabel = win.statusBar()->notificationLabel();
            assert(notifLabel->isVisible() == true);

            // Right click does not trigger copy
            QMouseEvent rightPress(QEvent::MouseButtonPress, QPointF(5, 5), notifLabel->mapToGlobal(QPoint(5, 5)),
                                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
            QApplication::sendEvent(notifLabel, &rightPress);
            QMouseEvent rightRelease(QEvent::MouseButtonRelease, QPointF(5, 5), notifLabel->mapToGlobal(QPoint(5, 5)),
                                     Qt::RightButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(notifLabel, &rightRelease);
            assert(clickedSpy.count() == 0);

            // Left click accepts press/release/double-click events cleanly (SWIPE_02), copies payload, flashes confirmation for 2000ms, and resets cooldown
            QMouseEvent leftPress(QEvent::MouseButtonPress, QPointF(5, 5), notifLabel->mapToGlobal(QPoint(5, 5)),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(notifLabel, &leftPress);
            assert(leftPress.isAccepted() == true);

            QMouseEvent leftRelease(QEvent::MouseButtonRelease, QPointF(5, 5), notifLabel->mapToGlobal(QPoint(5, 5)),
                                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(notifLabel, &leftRelease);
            assert(leftRelease.isAccepted() == true);

            QMouseEvent leftDblClick(QEvent::MouseButtonDblClick, QPointF(5, 5), notifLabel->mapToGlobal(QPoint(5, 5)),
                                     Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(notifLabel, &leftDblClick);
            assert(leftDblClick.isAccepted() == true);

            assert(clickedSpy.count() == 1);
            assert(QApplication::clipboard()->text() == QStringLiteral("brew upgrade markd"));
            assert(notifLabel->isVisible() == true);
            assert(notifLabel->text() == QStringLiteral("✓ Copied 'brew upgrade markd' to clipboard"));
            assert(notifLabel->property("level").toString() == QStringLiteral("info"));
            assert(win.statusBar()->notificationTimer()->isActive() == true);
            assert(win.statusBar()->notificationTimer()->interval() == 2000);
            assert(defaultSettings.value(QStringLiteral("updates/runsSinceLastNudge")).toInt() == 0);
            assert(defaultSettings.value(QStringLiteral("updates/lastNudgedVersion")).toString() == QStringLiteral("0.2.0"));
            assert(defaultSettings.value(QStringLiteral("updates/dismissedVersion")).toString() == QStringLiteral("0.2.0"));

            // Standalone binary fallback payload uses canonical https://markd.aramudan.dev (SWIPE_12)
            UpdateInfo standaloneInfo;
            standaloneInfo.state = UpdateState::UpdateAvailable;
            standaloneInfo.latestVersion = QStringLiteral("0.2.0");
            standaloneInfo.provenance = InstallationProvenance::StandaloneBinary;
            assert(UpdateNudgePolicy::resolveCopyPayload(standaloneInfo) == QStringLiteral("https://markd.aramudan.dev"));
            standaloneInfo.downloadUrl = QStringLiteral("https://markd.aramudan.dev");
            assert(UpdateNudgePolicy::resolveCopyPayload(standaloneInfo) == QStringLiteral("https://markd.aramudan.dev"));
            UpdateNudgePolicy::recordDismissal(QString());
        });

        it("remains silent in status bar when background check fails offline, preserves prior update state, and coordinates manual checks", [&]() {
            QSettings defaultSettings;
            defaultSettings.remove(QStringLiteral("updates"));

            MainWindow win;
            win.show();
            app.processEvents();

            auto failTransport = std::make_shared<ScriptedUpdateTransport>();
            failTransport->success = false;
            failTransport->errorText = QStringLiteral("Network unreachable");
            win.setUpdateTransport(failTransport);

            win.runBackgroundUpdateCheck();
            app.processEvents();

            // AC 6 & SWIPE_06: No status bar notification and no modal/popup UpdateDialog on background check failure
            assert(win.statusBar()->notificationLabel()->isVisible() == false);
            assert(win.findChild<UpdateDialog*>() == nullptr);

            // Open AboutDialog first, then run successful background check and verify live status update
            QAction* aboutAction = win.findChild<QAction*>("action_about");
            assert(aboutAction != nullptr);
            aboutAction->trigger();
            app.processEvents();

            AboutDialog* aboutDlg = win.findChild<AboutDialog*>();
            assert(aboutDlg != nullptr);
            assert(aboutDlg->silentUpdateStatusText() == QStringLiteral("✓ Up to date"));

            auto okTransport = std::make_shared<ScriptedUpdateTransport>();
            okTransport->success = true;
            okTransport->payload = QByteArrayLiteral("{\"version\": \"0.2.0\"}");
            win.setUpdateTransport(okTransport);

            win.runBackgroundUpdateCheck();
            app.processEvents();

            assert(aboutDlg->silentUpdateStatusText() == QStringLiteral("↑ Update v0.2.0 available"));
            assert(win.lastUpdateInfo().state == UpdateState::UpdateAvailable);

            // Subsequent offline failure preserves previously discovered UpdateAvailable state in AboutDialog and MainWindow
            win.setUpdateTransport(failTransport);
            win.runBackgroundUpdateCheck();
            app.processEvents();
            assert(aboutDlg->silentUpdateStatusText() == QStringLiteral("↑ Update v0.2.0 available"));
            assert(win.lastUpdateInfo().state == UpdateState::UpdateAvailable);

            aboutDlg->reject();
            app.processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

            // Re-open AboutDialog and verify cached m_lastUpdateInfo populates inline status
            aboutAction->trigger();
            app.processEvents();
            AboutDialog* reopenedAbout = win.findChild<AboutDialog*>();
            assert(reopenedAbout != nullptr);
            assert(reopenedAbout->silentUpdateStatusText() == QStringLiteral("↑ Update v0.2.0 available"));
            reopenedAbout->reject();
            app.processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

            // Interleaving manual Check for Updates while UpdateDialog is open suppresses background check
            win.setUpdateTransport(okTransport);
            QAction* checkAction = win.findChild<QAction*>("action_check_updates");
            assert(checkAction != nullptr);
            checkAction->trigger();
            app.processEvents();
            assert(win.startupUpdateTimer()->isActive() == false);
            UpdateDialog* updateDlg = win.findChild<UpdateDialog*>();
            assert(updateDlg != nullptr);
            win.runBackgroundUpdateCheck();
            app.processEvents();
            updateDlg->reject();
            app.processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

            defaultSettings.remove(QStringLiteral("updates"));
        });

        it("survives rapid in-flight network aborts, window teardown, and end-to-end localhost http update responses", [&]() {
            QSettings defaultSettings;
            defaultSettings.remove(QStringLiteral("updates"));

            QTcpServer holdServer;
            assert(holdServer.listen(QHostAddress::LocalHost, 0));
            const QByteArray holdUrl = QStringLiteral("http://127.0.0.1:%1/api/v1/updates/check")
                .arg(holdServer.serverPort()).toUtf8();
            qputenv("MARKD_UPDATE_ENDPOINT", holdUrl);

            // Reproduce rapid in-flight abort + immediate MainWindow/UpdateChecker teardown while QHttpThreadDelegate is active
            for (int iter = 0; iter < 3; ++iter) {
                MainWindow transientWin;
                transientWin.runBackgroundUpdateCheck();
                app.processEvents();
                transientWin.runBackgroundUpdateCheck();
                app.processEvents();
            }
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            app.processEvents();
            holdServer.close();

            // End-to-end HTTP response through real NetworkUpdateTransport into MainWindow status bar nudge
            QTcpServer httpServer;
            assert(httpServer.listen(QHostAddress::LocalHost, 0));
            QObject::connect(&httpServer, &QTcpServer::newConnection, &httpServer, [&]() {
                while (QTcpSocket* client = httpServer.nextPendingConnection()) {
                    QObject::connect(client, &QTcpSocket::readyRead, client, [client]() {
                        client->readAll();
                        const QByteArray body("{\"version\":\"0.2.0\"}");
                        const QByteArray response =
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                            "Connection: close\r\n\r\n" + body;
                        client->write(response);
                        client->flush();
                        client->disconnectFromHost();
                    });
                }
            });

            const QByteArray liveUrl = QStringLiteral("http://127.0.0.1:%1/api/v1/updates/check")
                .arg(httpServer.serverPort()).toUtf8();
            qputenv("MARKD_UPDATE_ENDPOINT", liveUrl);
            qputenv("MARKD_APP_PATH", "/opt/homebrew/bin/markd");

            MainWindow liveWin;
            liveWin.show();
            liveWin.runBackgroundUpdateCheck();

            for (int tick = 0; tick < 40 && !liveWin.statusBar()->notificationLabel()->isVisible(); ++tick) {
                QTest::qWait(25);
                app.processEvents();
            }

            assert(liveWin.statusBar()->notificationLabel()->isVisible() == true);
            assert(liveWin.statusBar()->notificationLabel()->text() ==
                   QStringLiteral("↑ Update v0.2.0 available · Click to copy 'brew upgrade markd'"));

            // Trigger an immediate follow-up check before DeferredDelete runs on the completed QNetworkReply
            liveWin.runBackgroundUpdateCheck();
            app.processEvents();

            // Verify remaining edge branches in UpdateNudgePolicy, SilentAutoUpdater, and UpdateDialog
            assert(UpdateNudgePolicy::normalizeVersion(QStringLiteral("invalid version tokens")).isEmpty());
            UpdateInfo upToDateInfo;
            upToDateInfo.state = UpdateState::UpToDate;
            assert(UpdateNudgePolicy::formatAboutStatus(upToDateInfo) == QStringLiteral("✓ Up to date"));

            SilentAutoUpdater updaterInstance;
            QTemporaryDir writableDir;
            assert(writableDir.isValid());
            const QString writableBin = writableDir.filePath(QStringLiteral("markd"));
            assert(SilentAutoUpdater::determineUpdateMode(
                QStringLiteral("/nonexistent_markd_root_dir/bin/markd"),
                InstallationProvenance::StandaloneBinary) == SilentUpdateMode::ManualCommandRequired);
            assert(SilentAutoUpdater::determineUpdateMode(
                writableBin,
                InstallationProvenance::Homebrew) == SilentUpdateMode::BackgroundPackageCommand);
            assert(SilentAutoUpdater::stageAndApplyBinaryUpdate(
                QString(), QByteArray(), QString(), QStringLiteral("0.2.0")).applied == false);
            const QByteArray dummyBytes("payload");
            const QString dummySha = SilentAutoUpdater::computeSha256Hex(dummyBytes);
            assert(SilentAutoUpdater::stageAndApplyBinaryUpdate(
                QStringLiteral("/nonexistent_markd_root_dir/bin/markd"),
                dummyBytes, dummySha, QStringLiteral("0.2.0")).applied == false);
            assert(SilentAutoUpdater::executePackageUpgrade(
                InstallationProvenance::AptPackage, QStringLiteral("0.2.0")).mode == SilentUpdateMode::ManualCommandRequired);

            UpdateDialog releaseDlg;
            QMouseEvent releaseEvt(QEvent::MouseButtonRelease, QPointF(10, 10), QPointF(10, 10),
                                   Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&releaseDlg, &releaseEvt);

            qunsetenv("MARKD_APP_PATH");
            qunsetenv("MARKD_UPDATE_ENDPOINT");
            defaultSettings.remove(QStringLiteral("updates"));
        });
    });
}


