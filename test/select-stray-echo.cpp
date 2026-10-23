// SPDX-License-Identifier: GPL-3.0-or-later
//
// A replacement only completes on its own echoes: a late Left/BackSpace of a previous burst (or of
// the other burst kind after a server-side Select fallback) must never be counted, otherwise the
// replacement finishes before the app applied its Delete and the commit duplicates the old tail.
#include "kb-socket-listener.h"
#include "lotus-engine.h"
#include "test-input-context.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace {

    void reportFailure(const std::string& step, const std::string& expected, const std::string& actual, const std::string& meaning) {
        std::fprintf(stderr, "Step: %s\nExpected: %s\nActual: %s\nMeaning: %s\n", step.c_str(), expected.c_str(), actual.c_str(), meaning.c_str());
    }

    bool send(fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, fcitx::KeySym symbol, bool requireAccepted) {
        fcitx::KeyEvent event(&context, fcitx::Key(symbol), false);
        engine.keyEvent(entry, event);
        if (event.accepted() != requireAccepted) {
            reportFailure("process key " + std::to_string(symbol), "accepted=" + std::to_string(static_cast<int>(requireAccepted)),
                          "accepted=" + std::to_string(static_cast<int>(event.accepted())) + ", commits=" + std::to_string(context.commits().size()),
                          "the select ack test saw an unexpected key outcome");
            return false;
        }
        return true;
    }

    std::string commitList(const TestInputContext& context) {
        std::string actual;
        for (const auto& commit : context.commits())
            actual += "['" + commit + "']";
        return actual;
    }

    bool receiveSelectRequest(KbSocketListener& listener, size_t& count, const char* meaning) {
        KbMsg msg{};
        if (!listener.receive(msg, meaning))
            return false;
        if (msg.op != KbOp::Select || msg.count == 0) {
            reportFailure("receive select request", "op=select, count > 0", "op=" + std::to_string(static_cast<int>(msg.op)) + ", count=" + std::to_string(msg.count), meaning);
            return false;
        }
        count = msg.count;
        return true;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-lotus-select-stray-echo");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Uinput (Select)");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);

    KbSocketListener listener;
    if (!listener.valid())
        return 1;

    fcitx::InputMethodEntry entry("lotus", "Lotus", "vi", "lotus");
    auto                    context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->focusIn();
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    if (!send(engine, entry, *context, FcitxKey_a, false) || !send(engine, entry, *context, FcitxKey_s, true))
        return 1;

    size_t selects = 0;
    if (!receiveSelectRequest(listener, selects, "the initial Telex replacement did not request a selection"))
        return 1;

    // A stray BackSpace echo (wrong kind for a Select burst) is forwarded and must not count.
    if (!send(engine, entry, *context, FcitxKey_BackSpace, false))
        return 1;
    if (!context->commits().empty()) {
        reportFailure("stray backspace echo", "no commits", commitList(*context), "a stray echo completed the replacement");
        return 1;
    }

    // The real Left echoes are intermediate, the anchor closes the burst.
    for (size_t i = 0; i < selects; ++i) {
        if (!send(engine, entry, *context, FcitxKey_Left, false))
            return 1;
    }
    if (!context->commits().empty()) {
        reportFailure("left echoes", "no commits yet", commitList(*context), "the replacement finished before the Selection was deleted");
        return 1;
    }
    if (!send(engine, entry, *context, FcitxKey_Delete, false))
        return 1;
    if (!send(engine, entry, *context, FcitxKey_Pause, true))
        return 1;

    const std::vector<std::string> expected{"á"};
    if (context->commits() != expected) {
        reportFailure("commit on the anchor", "commits=['á']", commitList(*context), "the anchor did not finish the replacement exactly once");
        return 1;
    }

    // A stray Left after the burst must not commit anything again.
    if (!send(engine, entry, *context, FcitxKey_Left, false))
        return 1;
    if (context->commits() != expected) {
        reportFailure("stray left after the burst", "commits unchanged", commitList(*context), "a stray echo produced a second commit");
        return 1;
    }

    std::printf("lotus select stray echo: all checks passed\n");
    return 0;
}
