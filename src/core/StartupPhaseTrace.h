#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QtGlobal>
#include <cstdio>

// Opt-in startup phase marks for scripts/benchmark_startup.sh (Task 8.12.7).
// MARKD_STARTUP_PHASES=1 prints "MARKD_PHASE <name> <epoch_ms>" to stderr; otherwise nothing.
// Locked names: main_entry, window_shown, plugin_loaded, profile_created, template_loaded,
// bridge_ready, content_revealed. Marks print on every occurrence.
namespace StartupPhaseTrace {

inline bool enabled()
{
    return qEnvironmentVariableIntValue("MARKD_STARTUP_PHASES") == 1;
}

inline QByteArray formatMark(const char* name, qint64 epochMs)
{
    return QByteArray("MARKD_PHASE ") + name + ' ' + QByteArray::number(epochMs) + '\n';
}

inline void mark(const char* name)
{
    if (!enabled()) return;
    const QByteArray line = formatMark(name, QDateTime::currentMSecsSinceEpoch());
    std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stderr);
    std::fflush(stderr);
}

} // namespace StartupPhaseTrace
