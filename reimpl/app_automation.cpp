#include "app_internal.h"

namespace app_detail
{

bool inputScriptScreenNameValid(const std::string& name)
{
    for(int screen = GAME_SCREEN_LOADING; screen <= GAME_SCREEN_EXIT; ++screen)
    {
        if(name == Game::screenName(static_cast<GameScreen>(screen)))
            return true;
    }
    return false;
}

std::string trimInputScriptText(const std::string& text)
{
    size_t begin = 0;
    while(begin < text.size() &&
          isspace(static_cast<unsigned char>(text[begin])) != 0)
        ++begin;
    size_t end = text.size();
    while(end > begin &&
          isspace(static_cast<unsigned char>(text[end - 1])) != 0)
        --end;
    return text.substr(begin, end - begin);
}

std::vector<std::string> tokenizeInputScriptLine(const std::string& line)
{
    std::vector<std::string> tokens;
    size_t cursor = 0;
    while(cursor < line.size())
    {
        while(cursor < line.size() &&
              isspace(static_cast<unsigned char>(line[cursor])) != 0)
            ++cursor;
        const size_t begin = cursor;
        while(cursor < line.size() &&
              isspace(static_cast<unsigned char>(line[cursor])) == 0)
            ++cursor;
        if(cursor > begin)
            tokens.push_back(line.substr(begin, cursor - begin));
    }
    return tokens;
}

bool parseInputScriptUnsigned(const std::string& text,
                              unsigned long long* value)
{
    if(value == NULL || text.empty() || text[0] == '-')
        return false;
    char* end = NULL;
    errno = 0;
    const unsigned long long parsed = strtoull(text.c_str(), &end, 10);
    if(errno == ERANGE || end == text.c_str() || *end != '\0')
        return false;
    *value = parsed;
    return true;
}

bool parseInputScriptNumber(const std::string& text, double* value)
{
    if(value == NULL || text.empty())
        return false;
    char* end = NULL;
    errno = 0;
    const double parsed = strtod(text.c_str(), &end);
    if(errno == ERANGE || end == text.c_str() || *end != '\0' ||
       !std::isfinite(parsed))
        return false;
    *value = parsed;
    return true;
}

uint32_t inputScriptButton(const std::string& name)
{
    if(name == "up")      return GAME_INPUT_UP;
    if(name == "down")    return GAME_INPUT_DOWN;
    if(name == "left")    return GAME_INPUT_LEFT;
    if(name == "right")   return GAME_INPUT_RIGHT;
    if(name == "confirm") return GAME_INPUT_CONFIRM;
    if(name == "cancel")  return GAME_INPUT_CANCEL;
    if(name == "start")   return GAME_INPUT_START;
    if(name == "pause")   return GAME_INPUT_PAUSE;
    if(name == "punch")   return GAME_INPUT_PUNCH;
    if(name == "kick")    return GAME_INPUT_KICK;
    if(name == "attack")  return GAME_INPUT_ATTACK;
    if(name == "guard")   return GAME_INPUT_GUARD;
    if(name == "heavy")   return GAME_INPUT_HEAVY;
    if(name == "run")     return GAME_INPUT_DODGE;
    if(name == "dodge")   return GAME_INPUT_DODGE;
    if(name == "grab")    return GAME_INPUT_GRAB;
    if(name == "interact") return GAME_INPUT_INTERACT;
    if(name == "use_first") return GAME_INPUT_USE_FIRST;
    if(name == "use_second") return GAME_INPUT_USE_SECOND;
    return 0;
}

bool parseInputScriptButtons(const std::string& text, uint32_t* buttons)
{
    if(buttons == NULL || text.empty())
        return false;
    uint32_t parsed = 0;
    size_t begin = 0;
    while(begin < text.size())
    {
        const size_t comma = text.find(',', begin);
        const size_t end = comma == std::string::npos ? text.size() : comma;
        if(end == begin)
            return false;
        const uint32_t button =
            inputScriptButton(text.substr(begin, end - begin));
        if(button == 0)
            return false;
        parsed |= button;
        if(comma == std::string::npos)
            break;
        begin = comma + 1;
    }
    *buttons = parsed;
    return true;
}

bool parseInputScriptScreens(
    const std::string& text, std::vector<std::string>* names)
{
    if(names == NULL || text.empty())
        return false;
    size_t begin = 0;
    while(begin < text.size())
    {
        const size_t comma = text.find(',', begin);
        const size_t end = comma == std::string::npos ? text.size() : comma;
        if(end == begin)
            return false;
        const std::string name = text.substr(begin, end - begin);
        if(!inputScriptScreenNameValid(name))
            return false;
        names->push_back(name);
        if(comma == std::string::npos)
            break;
        begin = comma + 1;
    }
    return !names->empty();
}

bool reportInputScriptParseError(int lineNumber, const char* message)
{
    fprintf(stderr, "input script parse error at %s:%d: %s\n",
            g_inputScriptPath != NULL ? g_inputScriptPath : "(unknown)",
            lineNumber, message);
    return false;
}

bool parseInputScriptCommand(const std::string& text, int lineNumber)
{
    const std::vector<std::string> tokens = tokenizeInputScriptLine(text);
    if(tokens.empty())
        return true;

    InputScriptCommand command;
    command.lineNumber = lineNumber;
    command.text = text;
    if(tokens[0] == "wait")
    {
        if(tokens.size() < 3)
            return reportInputScriptParseError(
                lineNumber, "wait requires a kind and value");
        if(tokens[1] == "screen")
        {
            if(tokens.size() != 3 && tokens.size() != 4)
                return reportInputScriptParseError(
                    lineNumber,
                    "wait screen requires names and an optional timeout");
            command.type = INPUT_SCRIPT_WAIT_SCREEN;
            if(!parseInputScriptScreens(tokens[2], &command.screenNames))
                return reportInputScriptParseError(
                    lineNumber, "wait screen has an invalid screen name");
            command.timeoutSeconds = INPUT_SCRIPT_DEFAULT_TIMEOUT_SECONDS;
            if(tokens.size() == 4 &&
               (!parseInputScriptNumber(tokens[3],
                                        &command.timeoutSeconds) ||
                command.timeoutSeconds <= 0.0))
            {
                return reportInputScriptParseError(
                    lineNumber, "wait screen timeout must be positive");
            }
        }
        else if(tokens[1] == "frames")
        {
            if(tokens.size() != 3 ||
               !parseInputScriptUnsigned(tokens[2], &command.frameCount))
            {
                return reportInputScriptParseError(
                    lineNumber,
                    "wait frames requires a non-negative integer");
            }
            command.type = INPUT_SCRIPT_WAIT_FRAMES;
            command.timeoutSeconds = INPUT_SCRIPT_DEFAULT_TIMEOUT_SECONDS;
        }
        else if(tokens[1] == "seconds")
        {
            if(tokens.size() != 3 ||
               !parseInputScriptNumber(tokens[2],
                                       &command.durationSeconds) ||
               command.durationSeconds < 0.0)
            {
                return reportInputScriptParseError(
                    lineNumber,
                    "wait seconds requires a non-negative number");
            }
            command.type = INPUT_SCRIPT_WAIT_SECONDS;
            command.timeoutSeconds =
                command.durationSeconds + INPUT_SCRIPT_DEFAULT_TIMEOUT_SECONDS;
        }
        else
        {
            return reportInputScriptParseError(
                lineNumber, "wait kind must be screen, frames or seconds");
        }
    }
    else if(tokens[0] == "press")
    {
        if(tokens.size() != 2 && tokens.size() != 5)
            return reportInputScriptParseError(
                lineNumber,
                "press requires buttons, optional screen condition, or post capture");
        if(!parseInputScriptButtons(tokens[1], &command.buttons))
            return reportInputScriptParseError(
                lineNumber, "press has an invalid button name");
        if(tokens.size() == 5)
        {
            if(tokens[2] == "capture" && tokens[3] == "post" &&
               !tokens[4].empty())
            {
                command.postInputCaptureName = tokens[4];
            }
            else if(tokens[2] == "if" && tokens[3] == "screen" &&
                    inputScriptScreenNameValid(tokens[4]))
            {
                command.conditionScreen = tokens[4];
            }
            else
            {
                return reportInputScriptParseError(
                    lineNumber,
                    "press suffix must be 'capture post <name>' or "
                    "'if screen <name>'");
            }
        }
        command.type = INPUT_SCRIPT_PRESS;
    }
    else if(tokens[0] == "hold")
    {
        if(tokens.size() != 3 ||
           !parseInputScriptButtons(tokens[1], &command.buttons) ||
           !parseInputScriptNumber(tokens[2], &command.durationSeconds) ||
           command.durationSeconds < 0.0)
        {
            return reportInputScriptParseError(
                lineNumber,
                "hold requires valid buttons and non-negative seconds");
        }
        command.type = INPUT_SCRIPT_HOLD;
    }
    else if(tokens[0] == "axis")
    {
        double axisX = 0.0;
        double axisY = 0.0;
        if(tokens.size() != 4 ||
           !parseInputScriptNumber(tokens[1], &axisX) ||
           !parseInputScriptNumber(tokens[2], &axisY) ||
           !parseInputScriptNumber(tokens[3], &command.durationSeconds) ||
           axisX < -1.0 || axisX > 1.0 ||
           axisY < -1.0 || axisY > 1.0 ||
           command.durationSeconds < 0.0)
        {
            return reportInputScriptParseError(
                lineNumber,
                "axis requires x/y in [-1,1] and non-negative seconds");
        }
        command.type = INPUT_SCRIPT_AXIS;
        command.axisX = static_cast<float>(axisX);
        command.axisY = static_cast<float>(axisY);
    }
    else if(tokens[0] == "capture")
    {
        size_t nameIndex = 1;
        command.captureTiming = INPUT_SCRIPT_CAPTURE_PRE_EDGE;
        if(tokens.size() == 3)
        {
            if(tokens[1] != "pre" && tokens[1] != "post")
                return reportInputScriptParseError(
                    lineNumber, "capture timing must be 'pre' or 'post'");
            nameIndex = 2;
            command.captureTiming =
                tokens[1] == "post"
                    ? INPUT_SCRIPT_CAPTURE_POST_UPDATE
                    : INPUT_SCRIPT_CAPTURE_PRE_EDGE;
        }
        if((tokens.size() != 2 && tokens.size() != 3) ||
           tokens[nameIndex].empty())
        {
            return reportInputScriptParseError(
                lineNumber, "capture requires a name, optionally after pre/post");
        }
        command.type = INPUT_SCRIPT_CAPTURE;
        command.captureName = tokens[nameIndex];
    }
    else if(tokens[0] == "expect")
    {
        if(tokens.size() != 3 || tokens[1] != "screen" ||
           !inputScriptScreenNameValid(tokens[2]))
        {
            return reportInputScriptParseError(
                lineNumber, "expect requires 'screen <name>'");
        }
        command.type = INPUT_SCRIPT_EXPECT_SCREEN;
        command.screenNames.push_back(tokens[2]);
    }
    else if(tokens[0] == "quit")
    {
        if(tokens.size() != 1)
            return reportInputScriptParseError(
                lineNumber, "quit takes no arguments");
        command.type = INPUT_SCRIPT_QUIT;
    }
    else
    {
        return reportInputScriptParseError(lineNumber, "unknown command");
    }

    g_inputScriptCommands.push_back(command);
    return true;
}

bool loadInputScript(void)
{
    if(g_inputScriptPath == NULL)
        return true;
    FILE* input = strcmp(g_inputScriptPath, "-") == 0 ?
        stdin : fopen(g_inputScriptPath, "rb");
    if(input == NULL)
    {
        fprintf(stderr, "input script: could not open '%s'\n",
                g_inputScriptPath);
        return false;
    }

    char buffer[2048];
    int lineNumber = 0;
    bool valid = true;
    while(fgets(buffer, sizeof(buffer), input) != NULL)
    {
        ++lineNumber;
        if(strchr(buffer, '\n') == NULL && !feof(input))
        {
            valid = reportInputScriptParseError(
                lineNumber, "line exceeds 2047 bytes");
            break;
        }
        std::string line(buffer);
        const size_t comment = line.find('#');
        if(comment != std::string::npos)
            line.erase(comment);
        line = trimInputScriptText(line);
        if(!line.empty() && !parseInputScriptCommand(line, lineNumber))
        {
            valid = false;
            break;
        }
    }
    if(valid && ferror(input) != 0)
    {
        fprintf(stderr, "input script: could not read '%s'\n",
                g_inputScriptPath);
        valid = false;
    }
    if(input != stdin)
        fclose(input);
    if(valid)
    {
        printf("input script: loaded %u commands from %s\n",
               static_cast<unsigned int>(g_inputScriptCommands.size()),
               g_inputScriptPath);
    }
    return valid;
}

bool inputScriptAtScreen(const InputScriptCommand& command)
{
    const char* current = Game::screenName(g_game.screen());
    for(size_t i = 0; i < command.screenNames.size(); ++i)
    {
        if(command.screenNames[i] == current)
            return true;
    }
    return false;
}

void failInputScript(const InputScriptCommand& command, const char* message)
{
    fprintf(stderr,
            "input script FAILED at line %d: %s\n"
            "  command: %s\n"
            "  screen: %s\n",
            command.lineNumber, message, command.text.c_str(),
            Game::screenName(g_game.screen()));
    fflush(stderr);
    exit(EXIT_FAILURE);
}

void completeInputScriptCommand(const InputScriptCommand& command,
                                const char* outcome)
{
    printf("input script line %d done: %s -- %s\n",
           command.lineNumber, command.text.c_str(), outcome);
    ++g_inputScriptCommandIndex;
    g_inputScriptCommandStarted = false;
    g_inputScriptCommandFrames = 0;
    g_inputScriptCommandSeconds = 0.0;
}

bool inputScriptCommandTimedOut(const InputScriptCommand& command)
{
    if(command.timeoutSeconds <= 0.0)
        return false;
    const std::chrono::duration<double> elapsed =
        std::chrono::steady_clock::now() - g_inputScriptCommandStartTime;
    return elapsed.count() >= command.timeoutSeconds;
}

void applyInputScript(float deltaSeconds, uint32_t* held, uint32_t* pressed)
{
    if(g_inputScriptPath == NULL || held == NULL || pressed == NULL)
        return;
    if(!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f)
        deltaSeconds = 0.0f;

    while(g_inputScriptCommandIndex < g_inputScriptCommands.size())
    {
        const InputScriptCommand& command =
            g_inputScriptCommands[g_inputScriptCommandIndex];
        if(!g_inputScriptCommandStarted)
        {
            printf("input script line %d start: %s\n",
                   command.lineNumber, command.text.c_str());
            g_inputScriptCommandStarted = true;
            g_inputScriptCommandFrames = 0;
            g_inputScriptCommandSeconds = 0.0;
            g_inputScriptCommandStartTime =
                std::chrono::steady_clock::now();
        }

        switch(command.type)
        {
        case INPUT_SCRIPT_WAIT_SCREEN:
            if(inputScriptAtScreen(command))
            {

                if(g_inputScriptCommandFrames == 0)
                {
                    ++g_inputScriptCommandFrames;
                    return;
                }
                completeInputScriptCommand(command, "screen reached");
                continue;
            }
            g_inputScriptCommandFrames = 0;
            if(inputScriptCommandTimedOut(command))
                failInputScript(command, "timed out waiting for screen");
            return;

        case INPUT_SCRIPT_WAIT_FRAMES:
            if(g_inputScriptCommandFrames >= command.frameCount)
            {
                completeInputScriptCommand(command, "frame wait elapsed");
                continue;
            }
            if(inputScriptCommandTimedOut(command))
                failInputScript(command, "timed out waiting for frames");
            ++g_inputScriptCommandFrames;
            return;

        case INPUT_SCRIPT_WAIT_SECONDS:
            if(g_inputScriptCommandSeconds >= command.durationSeconds)
            {
                completeInputScriptCommand(command, "time wait elapsed");
                continue;
            }
            if(inputScriptCommandTimedOut(command))
                failInputScript(command, "timed out waiting for seconds");
            g_inputScriptCommandSeconds += deltaSeconds;
            return;

        case INPUT_SCRIPT_PRESS:
            if(g_inputScriptCommandFrames == 0)
            {
                if(!command.conditionScreen.empty() &&
                   command.conditionScreen !=
                       Game::screenName(g_game.screen()))
                {
                    completeInputScriptCommand(
                        command, "condition false; skipped");
                    continue;
                }
                *held |= command.buttons;
                *pressed |= command.buttons;
                if(!command.postInputCaptureName.empty())
                {
                    if(!frameCaptureEnabled())
                    {
                        failInputScript(
                            command,
                            "post capture requires an enabled --capture-dir");
                    }

                    frameCaptureRequest(
                        command.postInputCaptureName.c_str());
                }
                ++g_inputScriptCommandFrames;
                return;
            }
            completeInputScriptCommand(command, "pressed and released");
            return;

        case INPUT_SCRIPT_HOLD:
            if(g_inputScriptCommandSeconds >= command.durationSeconds)
            {
                completeInputScriptCommand(command, "hold elapsed");
                continue;
            }
            *held |= command.buttons;
            if(g_inputScriptCommandFrames == 0)
                *pressed |= command.buttons;
            ++g_inputScriptCommandFrames;
            g_inputScriptCommandSeconds += deltaSeconds;
            return;

        case INPUT_SCRIPT_AXIS:
            if(g_inputScriptCommandSeconds >= command.durationSeconds)
            {
                completeInputScriptCommand(command, "axis interval elapsed");
                continue;
            }
            g_game.setPlayerMovementAxes(command.axisX, command.axisY);
            ++g_inputScriptCommandFrames;
            g_inputScriptCommandSeconds += deltaSeconds;
            return;

        case INPUT_SCRIPT_CAPTURE:
            if(!frameCaptureEnabled())
            {
                failInputScript(
                    command,
                    "capture requires an enabled --capture-dir");
            }
            if(g_inputScriptCommandFrames == 0)
            {
                if(command.captureTiming ==
                   INPUT_SCRIPT_CAPTURE_POST_UPDATE)
                {

                    frameCaptureRequest(command.captureName.c_str());
                }
                else
                {

                    frameCaptureRequestImmediate(
                        command.captureName.c_str());
                }
                ++g_inputScriptCommandFrames;
                return;
            }
            completeInputScriptCommand(command, "capture serviced");

            return;

        case INPUT_SCRIPT_EXPECT_SCREEN:
            if(!inputScriptAtScreen(command))
                failInputScript(command, "screen expectation failed");
            completeInputScriptCommand(command, "screen matched");
            continue;

        case INPUT_SCRIPT_QUIT:
            completeInputScriptCommand(command, "clean exit requested");
            printf("input script complete\n");
            g_inputScriptCompletionReported = true;
            sk::globals.quit = 1;
            return;
        }
    }

    if(!g_inputScriptCompletionReported)
    {
        printf("input script complete; application remains running\n");
        g_inputScriptCompletionReported = true;
    }
}

bool inputScriptCaptureActive(void)
{
    return g_inputScriptPath != NULL &&
           g_inputScriptCommandStarted &&
           g_inputScriptCommandIndex < g_inputScriptCommands.size() &&
           g_inputScriptCommands[g_inputScriptCommandIndex].type ==
               INPUT_SCRIPT_CAPTURE;
}

bool autoAdvanceTargetValid(const char* target)
{
    if(target == NULL)
        return false;
    if(strcmp(target, "first_cutscene") == 0 ||
       strcmp(target, "first_control") == 0 ||
       strcmp(target, "first_walk") == 0 ||
       strcmp(target, "first_battle") == 0 ||
       strcmp(target, "first_guard") == 0)
        return true;
    for(int screen = GAME_SCREEN_LOADING;
        screen <= GAME_SCREEN_EXIT; ++screen)
    {
        if(strcmp(target,
                  Game::screenName(static_cast<GameScreen>(screen))) == 0)
            return true;
    }
    return false;
}

bool autoAdvanceAtTarget(void)
{
    if(g_autoAdvanceTarget == NULL)
        return false;
    if(strcmp(g_autoAdvanceTarget, "first_cutscene") == 0)
        return g_gameSceneActive;
    if(strcmp(g_autoAdvanceTarget, "first_control") == 0)
    {
        return g_game.screen() == GAME_SCREEN_GAMEPLAY &&
               !g_gameSceneActive && !hudAdvicePanelActive() &&
               g_game.runtimeState().story.progression.adviceShownMask != 0;
    }
    if(strcmp(g_autoAdvanceTarget, "first_walk") == 0)
    {
        return g_game.screen() == GAME_SCREEN_GAMEPLAY &&
               !g_gameSceneActive && !hudAdvicePanelActive() &&
               g_game.runtimeState().story.progression.adviceShownMask != 0 &&
               g_game.runtimeState().player.action == COMBAT_ACTION_MOVE &&
               g_autoAdvanceMovementFrames >= 20;
    }
    if(strcmp(g_autoAdvanceTarget, "first_battle") == 0)
    {
        return g_game.screen() == GAME_SCREEN_GAMEPLAY &&
               !g_gameSceneActive && !hudAdvicePanelActive() &&
               g_game.runtimeState().story.battleActive != 0;
    }
    if(strcmp(g_autoAdvanceTarget, "first_guard") == 0)
    {
        return g_game.screen() == GAME_SCREEN_GAMEPLAY &&
               !g_gameSceneActive && !hudAdvicePanelActive() &&
               g_game.runtimeState().story.battleActive != 0 &&
               g_game.runtimeState().player.action == COMBAT_ACTION_GUARD;
    }
    return strcmp(g_autoAdvanceTarget,
                  Game::screenName(g_game.screen())) == 0;
}

void observeAutoAdvanceTarget(void)
{
    if(g_autoAdvanceReached || !autoAdvanceAtTarget())
        return;
    g_autoAdvanceReached = true;
    g_autoAdvanceSettleFrames = 0;
    if(strcmp(g_autoAdvanceTarget, "first_cutscene") == 0 ||
       strcmp(g_autoAdvanceTarget, "first_control") == 0 ||
       strcmp(g_autoAdvanceTarget, "first_walk") == 0 ||
       strcmp(g_autoAdvanceTarget, "first_battle") == 0 ||
       strcmp(g_autoAdvanceTarget, "first_guard") == 0)
        frameCaptureRequest(g_autoAdvanceTarget);
    printf("auto advance reached %s\n", g_autoAdvanceTarget);
}

void applyAutoAdvance(float deltaSeconds,
                      uint32_t* held, uint32_t* pressed)
{
    if(g_autoAdvanceTarget == NULL || held == NULL || pressed == NULL)
        return;
    if(strcmp(g_autoAdvanceTarget, "first_walk") == 0 &&
       g_game.screen() == GAME_SCREEN_GAMEPLAY &&
       !g_gameSceneActive && !hudAdvicePanelActive() &&
       g_game.runtimeState().story.progression.adviceShownMask != 0)
    {
        ++g_autoAdvanceMovementFrames;
        g_game.setPlayerMovementAxes(0.55f, 0.0f);
    }
    if(strcmp(g_autoAdvanceTarget, "first_guard") == 0 &&
       g_game.screen() == GAME_SCREEN_GAMEPLAY &&
       !g_gameSceneActive && !hudAdvicePanelActive() &&
       g_game.runtimeState().story.battleActive != 0)
        *held |= GAME_INPUT_GUARD;
    observeAutoAdvanceTarget();
    if(g_autoAdvanceReached)
    {
        if(++g_autoAdvanceSettleFrames >= 3)
            sk::globals.quit = 1;
        return;
    }

    g_autoAdvancePulseSeconds -= deltaSeconds;
    if(g_autoAdvancePulseSeconds > 0.0f)
        return;
    if(g_game.screen() == GAME_SCREEN_TITLE)
        *pressed |= GAME_INPUT_START;
    else if(g_game.screen() == GAME_SCREEN_MAIN_MENU &&
            strcmp(g_autoAdvanceTarget, "versus_type_select") == 0)
    {
        if(g_game.mainMenuSelection() < 2)
            *pressed |= GAME_INPUT_DOWN;
        else if(g_game.mainMenuSelection() > 2)
            *pressed |= GAME_INPUT_UP;
        else
            *pressed |= GAME_INPUT_CONFIRM;
    }
    else
        *pressed |= GAME_INPUT_CONFIRM;
    g_autoAdvancePulseSeconds = 0.35f;
}

bool validNativeHandle(HANDLE handle)
{
    return handle != NULL && handle != INVALID_HANDLE_VALUE;
}

HANDLE duplicateNativeHandle(HANDLE source)
{
    if(!validNativeHandle(source))
        return NULL;

    HANDLE duplicate = NULL;
    if(DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(),
                       &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS) == 0)
        return NULL;
    return duplicate;
}

DWORD WINAPI nativeLogPump(void*)
{
    char buffer[4096];
    DWORD bytesRead = 0;
    while(ReadFile(g_nativeLogRead, buffer, sizeof(buffer),
                   &bytesRead, NULL) != 0 && bytesRead != 0)
    {
        DWORD bytesWritten = 0;
        WriteFile(g_nativeLogFile, buffer, bytesRead, &bytesWritten, NULL);
        HANDLE console = validNativeHandle(g_nativeLogOriginalOutput) ?
            g_nativeLogOriginalOutput : g_nativeLogOriginalError;
        if(validNativeHandle(console))
            WriteFile(console, buffer, bytesRead, &bytesWritten, NULL);
    }
    FlushFileBuffers(g_nativeLogFile);
    return 0;
}

bool beginNativeLog(const char* path)
{
    if(path == NULL || path[0] == '\0' || g_nativeLogActive)
        return false;
    g_nativeLogFile = CreateFileA(
        path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(!validNativeHandle(g_nativeLogFile))
        return false;

    HANDLE writePipe = NULL;
    if(CreatePipe(&g_nativeLogRead, &writePipe, NULL, 0) == 0)
    {
        CloseHandle(g_nativeLogFile);
        g_nativeLogFile = NULL;
        return false;
    }
    SetHandleInformation(g_nativeLogRead, HANDLE_FLAG_INHERIT, 0);

    g_nativeLogOriginalOutput =
        duplicateNativeHandle(GetStdHandle(STD_OUTPUT_HANDLE));
    g_nativeLogOriginalError =
        duplicateNativeHandle(GetStdHandle(STD_ERROR_HANDLE));
    if((_fileno(stdout) < 0 && freopen("NUL", "wb", stdout) == NULL) ||
       (_fileno(stderr) < 0 && freopen("NUL", "wb", stderr) == NULL))
    {
        if(validNativeHandle(g_nativeLogOriginalOutput))
            CloseHandle(g_nativeLogOriginalOutput);
        if(validNativeHandle(g_nativeLogOriginalError))
            CloseHandle(g_nativeLogOriginalError);
        CloseHandle(g_nativeLogRead);
        CloseHandle(writePipe);
        CloseHandle(g_nativeLogFile);
        g_nativeLogRead = g_nativeLogFile = NULL;
        g_nativeLogOriginalOutput = g_nativeLogOriginalError = NULL;
        return false;
    }

    g_nativeLogSavedStdout = _dup(_fileno(stdout));
    g_nativeLogSavedStderr = _dup(_fileno(stderr));
    g_nativeLogThread = CreateThread(
        NULL, 0, nativeLogPump, NULL, 0, NULL);
    if(!validNativeHandle(g_nativeLogThread))
    {
        if(g_nativeLogSavedStdout >= 0)
            _close(g_nativeLogSavedStdout);
        if(g_nativeLogSavedStderr >= 0)
            _close(g_nativeLogSavedStderr);
        CloseHandle(g_nativeLogRead);
        CloseHandle(writePipe);
        CloseHandle(g_nativeLogFile);
        g_nativeLogRead = g_nativeLogFile = NULL;
        g_nativeLogSavedStdout = g_nativeLogSavedStderr = -1;
        if(validNativeHandle(g_nativeLogOriginalOutput))
            CloseHandle(g_nativeLogOriginalOutput);
        if(validNativeHandle(g_nativeLogOriginalError))
            CloseHandle(g_nativeLogOriginalError);
        g_nativeLogOriginalOutput = g_nativeLogOriginalError = NULL;
        return false;
    }

    const int pipeDescriptor = _open_osfhandle(
        reinterpret_cast<intptr_t>(writePipe), _O_WRONLY | _O_BINARY);
    const bool stdoutRedirected =
        pipeDescriptor >= 0 &&
        _dup2(pipeDescriptor, _fileno(stdout)) == 0;
    const bool stderrRedirected =
        stdoutRedirected &&
        _dup2(pipeDescriptor, _fileno(stderr)) == 0;
    if(!stderrRedirected)
    {
        if(stdoutRedirected && g_nativeLogSavedStdout >= 0)
            _dup2(g_nativeLogSavedStdout, _fileno(stdout));
        if(pipeDescriptor >= 0)
            _close(pipeDescriptor);
        else
            CloseHandle(writePipe);
        WaitForSingleObject(g_nativeLogThread, 5000);
        CloseHandle(g_nativeLogThread);
        CloseHandle(g_nativeLogRead);
        CloseHandle(g_nativeLogFile);
        if(g_nativeLogSavedStdout >= 0)
            _close(g_nativeLogSavedStdout);
        if(g_nativeLogSavedStderr >= 0)
            _close(g_nativeLogSavedStderr);
        g_nativeLogRead = g_nativeLogThread = g_nativeLogFile = NULL;
        if(validNativeHandle(g_nativeLogOriginalOutput))
            CloseHandle(g_nativeLogOriginalOutput);
        if(validNativeHandle(g_nativeLogOriginalError))
            CloseHandle(g_nativeLogOriginalError);
        g_nativeLogOriginalOutput = g_nativeLogOriginalError = NULL;
        g_nativeLogSavedStdout = g_nativeLogSavedStderr = -1;
        return false;
    }
    _close(pipeDescriptor);
    SetStdHandle(
        STD_OUTPUT_HANDLE,
        reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout))));
    SetStdHandle(
        STD_ERROR_HANDLE,
        reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr))));
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    g_nativeLogActive = true;
    return true;
}

void endNativeLog(void)
{
    if(!g_nativeLogActive)
        return;
    fflush(stdout);
    fflush(stderr);
    if(g_nativeLogSavedStdout >= 0)
        _dup2(g_nativeLogSavedStdout, _fileno(stdout));
    else
        _close(_fileno(stdout));
    if(g_nativeLogSavedStderr >= 0)
        _dup2(g_nativeLogSavedStderr, _fileno(stderr));
    else
        _close(_fileno(stderr));
    if(g_nativeLogSavedStdout >= 0)
    {
        SetStdHandle(
            STD_OUTPUT_HANDLE,
            reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout))));
    }
    if(g_nativeLogSavedStderr >= 0)
    {
        SetStdHandle(
            STD_ERROR_HANDLE,
            reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr))));
    }
    WaitForSingleObject(g_nativeLogThread, 5000);
    CloseHandle(g_nativeLogThread);
    CloseHandle(g_nativeLogRead);
    CloseHandle(g_nativeLogFile);
    if(g_nativeLogSavedStdout >= 0)
        _close(g_nativeLogSavedStdout);
    if(g_nativeLogSavedStderr >= 0)
        _close(g_nativeLogSavedStderr);
    g_nativeLogThread = g_nativeLogRead = g_nativeLogFile = NULL;
    if(validNativeHandle(g_nativeLogOriginalOutput))
        CloseHandle(g_nativeLogOriginalOutput);
    if(validNativeHandle(g_nativeLogOriginalError))
        CloseHandle(g_nativeLogOriginalError);
    g_nativeLogOriginalOutput = g_nativeLogOriginalError = NULL;
    g_nativeLogSavedStdout = g_nativeLogSavedStderr = -1;
    g_nativeLogActive = false;
}

bool beginAutomatedRunArtifacts(void)
{
#ifdef _WIN32
    char executablePath[MAX_PATH];
    const DWORD length = GetModuleFileNameA(
        NULL, executablePath, static_cast<DWORD>(sizeof(executablePath)));
    if(length == 0 || length >= sizeof(executablePath))
        return false;

    const char* executable = executablePath;
    const char* slash = strrchr(executablePath, '\\');
    if(slash != NULL)
        executable = slash + 1;
    slash = strrchr(executable, '/');
    if(slash != NULL)
        executable = slash + 1;
    char executableName[MAX_PATH];
    snprintf(executableName, sizeof(executableName), "%s", executable);
    char* extension = strrchr(executableName, '.');
    if(extension != NULL)
        *extension = '\0';

    const time_t now = time(NULL);
    struct tm localTime;
    if(localtime_s(&localTime, &now) != 0)
        return false;
    char stem[512];
    snprintf(stem, sizeof(stem),
             "%04d-%02d-%02d_%02d-%02d-%02d_%s",
             localTime.tm_year + 1900, localTime.tm_mon + 1,
             localTime.tm_mday, localTime.tm_hour, localTime.tm_min,
             localTime.tm_sec, executableName);

    g_runArtifactDirectory[0] = '\0';
    for(int occurrence = 1; occurrence <= 999; ++occurrence)
    {
        if(occurrence == 1)
        {
            snprintf(g_runArtifactDirectory,
                     sizeof(g_runArtifactDirectory), "%s", stem);
        }
        else
        {
            snprintf(g_runArtifactDirectory,
                     sizeof(g_runArtifactDirectory), "%s_%d",
                     stem, occurrence);
        }
        if(CreateDirectoryA(g_runArtifactDirectory, NULL) != 0)
            break;
        if(GetLastError() != ERROR_ALREADY_EXISTS)
        {
            g_runArtifactDirectory[0] = '\0';
            break;
        }
        g_runArtifactDirectory[0] = '\0';
    }
    if(g_runArtifactDirectory[0] == '\0')
        return false;

    frameCaptureSetDirectory(g_runArtifactDirectory);
    char logPath[640];
    snprintf(logPath, sizeof(logPath), "%s/debug.log",
             g_runArtifactDirectory);
    printf("run artifacts: %s\n", g_runArtifactDirectory);
    fflush(stdout);
    if(!beginNativeLog(logPath))
        return false;
    printf("run artifacts: %s\n", g_runArtifactDirectory);
    return true;
#else
    return false;
#endif
}

void parseArguments(void)
{

    setvbuf(stdout, NULL, _IONBF, 0);
    for(int i = 1; i < sk::args.argc; i++)
    {
        if(strcmp(sk::args.argv[i], "--save") == 0 && i + 1 < sk::args.argc)
            g_savePath = sk::args.argv[++i];
        else if(strcmp(sk::args.argv[i], "--fstage") == 0 && i + 1 < sk::args.argc)
            g_fstagePath = sk::args.argv[++i];
        else if(strcmp(sk::args.argv[i], "--stage") == 0 && i + 1 < sk::args.argc)
        {
            g_stageEntry = sk::args.argv[++i];
            g_stageOverride = true;
        }
        else if(strcmp(sk::args.argv[i], "--debug-overlay") == 0)
            g_hostDebugOverlayEnabled = true;
        else if(strcmp(sk::args.argv[i], "--scene") == 0 &&
                i + 1 < sk::args.argc)
            g_sceneEntry = sk::args.argv[++i];
        else if(strcmp(sk::args.argv[i], "--capture-dir") == 0 &&
                i + 1 < sk::args.argc)
            frameCaptureSetDirectory(sk::args.argv[++i]);
        else if(strcmp(sk::args.argv[i], "--capture-frame") == 0 &&
                i + 2 < sk::args.argc)
        {
            const long frame = atol(sk::args.argv[++i]);
            frameCaptureRequestFrame(frame, sk::args.argv[++i]);
        }
        else if(strcmp(sk::args.argv[i], "--width") == 0 &&
                i + 1 < sk::args.argc)
            g_windowWidth = atoi(sk::args.argv[++i]);
        else if(strcmp(sk::args.argv[i], "--height") == 0 &&
                i + 1 < sk::args.argc)
            g_windowHeight = atoi(sk::args.argv[++i]);
        else if(strcmp(sk::args.argv[i], "--input-script") == 0)
        {
            if(i + 1 >= sk::args.argc)
            {
                fprintf(stderr,
                        "input script: --input-script requires a path\n");
                exit(EXIT_FAILURE);
            }
            if(g_inputScriptPath != NULL)
            {
                fprintf(stderr,
                        "input script: --input-script may only be specified once\n");
                exit(EXIT_FAILURE);
            }
            g_inputScriptPath = sk::args.argv[++i];
        }
        else if(strcmp(sk::args.argv[i], "--auto-advance-to") == 0 &&
                i + 1 < sk::args.argc)
        {
            g_autoAdvanceTarget = sk::args.argv[++i];
            if(!autoAdvanceTargetValid(g_autoAdvanceTarget))
            {
                fprintf(stderr,
                        "ERROR: invalid --auto-advance-to target '%s'\n",
                        g_autoAdvanceTarget);
                g_autoAdvanceTarget = NULL;
                g_argumentError = true;
            }
        }
    }
    if(g_inputScriptPath != NULL && !loadInputScript())
    {
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
}

}
