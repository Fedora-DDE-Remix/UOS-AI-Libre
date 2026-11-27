#include "chatmodel.h"

#include <QCoreApplication>
#include <QVariantList>
#include <QVariantMap>
#include <iostream>

namespace {

QVariantMap message(const QString &role, const QString &text) {
    QVariantMap entry;
    entry.insert(QStringLiteral("role"), role);
    entry.insert(QStringLiteral("content"), text);
    return entry;
}

int fail(const char *message) {
    std::cerr << "chatmodel-test: " << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    ChatModel model;

    const QVariantList initial{
        message(QStringLiteral("user"), QStringLiteral("u1")),
        message(QStringLiteral("assistant"), QStringLiteral("a1")),
        message(QStringLiteral("user"), QStringLiteral("u2")),
        message(QStringLiteral("assistant"), QStringLiteral("a2")),
    };
    model.loadTranscript(initial, 40);
    if (model.rowCount() != 4)
        return fail("initial transcript row count");
    if (model.data(model.index(0), ChatModel::SourceIndexRole).toInt() != 40)
        return fail("initial source index");
    if (model.data(model.index(2), ChatModel::SourceIndexRole).toInt() != 42)
        return fail("later source index");
    if (model.data(model.index(0), ChatModel::TextRole).toString()
        != QStringLiteral("u1"))
        return fail("initial first row");
    if (model.data(model.index(3), ChatModel::TextRole).toString()
        != QStringLiteral("a2"))
        return fail("initial last row");

    const QVariantList older{
        message(QStringLiteral("user"), QStringLiteral("u0")),
        message(QStringLiteral("assistant"), QStringLiteral("a0")),
    };
    model.prependTranscript(older, 38);
    if (model.rowCount() != 6)
        return fail("prepended transcript row count");
    if (model.data(model.index(0), ChatModel::SourceIndexRole).toInt() != 38)
        return fail("prepended source index");
    if (model.data(model.index(2), ChatModel::SourceIndexRole).toInt() != 40)
        return fail("shifted source index");
    if (model.data(model.index(0), ChatModel::TextRole).toString()
        != QStringLiteral("u0"))
        return fail("prepended first row");
    if (model.data(model.index(1), ChatModel::TextRole).toString()
        != QStringLiteral("a0"))
        return fail("prepended second row");
    if (model.data(model.index(2), ChatModel::TextRole).toString()
        != QStringLiteral("u1"))
        return fail("existing first row after prepend");
    if (model.data(model.index(5), ChatModel::TextRole).toString()
        != QStringLiteral("a2"))
        return fail("existing last row after prepend");

    model.prependTranscript({}, 38);
    if (model.rowCount() != 6)
        return fail("empty prepend changed the model");

    model.appendUser(QStringLiteral("live"), {}, 100);
    if (model.rowCount() != 7)
        return fail("live append row count");
    if (model.data(model.index(6), ChatModel::SourceIndexRole).toInt() != 100)
        return fail("live append source index");

    std::cout << "chatmodel-test: ok\n";
    return 0;
}
