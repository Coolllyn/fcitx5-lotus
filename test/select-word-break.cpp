// SPDX-License-Identifier: GPL-3.0-or-later
//
// An invalid continuation typed after a committed replacement must not rewrite
// the committed word with the raw key strokes: the committed word stays and the
// key starts a new word.
#include "fcitx-utils/keysym.h"
#include "kb-socket-listener.h"
#include "lotus-engine.h"
#include "test-input-context.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace {

    bool press(fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, fcitx::KeySym symbol, bool requireAccepted) {
        fcitx::KeyEvent event(&context, fcitx::Key(symbol), false);
        engine.keyEvent(entry, event);
        if (event.accepted() == requireAccepted)
            return true;
        reportFailure("process key " + std::to_string(symbol), "accepted=" + std::to_string(static_cast<int>(requireAccepted)),
                      "accepted=" + std::to_string(static_cast<int>(event.accepted())) + ", commits=" + std::to_string(context.commits().size()), "unexpected key outcome");
        return false;
    }

    std::string commitList(const TestInputContext& context) {
        std::string actual = "commits=";
        for (const auto& commit : context.commits())
            actual += "['" + commit + "']";
        return actual;
    }

    // Types "at" + the sac mark key, waits for the Select request of 2 characters.
    bool startAtSacReplacement(fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, KbSocketListener& listener, const char* meaning) {
        if (!press(engine, entry, context, FcitxKey_a, false) || !press(engine, entry, context, FcitxKey_t, false) || !press(engine, entry, context, FcitxKey_s, true))
            return false;
        KbMsg msg{};
        if (!listener.receive(msg, meaning))
            return false;
        if (msg.op == KbOp::Select && msg.count == 2)
            return true;
        reportFailure("receive replacement request", "op=select, count=2", "op=" + std::to_string(static_cast<int>(msg.op)) + ", count=" + std::to_string(msg.count), meaning);
        return false;
    }

    // Plays back the echoed Shift+Left presses, the Delete and the anchor key.
    bool completeBurst(fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, size_t selects) {
        for (size_t i = 0; i < selects; ++i)
            if (!press(engine, entry, context, FcitxKey_Left, false))
                return false;
        return press(engine, entry, context, FcitxKey_Delete, false) && press(engine, entry, context, FcitxKey_Pause, true);
    }

    bool expectNoRequest(KbSocketListener& listener, const char* meaning) {
        KbMsg msg{};
        if (!listener.receive(msg, "no request expected", "no request within 5000 ms"))
            return true;
        reportFailure("wait after the invalid continuation", "no replacement request", "op=" + std::to_string(static_cast<int>(msg.op)) + ", count=" + std::to_string(msg.count),
                      meaning);
        return false;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-lotus-select-word-break");
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

    // Scenario 1: the 't' typed after the committed "át" makes the syllable
    // invalid and the fallback restores the raw key strokes. The committed word
    // must stay and 't' must start a new word instead of a rewrite.
    if (!startAtSacReplacement(engine, entry, *context, listener, "scenario 1: the select request did not arrive"))
        return 1;
    if (!completeBurst(engine, entry, *context, 2))
        return 1;
    const std::vector<std::string> committed{"át"};
    if (context->commits() != committed) {
        reportFailure("verify commit of the replacement", "commits=['át']", commitList(*context), "scenario 1: the replacement did not commit once");
        return 1;
    }
    if (!press(engine, entry, *context, FcitxKey_t, false))
        return 1;
    if (!expectNoRequest(listener, "scenario 1: the invalid continuation must not rewrite the committed word"))
        return 1;
    if (context->commits() != committed) {
        reportFailure("verify the committed word survived", "commits=['át']", commitList(*context), "scenario 1: the committed word was rewritten by the invalid continuation");
        return 1;
    }

    // Scenario 2: the same continuation typed while the burst is still running
    // is buffered and replayed after the commit; the replay must restart the
    // word the same way.
    if (!startAtSacReplacement(engine, entry, *context, listener, "scenario 2: the select request did not arrive"))
        return 1;
    if (!press(engine, entry, *context, FcitxKey_t, true))
        return 1;
    if (!completeBurst(engine, entry, *context, 2))
        return 1;
    // The replayed continuation is appended to the pending commit batch, so the
    // second "át" and the new word start arrive as one merged commit.
    const std::vector<std::string> replayed{"át", "átt"};
    if (context->commits() != replayed) {
        reportFailure("verify commits after the replay", "commits=['át']['átt']", commitList(*context),
                      "scenario 2: the replayed continuation must commit the new word start, not a raw rewrite");
        return 1;
    }
    if (!expectNoRequest(listener, "scenario 2: the replayed continuation must not start a rewrite"))
        return 1;

    std::printf("lotus select word break: all checks passed\n");
    return 0;
}
