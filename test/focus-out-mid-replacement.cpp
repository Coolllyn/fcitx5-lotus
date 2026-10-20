// SPDX-License-Identifier: GPL-3.0-or-later
//
// A focus bounce in the middle of a replacement loses the backspace echo for the injected keys: the
// input context is unfocused, so fcitx never delivers those key events back to the addon. The state
// must not stay in the deleting state forever - every further key would be queued and dropped.
#include "kb-socket-listener.h"
#include "lotus-engine.h"
#include "test-input-context.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

    bool send(fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, fcitx::KeySym symbol, bool requireAccepted) {
        fcitx::KeyEvent event(&context, fcitx::Key(symbol), false);
        engine.keyEvent(entry, event);
        if (event.accepted() != requireAccepted) {
            reportFailure("process key " + std::to_string(symbol), "accepted=" + std::to_string(static_cast<int>(requireAccepted)),
                          "accepted=" + std::to_string(static_cast<int>(event.accepted())) + ", commits=" + std::to_string(context.commits().size()),
                          "the mid-replacement focus-out test saw an unexpected key outcome");
            return false;
        }
        return true;
    }

    void loseFocus(fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context) {
        fcitx::InputContextEvent out(&context, fcitx::EventType::InputContextFocusOut);
        engine.deactivate(entry, out);
        engine.reset(entry, out);
        context.focusOut();
    }

    void gainFocus(fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context) {
        context.focusIn();
        fcitx::InputContextEvent in(&context, fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, in);
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-lotus-focus-out-mid-replacement");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Uinput (Smooth)");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("DefaultBackspaceInterval", "10");
    engine.setConfig(config);

    KbSocketListener listener;
    if (!listener.valid())
        return 1;

    fcitx::InputMethodEntry entry("lotus", "Lotus", "vi", "lotus");
    auto                    context = std::make_unique<TestInputContext>(&testInstance.instance);
    gainFocus(engine, entry, *context);

    // a, s: Telex turns the word into "á", which needs 1 deleted char + 1 anchor backspace.
    if (!send(engine, entry, *context, FcitxKey_a, false) || !send(engine, entry, *context, FcitxKey_s, true))
        return 1;
    KbMsg msg{};
    if (!listener.receive(msg, "the Telex replacement request did not arrive"))
        return 1;
    if (msg.op != KbOp::Backspace || msg.count != 2) {
        reportFailure("receive replacement request", "op=backspace, count=2", "op=" + std::to_string(static_cast<int>(msg.op)) + ", count=" + std::to_string(msg.count),
                      "the replacement never started");
        return 1;
    }

    // Only the first echo arrives; the second one is lost while the context is unfocused.
    if (!send(engine, entry, *context, FcitxKey_BackSpace, false))
        return 1;

    // Focus bounce in the middle of the deleting state.
    loseFocus(engine, entry, *context);
    gainFocus(engine, entry, *context);

    // Keys typed while the ack never completes must be queued, not committed.
    if (!send(engine, entry, *context, FcitxKey_q, true))
        return 1;
    if (!context->commits().empty()) {
        reportFailure("type while the replacement is pending", "no commits yet", "commits=" + std::to_string(context->commits().size()),
                      "a key was committed while the replacement still waits for its backspace echo");
        return 1;
    }

    // Past the ack deadline the replacement must finish and the queued key must be replayed. The
    // next activation is what checks the deadline.
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    gainFocus(engine, entry, *context);

    std::string committed;
    for (const auto& commit : context->commits())
        committed += commit;
    if (committed != "áq") {
        reportFailure("finish the overdue replacement", "committed text 'áq' ('á' + replayed 'q')", "committed='" + committed + "'",
                      "the replacement stayed stuck in the deleting state after the backspace echo was lost");
        return 1;
    }

    // After the recovery the queued key is gone from the queue: a plain letter is forwarded again.
    if (!send(engine, entry, *context, FcitxKey_q, false))
        return 1;

    std::printf("lotus focus-out mid replacement: all checks passed\n");
    return 0;
}
