#pragma once

#include "nau/compiler/nau_source_compiler.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <cmath>

// Each channel gets a separate parser: process reads need not contain whole lines.
class NauBuildProgressParser
{
public:
    explicit NauBuildProgressParser(std::function<void(const NauSourceCompiler::NauBuildProgress&)> sink)
        : m_sink(std::move(sink)) {}

    void feed(const QByteArray& bytes)
    {
        for (const char c : bytes) {
            if (c == '\r' || c == '\n') {
                if (!m_discard) parse();
                m_line.clear();
                m_discard = false;
            } else if (!m_discard) {
                if (m_line.size() >= 65536) {
                    m_line.clear();
                    m_discard = true;
                } else {
                    m_line.append(c);
                }
            }
        }
    }

    void finish()
    {
        if (!m_discard) parse();
        m_line.clear();
    }

private:
    void parse()
    {
        constexpr auto prefix = "NAU_BUILD_EVENT ";
        if (!m_sink || !m_line.startsWith(prefix)) return;
        const auto doc = QJsonDocument::fromJson(m_line.mid(sizeof("NAU_BUILD_EVENT ") - 1));
        if (!doc.isObject()) return;
        const auto obj = doc.object();
        if (!obj.value("v").isDouble() || obj.value("v").toDouble() != 1) return;
        const auto stage = obj.value("stage");
        const auto unit = obj.value("unit");
        if (!stage.isString() || !unit.isString()) return;
        static const QStringList stages = {"prepare", "configure", "compile-sources", "compile-assets",
            "copy-binaries", "package-assets", "write-package", "copy-resources", "copy-config", "finalize"};
        if (!stages.contains(stage.toString()) || (unit.toString() != "files" && unit.toString() != "bytes")) return;
        auto readCount = [](const QJsonValue& value, qint64& result) {
            if (!value.isDouble()) return false;
            const double n = value.toDouble();
            // JSON numbers are exact integers only up to 2^53 - 1.
            if (!std::isfinite(n) || n < 0 || n > 9007199254740991.0 || std::floor(n) != n) return false;
            result = static_cast<qint64>(n);
            return true;
        };
        NauSourceCompiler::NauBuildProgress progress;
        progress.stage = stage.toString();
        progress.unit = unit.toString();
        if (!readCount(obj.value("completed"), progress.completed)) return;
        const auto total = obj.value("total");
        if (!total.isNull() && !readCount(total, progress.total)) return;
        if (progress.total >= 0 && progress.completed > progress.total) return;
        if (obj.contains("file") && !obj.value("file").isString()) return;
        progress.file = obj.value("file").toString();
        m_sink(progress);
    }

    QByteArray m_line;
    bool m_discard = false;
    std::function<void(const NauSourceCompiler::NauBuildProgress&)> m_sink;
};
