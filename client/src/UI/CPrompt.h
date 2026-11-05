#pragma once
#include <functional>
#include <deque>
#include <string>

// [private build] yes/no question shown in the San Andreas help-box style.
// Y accepts, N rejects, the question expires on its own. Questions queue up.
class CPrompt
{
public:
    static void Ask(const std::string& text, std::function<void(bool)> onAnswer, unsigned int timeoutMs = 20000);
    static bool IsActive();
    static void Process();  // keys + timeout (gameProcessEvent)
    static void Draw();     // drawingEvent
    static void Clear();

private:
    struct SPrompt
    {
        std::string text;
        std::function<void(bool)> onAnswer;
        unsigned int timeoutMs;
        unsigned int startedAt;
    };
    static std::deque<SPrompt> ms_queue;
    static void Answer(bool accept);
};
