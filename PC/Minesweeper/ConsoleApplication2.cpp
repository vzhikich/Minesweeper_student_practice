#include <SFML/Graphics.hpp>
#include <windows.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <string>
#include <sstream>

// ================= PROTOCOL =================
#define CMD_MINEFIELD 'M'
#define CMD_CLICK     'C'
#define CMD_ABORT     'A'

#define DIFF_EASY     'E'
#define DIFF_MEDIUM   'M'
#define DIFF_HARD     'H'

#define STATUS_OK     0x00
#define STATUS_LOSE   0x01
#define STATUS_WIN    0x02

#define CELL_CLOSED 255
#define CELL_FLAG   254
#define CELL_MINE   9

enum class State { MAIN_MENU, DIFFICULTY_MENU, GAME, EROR };

// ================= SERIAL =================
HANDLE OpenSerial(const char* port)
{
    HANDLE h = CreateFileA(port, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return nullptr;

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(dcb);
    GetCommState(h, &dcb);
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    SetCommState(h, &dcb);

    COMMTIMEOUTS t = { 50,50,10,50,10 };
    SetCommTimeouts(h, &t);
    return h;
}

uint8_t XOR_Checksum(const std::vector<uint8_t>& d)
{
    uint8_t c = 0;
    for (uint8_t b : d) c ^= b;
    return c;
}

bool SendPacket(HANDLE h, char cmd, const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> p;
    p.push_back(cmd);
    p.push_back((uint8_t)payload.size());
    p.insert(p.end(), payload.begin(), payload.end());
    p.push_back(XOR_Checksum(p));

    DWORD w;
    WriteFile(h, p.data(), p.size(), &w, nullptr);
    Sleep(5);
    return true;
}

bool ReceivePacket(HANDLE h, char& cmd, uint8_t& status, std::vector<uint8_t>& payload)
{
    DWORD r;
    uint8_t header[3];
    if (!ReadFile(h, header, 3, &r, nullptr) || r != 3)
        return false;

    cmd = header[0];
    status = header[1];
    uint8_t len = header[2];

    std::vector<uint8_t> data(len + 1);
    if (!ReadFile(h, data.data(), len + 1, &r, nullptr) || r != len + 1)
        return false;

    std::vector<uint8_t> chk = { (uint8_t)cmd, status, len };
    chk.insert(chk.end(), data.begin(), data.end() - 1);

    if (XOR_Checksum(chk) != data.back())
        return false;

    payload.assign(data.begin(), data.begin() + len);
    Sleep(5);
    return true;
}

// ================= SERIAL HEALTH CHECK =================
bool IsSerialAlive(HANDLE h)
{
    DWORD errors;
    COMSTAT stat;
    if (!ClearCommError(h, &errors, &stat))
        return false;

    return true;
}

// ================= MAIN =================
int main()
{
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    sf::RenderWindow window(sf::VideoMode(800, 600), "Minesweeper");
    window.setFramerateLimit(60);

    State state = State::MAIN_MENU;

    // ========== MENU TEXTURES ==========
    sf::Texture bgTex, playTex, exitTex;
    sf::Texture easyTex, medTex, hardTex;
    sf::Texture resetTex, backTex, errorTex;

    bgTex.loadFromFile("menu.png");
    playTex.loadFromFile("play.png");
    exitTex.loadFromFile("exit.png");
    easyTex.loadFromFile("easy.png");
    medTex.loadFromFile("medium.png");
    hardTex.loadFromFile("hard.png");
    resetTex.loadFromFile("reset.png");
    backTex.loadFromFile("back.png");
    errorTex.loadFromFile("error.png");

    sf::Sprite bg(bgTex), play(playTex), exitBtn(exitTex);
    sf::Sprite easy(easyTex), med(medTex), hard(hardTex);
    sf::Sprite resetBtn(resetTex), backBtn(backTex);
    sf::Sprite errorSprite(errorTex);

    bg.setScale(800.f / bgTex.getSize().x, 600.f / bgTex.getSize().y);
    play.setPosition(330, 200);
    exitBtn.setPosition(330, 320);

    easy.setPosition(330, 180);
    med.setPosition(310, 260);
    hard.setPosition(325, 340);

    resetBtn.setPosition(330, 330);
    backBtn.setPosition(330, 440);

    errorSprite.setPosition(
        (800 - errorTex.getSize().x) / 2.f,
        (600 - errorTex.getSize().y) / 2.f
    );

    // ========== CELL TEXTURES ==========
    sf::Texture numbers[9], closedTex, mineTex, flagTex, winTex, loseTex;
    for (int i = 0; i <= 8; i++)
        numbers[i].loadFromFile(std::to_string(i) + ".png");

    closedTex.loadFromFile("closed.png");
    mineTex.loadFromFile("mine.png");
    flagTex.loadFromFile("flag.png");
    winTex.loadFromFile("WIN.png");
    loseTex.loadFromFile("NOWIN.png");

    sf::Sprite cell, endGameSprite;

    // ========== TIMER ==========
    sf::Texture timerDigits[10], colonTex;
    for (int i = 0; i <= 9; i++)
        timerDigits[i].loadFromFile("(" + std::to_string(i) + ").png");
    colonTex.loadFromFile("colon.png");

    sf::Sprite timerSprites[4], colonSprite;
    colonSprite.setTexture(colonTex);

    // ========== SERIAL ==========
    HANDLE hSerial = OpenSerial("\\\\.\\COM10");
    if (!hSerial)
    {
        state = State::EROR;
    }

    std::vector<uint8_t> displayField;
    int fieldSize = 0;
    const float cellSize = 25.f;
    bool gameEnded = false;
    int timerSec = 0;
    char currentDiff = 0;

    while (window.isOpen())
    {
        sf::Event e;
        while (window.pollEvent(e))
        {
            if (e.type == sf::Event::Closed)
                window.close();

            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            // ===== EXIT ON ERROR STATE =====
            if (state == State::EROR) continue;

            // ===== LEFT CLICK (GAME) =====
            if (state == State::GAME && !gameEnded &&
                e.type == sf::Event::MouseButtonPressed &&
                e.mouseButton.button == sf::Mouse::Left)
            {
                float sx = (800 - fieldSize * cellSize) / 2.f;
                float sy = (600 - fieldSize * cellSize) / 2.f;
                int col = int((mouse.x - sx) / cellSize);
                int row = int((mouse.y - sy) / cellSize);

                if (row >= 0 && col >= 0 && row < fieldSize && col < fieldSize)
                {
                    int idx = row * fieldSize + col;
                    if (displayField[idx] == CELL_FLAG) continue;

                    SendPacket(hSerial, CMD_CLICK, { (uint8_t)row,(uint8_t)col });

                    char rc; uint8_t st; std::vector<uint8_t> r;
                    if (ReceivePacket(hSerial, rc, st, r))
                    {
                        for (size_t i = 0; i < r.size(); i += 3)
                            displayField[r[i] * fieldSize + r[i + 1]] = r[i + 2];

                        if (st == STATUS_LOSE)
                        {
                            gameEnded = true;
                            endGameSprite.setTexture(loseTex);
                        }
                        else if (st == STATUS_WIN)
                        {
                            gameEnded = true;
                            endGameSprite.setTexture(winTex);
                        }

                        if (gameEnded)
                            endGameSprite.setPosition(
                                (800 - endGameSprite.getTexture()->getSize().x) / 2.f,
                                (600 - endGameSprite.getTexture()->getSize().y) / 2.f);
                    }
                    else state = State::EROR;
                }
            }

            // ===== RIGHT CLICK (FLAG) =====
            if (state == State::GAME && !gameEnded &&
                e.type == sf::Event::MouseButtonPressed &&
                e.mouseButton.button == sf::Mouse::Right)
            {
                float sx = (800 - fieldSize * cellSize) / 2.f;
                float sy = (600 - fieldSize * cellSize) / 2.f;
                int col = int((mouse.x - sx) / cellSize);
                int row = int((mouse.y - sy) / cellSize);

                if (row >= 0 && col >= 0 && row < fieldSize && col < fieldSize)
                {
                    int idx = row * fieldSize + col;
                    if (displayField[idx] == CELL_CLOSED) displayField[idx] = CELL_FLAG;
                    else if (displayField[idx] == CELL_FLAG) displayField[idx] = CELL_CLOSED;
                }
            }

            // ===== GAME ENDED BUTTONS =====
            if (gameEnded && e.type == sf::Event::MouseButtonPressed &&
                e.mouseButton.button == sf::Mouse::Left)
            {
                // RESET
                if (resetBtn.getGlobalBounds().contains(mouse))
                {
                    SendPacket(hSerial, CMD_MINEFIELD, { (uint8_t)currentDiff });
                    char rc; uint8_t st; std::vector<uint8_t> field;
                    if (ReceivePacket(hSerial, rc, st, field))
                    {
                        fieldSize = int(std::sqrt(field.size()));
                        displayField.assign(fieldSize * fieldSize, CELL_CLOSED);
                        gameEnded = false;
                        timerSec = 0;
                    }
                    else state = State::EROR;
                }

                // BACK TO MENU
                if (backBtn.getGlobalBounds().contains(mouse))
                {
                    SendPacket(hSerial, CMD_ABORT, {});
                    state = State::MAIN_MENU;
                    gameEnded = false;
                    currentDiff = 0;
                }
            }

            // ===== MENUS =====
            if (e.type == sf::Event::MouseButtonPressed)
            {
                if (state == State::MAIN_MENU)
                {
                    if (play.getGlobalBounds().contains(mouse))
                        state = State::DIFFICULTY_MENU;
                    else if (exitBtn.getGlobalBounds().contains(mouse))
                        window.close();
                }
                else if (state == State::DIFFICULTY_MENU)
                {
                    if (backBtn.getGlobalBounds().contains(mouse))
                    {
                        state = State::MAIN_MENU;
                        continue;
                    }

                    char diff = 0;
                    if (easy.getGlobalBounds().contains(mouse)) diff = DIFF_EASY;
                    if (med.getGlobalBounds().contains(mouse)) diff = DIFF_MEDIUM;
                    if (hard.getGlobalBounds().contains(mouse)) diff = DIFF_HARD;

                    if (diff)
                    {
                        SendPacket(hSerial, CMD_MINEFIELD, { (uint8_t)diff });
                        char rc; uint8_t st; std::vector<uint8_t> field;
                        if (ReceivePacket(hSerial, rc, st, field))
                        {
                            fieldSize = int(std::sqrt(field.size()));
                            displayField.assign(fieldSize * fieldSize, CELL_CLOSED);
                            state = State::GAME;
                            gameEnded = false;
                            timerSec = 0;
                            currentDiff = diff;
                        }
                        else state = State::EROR;
                    }
                }
            }
        }

        // ===== TIMER UART =====
        if (!IsSerialAlive(hSerial))
        {
            state = State::EROR;
        }
        else
        {
            char ch;
            DWORD r;
            while (ReadFile(hSerial, &ch, 1, &r, nullptr) && r == 1)
            {
                if (ch == 'T')
                {
                    std::string s;
                    while (ReadFile(hSerial, &ch, 1, &r, nullptr) && r == 1 && ch != '\n')
                        s += ch;
                    try { timerSec = std::stoi(s); }
                    catch (...) {}
                }
            }
        }

        // ===== DRAW =====
        window.clear();

        if (state == State::EROR)
        {
            window.draw(errorSprite);
            window.display();
            continue;
        }

        window.draw(bg);

        if (state == State::MAIN_MENU)
        {
            window.draw(play);
            window.draw(exitBtn);
        }
        else if (state == State::DIFFICULTY_MENU)
        {
            window.draw(easy);
            window.draw(med);
            window.draw(hard);
            window.draw(backBtn);
        }
        else if (state == State::GAME)
        {
            float sx = (800 - fieldSize * cellSize) / 2.f;
            float sy = (600 - fieldSize * cellSize) / 2.f;

            for (int i = 0; i < fieldSize; i++)
                for (int j = 0; j < fieldSize; j++)
                {
                    cell.setPosition(sx + j * cellSize, sy + i * cellSize);
                    uint8_t v = displayField[i * fieldSize + j];
                    if (v == CELL_CLOSED) cell.setTexture(closedTex);
                    else if (v == CELL_FLAG) cell.setTexture(flagTex);
                    else if (v == CELL_MINE) cell.setTexture(mineTex);
                    else cell.setTexture(numbers[v]);
                    window.draw(cell);
                }

            // ===== DRAW TIMER MM:SS =====
            int m = timerSec / 60;
            int s = timerSec % 60;
            int digitsArr[4] = { m / 10, m % 10, s / 10, s % 10 };
            float timerX = 340.f;
            float timerY = 500.f;
            float spacing = 25.f;
            float scale = 1.f;
            for (int i = 0; i < 4; i++)
            {
                timerSprites[i].setTexture(timerDigits[digitsArr[i]]);
                timerSprites[i].setPosition(timerX + i * spacing + (i >= 2 ? 10.f : 0.f), timerY);
                timerSprites[i].setScale(scale, scale);
                window.draw(timerSprites[i]);
            }
            colonSprite.setPosition(timerX + 2 * spacing - 5, timerY);
            colonSprite.setScale(scale, scale);
            window.draw(colonSprite);

            if (gameEnded)
            {
                window.draw(endGameSprite);
                window.draw(resetBtn);
                window.draw(backBtn);
            }
        }

        window.display();
    }

    CloseHandle(hSerial);
    return 0;
}
