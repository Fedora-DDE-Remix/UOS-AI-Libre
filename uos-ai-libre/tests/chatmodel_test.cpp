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

    ChatModel live;
    live.beginAssistant(QStringLiteral("model"));
    live.beginReasoning();
    live.appendReasoning(QStringLiteral("think"));
    live.endReasoning();
    live.appendContent(QStringLiteral("before"));
    QVariantMap toolStart;
    toolStart.insert(QStringLiteral("tool_call_id"), QStringLiteral("t1"));
    toolStart.insert(QStringLiteral("tool_name"), QStringLiteral("WebFetch"));
    QVariantMap toolArgs;
    toolArgs.insert(QStringLiteral("url"), QStringLiteral("https://example.com"));
    toolStart.insert(QStringLiteral("tool_args"), toolArgs);
    live.addToolStart(toolStart);
    QVariantMap toolEnd;
    toolEnd.insert(QStringLiteral("tool_call_id"), QStringLiteral("t1"));
    toolEnd.insert(QStringLiteral("ok"), true);
    toolEnd.insert(QStringLiteral("duration_ms"), 5);
    live.addToolEnd(toolEnd);
    live.appendContent(QStringLiteral("after"));
    live.finishAssistant();

    const QVariantList timeline =
        live.data(live.index(0), ChatModel::TimelineRole).toList();
    if (timeline.size() != 4)
        return fail("timeline segment count");
    if (timeline.at(0).toMap().value(QStringLiteral("kind")).toString()
        != QStringLiteral("reasoning"))
        return fail("first timeline segment should be reasoning");
    if (timeline.at(0).toMap().value(QStringLiteral("reasoning")).toString()
        != QStringLiteral("think"))
        return fail("reasoning content");
    if (timeline.at(1).toMap().value(QStringLiteral("kind")).toString()
        != QStringLiteral("text"))
        return fail("second timeline segment should be text");
    if (timeline.at(2).toMap().value(QStringLiteral("kind")).toString()
        != QStringLiteral("tools"))
        return fail("third timeline segment should be tools");
    if (timeline.at(3).toMap().value(QStringLiteral("kind")).toString()
        != QStringLiteral("text"))
        return fail("fourth timeline segment should be text");
    if (live.data(live.index(0), ChatModel::TextRole).toString()
        != QStringLiteral("beforeafter"))
        return fail("timeline text accumulation");
    if (!live.data(live.index(0), ChatModel::ActiveTextRole).toString().isEmpty())
        return fail("active text should be flushed on finish");

    QVariantMap toolAssistant;
    toolAssistant.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    toolAssistant.insert(QStringLiteral("content"), QString());
    toolAssistant.insert(QStringLiteral("reasoning_content"), QStringLiteral("think"));
    QVariantList calls;
    QVariantMap function;
    function.insert(QStringLiteral("name"), QStringLiteral("WebFetch"));
    function.insert(QStringLiteral("arguments"),
                    QStringLiteral("{\"url\":\"https://example.com\"}"));
    QVariantMap call;
    call.insert(QStringLiteral("id"), QStringLiteral("t1"));
    call.insert(QStringLiteral("type"), QStringLiteral("function"));
    call.insert(QStringLiteral("function"), function);
    calls.append(call);
    toolAssistant.insert(QStringLiteral("tool_calls"), calls);
    QVariantMap toolResult;
    toolResult.insert(QStringLiteral("role"), QStringLiteral("tool"));
    toolResult.insert(QStringLiteral("content"), QStringLiteral("page"));
    toolResult.insert(QStringLiteral("tool_call_id"), QStringLiteral("t1"));
    QVariantMap finalAssistant;
    finalAssistant.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    finalAssistant.insert(QStringLiteral("content"), QStringLiteral("summary"));
    ChatModel transcript;
    transcript.loadTranscript({message(QStringLiteral("user"), QStringLiteral("fetch")),
                               toolAssistant, toolResult, finalAssistant});
    const QVariantList transcriptTimeline =
        transcript.data(transcript.index(1), ChatModel::TimelineRole).toList();
    if (transcriptTimeline.size() != 3)
        return fail("transcript timeline segment count");
    if (transcriptTimeline.at(0).toMap().value(QStringLiteral("kind")).toString()
        != QStringLiteral("reasoning"))
        return fail("transcript first segment should be reasoning");
    if (transcriptTimeline.at(1).toMap().value(QStringLiteral("kind")).toString()
        != QStringLiteral("tools"))
        return fail("transcript second segment should be tools");
    if (transcriptTimeline.at(2).toMap().value(QStringLiteral("kind")).toString()
        != QStringLiteral("text"))
        return fail("transcript third segment should be text");

    const QVariantMap snapshot = transcript.lastAssistantSnapshot();
    if (snapshot.value(QStringLiteral("text")).toString()
            != QStringLiteral("summary")
        || snapshot.value(QStringLiteral("timeline")).toList().size() != 3)
        return fail("last assistant snapshot");

    ChatModel multi;
    multi.beginAssistant(QStringLiteral("model"));
    multi.beginReasoning();
    multi.appendReasoning(QStringLiteral("r1"));
    multi.endReasoning();
    QVariantMap firstTool;
    firstTool.insert(QStringLiteral("tool_call_id"), QStringLiteral("a"));
    firstTool.insert(QStringLiteral("tool_name"), QStringLiteral("read_file"));
    multi.addToolStart(firstTool);
    QVariantMap firstDone;
    firstDone.insert(QStringLiteral("tool_call_id"), QStringLiteral("a"));
    firstDone.insert(QStringLiteral("ok"), true);
    multi.addToolEnd(firstDone);
    multi.beginReasoning();
    multi.appendReasoning(QStringLiteral("r2"));
    multi.endReasoning();
    QVariantMap secondTool;
    secondTool.insert(QStringLiteral("tool_call_id"), QStringLiteral("b"));
    secondTool.insert(QStringLiteral("tool_name"), QStringLiteral("read_file"));
    multi.addToolStart(secondTool);
    QVariantMap secondDone;
    secondDone.insert(QStringLiteral("tool_call_id"), QStringLiteral("b"));
    secondDone.insert(QStringLiteral("ok"), true);
    multi.addToolEnd(secondDone);
    multi.beginReasoning();
    multi.appendReasoning(QStringLiteral("r3"));
    multi.endReasoning();
    multi.appendContent(QStringLiteral("done"));
    multi.finishAssistant();
    const QVariantList multiTimeline =
        multi.data(multi.index(0), ChatModel::TimelineRole).toList();
    const QStringList expected{
        QStringLiteral("reasoning"), QStringLiteral("tools"),
        QStringLiteral("reasoning"), QStringLiteral("tools"),
        QStringLiteral("reasoning"), QStringLiteral("text"),
    };
    if (multiTimeline.size() != expected.size())
        return fail("multi-step timeline segment count");
    for (int i = 0; i < expected.size(); ++i) {
        if (multiTimeline.at(i).toMap().value(QStringLiteral("kind")).toString()
            != expected.at(i))
            return fail("multi-step timeline order");
    }

    ChatModel sequential;
    sequential.beginAssistant(QStringLiteral("model"));
    sequential.addToolStart(firstTool);
    sequential.addToolEnd(firstDone);
    sequential.addToolStart(secondTool);
    sequential.addToolEnd(secondDone);
    sequential.finishAssistant();
    const QVariantList sequentialTimeline =
        sequential.data(sequential.index(0), ChatModel::TimelineRole).toList();
    if (sequentialTimeline.size() != 2)
        return fail("sequential tools should keep separate steps");
    for (const QVariant &entry : sequentialTimeline) {
        if (entry.toMap().value(QStringLiteral("kind")).toString()
            != QStringLiteral("tools"))
            return fail("sequential tool block kind");
    }

    std::cout << "chatmodel-test: ok\n";
    return 0;
}
