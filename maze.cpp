//==============================================================================
// 中文迷宫游戏  ——  基于 Win32 API + GDI（深色主题美化版）
// 功能：迷宫生成（深度优先递归回溯）、键盘移动、难度选择、计时、计步
//       通关后自动重新生成迷宫；首次启动需点击"开始游戏"按钮
// 视觉：统一深色主题 / 自定义按钮（hover/active/disabled 过渡）/ 圆角卡片
//       圆角墙线 / 玩家光晕 / 缓动移动动画 / 拟态状态栏
// 编码：源文件以 UTF-8 保存，编译时需开启执行字符集 UTF-8
//==============================================================================

#pragma execution_character_set("utf-8")
#include "theme.h"
#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <stack>
#include <string>
#include <random>
#include <ctime>
#include <sstream>
#include <cmath>
#include <cstring>

//------------------------------------------------------------------------------
// 中文文本常量（全部 UTF-8 编码）
//------------------------------------------------------------------------------
static const char* TXT_TITLE            = "迷宫游戏";
static const char* TXT_MENU_GAME        = "游戏(&G)";
static const char* TXT_MENU_DIFFICULTY  = "难度(&D)";
static const char* TXT_MENU_HELP        = "帮助(&H)";
static const char* TXT_ITEM_NEW         = "新游戏(&N)";
static const char* TXT_ITEM_EXIT        = "退出(&X)";
static const char* TXT_ITEM_EASY        = "简单  15×15";
static const char* TXT_ITEM_MEDIUM      = "中等  25×25";
static const char* TXT_ITEM_HARD        = "困难  35×35";
static const char* TXT_ITEM_HOWTO       = "操作说明(&O)";
static const char* TXT_ITEM_ABOUT       = "关于(&A)";
static const char* TXT_BTN_START        = "开始游戏";
static const char* TXT_DIFF_EASY        = "简单";
static const char* TXT_DIFF_MEDIUM      = "中等";
static const char* TXT_DIFF_HARD        = "困难";
static const char* TXT_MSG_WELCOME      = "欢迎进入迷宫之旅";
static const char* TXT_MSG_WELCOME_HINT = "选择难度后点击「开始游戏」即可启程";
static const char* TXT_MSG_HOWTO        = "操作说明：\n\n1. 选择难度后点击「开始游戏」按钮即可开始\n2. 使用 ↑ ↓ ← → 方向键 或 W A S D 控制角色移动\n3. 从左上角走到右下角即可通关\n4. 角色无法穿过墙壁\n5. 按 P 键可以暂停 / 继续游戏（暂停时计时与动画都会冻结）\n6. 通关后将自动重新生成新迷宫";
static const char* TXT_MSG_ABOUT        = "中文迷宫游戏 v2.0\n\n深色主题版 · Microsoft YaHei UI\nWin32 GDI · 双缓冲渲染\n支持 P 键暂停\n通关后自动重新生成迷宫";
static const char* TXT_STATUS_PLAYING   = "步数 %d  ·  用时 %d 秒";
static const char* TXT_STATUS_PAUSED    = "已暂停  ·  步数 %d  ·  用时 %d 秒（按 P 继续）";
static const char* TXT_STATUS_WIN       = "★ 通关！步数 %d，用时 %d 秒 — %d 秒后自动开始新迷宫 ★";

// 控件 ID
#define IDM_NEW          1001
#define IDM_EXIT         1002
#define IDM_EASY         1003
#define IDM_MEDIUM       1004
#define IDM_HARD         1005
#define IDM_HOWTO        1006
#define IDM_ABOUT        1007
#define IDB_START        2001
#define IDB_EASY         2002
#define IDB_MEDIUM       2003
#define IDB_HARD         2004
#define IDT_STATUS       3001   // 状态栏刷新（约 250ms）
#define IDT_ANIM         3002   // 全局动画帧（约 16ms / 60fps）
#define IDT_AUTO_REGEN   3003   // 通关后自动重新生成

#define AUTO_REGEN_DELAY_MS  3000
#define PLAYER_ANIM_MS       130
#define HOVER_ANIM_RATE      0.18   // 每帧 hover 进度增量（60fps → ~3 帧即可完成）

//==============================================================================
// 音效系统 ——  内存生成 WAV，零外部资源
//==============================================================================

// 内存 WAV 容器（RIFF/WAVE/PCM 16bit 单声道）
struct MemWav {
    std::vector<BYTE> bytes;
};

// 合成 PCM 单音轨（基频 + 倍频，营造更丰富音色），带 ADSR 包络
// gain: 主增益（0..1），默认 0.78，可调到接近 1.0 让小喇叭也听得见
static MemWav SynthWav(const std::vector<std::pair<double, double>>& notes,
                        int sampleRate = 22050,
                        double attack  = 0.008,
                        double release = 0.06,
                        double gain    = 0.78) {
    const double PI = 3.14159265358979323846;
    std::vector<short> pcm;
    double t = 0.0;  // 连续时间轴，避免相邻音符衔接时相位跳变
    for (auto& n : notes) {
        double f = n.first;       // 基频
        double dur = n.second;    // 持续秒数
        int nSamples = (int)(sampleRate * dur);
        for (int i = 0; i < nSamples; ++i) {
            double lt = (double)i / sampleRate;
            double env;
            if (lt < attack) {
                env = lt / attack;                                // 线性起音
            } else {
                double k = 3.0 / std::max(dur - attack, 0.01);    // 指数衰减
                env = std::exp(-(lt - attack) * k);
            }
            // 基频 + 八度 + 轻微五度叠合，音色更圆润
            double s  = 0.65 * std::sin(2 * PI * f * t)
                      + 0.25 * std::sin(2 * PI * f * 2 * t)
                      + 0.10 * std::sin(2 * PI * f * 3 * t);
            s *= env * gain;
            if (s > 1)  s = 1;
            if (s < -1) s = -1;
            pcm.push_back((short)(s * 32767));
            t += 1.0 / sampleRate;
        }
    }
    int dataSize = (int)(pcm.size() * 2);
    int fileSize = 36 + dataSize;
    MemWav wav;
    wav.bytes.resize(8 + fileSize);
    BYTE* p = wav.bytes.data();
    auto w32 = [&](DWORD v) { memcpy(p, &v, 4); p += 4; };
    auto w16 = [&](WORD v)   { memcpy(p, &v, 2); p += 2; };
    auto wstr = [&](const char* s, int n) { memcpy(p, s, n); p += n; };
    wstr("RIFF", 4);
    w32(fileSize);
    wstr("WAVE", 4);
    wstr("fmt ", 4);
    w32(16);            // fmt chunk size
    w16(1);             // audio format = PCM
    w16(1);             // channels = mono
    w32(sampleRate);    // sample rate
    w32(sampleRate * 2); // byte rate (16bit × mono)
    w16(2);             // block align
    w16(16);            // bits per sample
    wstr("data", 4);
    w32(dataSize);
    memcpy(p, pcm.data(), dataSize);
    return wav;
}

// 预生成的所有音效（启动时一次性合成 PCM 样本）
static MemWav  G_WavClick, G_WavMove, G_WavWin;
static MemWav  G_WavWall, G_WavDiff;            // 撞墙 / 难度切换
static MemWav  G_WavCount1, G_WavCount2, G_WavCount3;   // 倒计时 3-2-1
static MemWav  G_WavHover;                       // 按钮 hover

static std::vector<short> G_ClickSamples, G_MoveSamples, G_WinSamples;
static std::vector<short> G_WallSamples, G_DiffSamples;
static std::vector<short> G_Count1Samples, G_Count2Samples, G_Count3Samples;
static std::vector<short> G_HoverSamples;
static bool    G_SoundsReady = false;

//------------------------------------------------------------------------------
// waveOut 低层音频输出（绕过 PlaySoundW / WASAPI，兼容性最好）
//------------------------------------------------------------------------------
static HWAVEOUT       G_hWaveOut = NULL;
static WAVEFORMATEX   G_WaveFmt  = {0};

// 播放完毕后由 waveOut 回调函数回收的缓冲区（含 PCM 样本 + WAVEHDR）
struct WaveBuffer {
    WAVEHDR hdr;
    std::vector<short> samples;
};

static void CALLBACK WaveOutProc(HWAVEOUT hwo, UINT uMsg,
                                 DWORD_PTR dwInstance,
                                 DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (uMsg == WOM_DONE) {
        WAVEHDR* pHdr = (WAVEHDR*)dwParam1;
        WaveBuffer* pBuf = (WaveBuffer*)pHdr->dwUser;
        // 先 unprepare 再 delete，顺序不能反
        waveOutUnprepareHeader(hwo, pHdr, sizeof(WAVEHDR));
        delete pBuf;
    }
}

static bool OpenWaveOut() {
    if (G_hWaveOut) return true;
    G_WaveFmt.wFormatTag      = WAVE_FORMAT_PCM;
    G_WaveFmt.nChannels       = 1;
    G_WaveFmt.nSamplesPerSec  = 22050;
    G_WaveFmt.nAvgBytesPerSec = 22050 * 2;
    G_WaveFmt.nBlockAlign     = 2;
    G_WaveFmt.wBitsPerSample  = 16;
    G_WaveFmt.cbSize          = 0;
    MMRESULT r = waveOutOpen(&G_hWaveOut, WAVE_MAPPER, &G_WaveFmt,
                             (DWORD_PTR)WaveOutProc, 0, CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) {
        G_hWaveOut = NULL;
        return false;
    }
    return true;
}

static void CloseWaveOut() {
    if (G_hWaveOut) {
        waveOutReset(G_hWaveOut);   // 取消所有未播放 buffer，让回调回收它们
        waveOutClose(G_hWaveOut);
        G_hWaveOut = NULL;
    }
}

static void InitSounds() {
    if (G_SoundsReady) return;
    // 按钮点击：880 Hz 短促"叮"
    G_WavClick = SynthWav({ { 880.0, 0.07 } });
    // 移动脚步：660 Hz（E5）单音，120ms，主增益拉到 0.95（接近满幅）。
// 频率选在中高频段，笔记本小喇叭/手机扬声器对 660Hz 比之前的 330Hz 灵敏得多；
// gain 0.95 把振幅从 0.78 提到接近极限，确保在低音量时也明显能听见。
G_WavMove  = SynthWav({ { 660.0, 0.12 } }, 22050, 0.008, 0.06, 0.95);
    // 通关提示音：C5 → E5 → G5 → C6 上升琶音，模拟胜利号角
    G_WavWin   = SynthWav({
        { 523.25, 0.16 },   // C5
        { 659.25, 0.16 },   // E5
        { 783.99, 0.16 },   // G5
        { 1046.5, 0.45 },   // C6（带尾音延音）
    });

    // 撞墙音：110 Hz 单音，60ms，attack 仅 3ms 几乎瞬起 + 快指数衰减 → 低沉"咚"
    G_WavWall  = SynthWav({ { 110.0, 0.06 } }, 22050, 0.003, 0.05, 0.95);
    // 难度切换音：C5 → G5 上升，8ms+10ms，"叮咚"确认感
    G_WavDiff  = SynthWav({ { 523.25, 0.08 }, { 783.99, 0.10 } }, 22050, 0.005, 0.05, 0.85);
    // 倒计时：880 → 660 → 440 Hz 递减，60ms 每滴
    G_WavCount1 = SynthWav({ { 880.0, 0.06 } }, 22050, 0.005, 0.04, 0.85);
    G_WavCount2 = SynthWav({ { 660.0, 0.06 } }, 22050, 0.005, 0.04, 0.85);
    G_WavCount3 = SynthWav({ { 440.0, 0.06 } }, 22050, 0.005, 0.04, 0.85);
    // 按钮 hover：1500 Hz 极短 pip（30ms），gain 仅 0.4 保持微妙
    G_WavHover = SynthWav({ { 1500.0, 0.03 } }, 22050, 0.002, 0.025, 0.20);

    // 从 WAV 中提取纯 PCM 样本（跳过 44 字节标准 PCM 头）给 waveOut 用
    auto ExtractPcm = [](const MemWav& wav) -> std::vector<short> {
        if (wav.bytes.size() < 44) return {};
        const short* p = (const short*)(wav.bytes.data() + 44);
        size_t n = (wav.bytes.size() - 44) / 2;
        return std::vector<short>(p, p + n);
    };
    G_ClickSamples = ExtractPcm(G_WavClick);
    G_MoveSamples  = ExtractPcm(G_WavMove);
    G_WinSamples   = ExtractPcm(G_WavWin);
    G_WallSamples  = ExtractPcm(G_WavWall);
    G_DiffSamples  = ExtractPcm(G_WavDiff);
    G_Count1Samples = ExtractPcm(G_WavCount1);
    G_Count2Samples = ExtractPcm(G_WavCount2);
    G_Count3Samples = ExtractPcm(G_WavCount3);
    G_HoverSamples  = ExtractPcm(G_WavHover);

    G_SoundsReady = true;
}

// 异步播放一段 PCM 样本（waveOut 低层 API，绕过 PlaySoundW / WASAPI）
static void PlaySamples(const std::vector<short>& samples,
                        DWORD beepFreq, DWORD beepMs) {
    if (samples.empty()) { Beep(beepFreq, beepMs); return; }
    if (!OpenWaveOut())  { Beep(beepFreq, beepMs); return; }

    WaveBuffer* buf = new WaveBuffer();
    buf->samples    = samples;                         // 拷贝样本，回调释放前都有效
    buf->hdr.lpData          = (LPSTR)buf->samples.data();
    buf->hdr.dwBufferLength  = (DWORD)(buf->samples.size() * 2);
    buf->hdr.dwFlags         = 0;
    buf->hdr.dwLoops         = 0;
    buf->hdr.lpNext          = NULL;
    buf->hdr.reserved        = 0;
    buf->hdr.dwUser          = (DWORD_PTR)buf;          // 回调靠这个找回来 delete

    MMRESULT prepRes = waveOutPrepareHeader(G_hWaveOut, &buf->hdr, sizeof(WAVEHDR));
    if (prepRes != MMSYSERR_NOERROR) { delete buf; Beep(beepFreq, beepMs); return; }

    MMRESULT wrRes = waveOutWrite(G_hWaveOut, &buf->hdr, sizeof(WAVEHDR));
    if (wrRes != MMSYSERR_NOERROR) {
        waveOutUnprepareHeader(G_hWaveOut, &buf->hdr, sizeof(WAVEHDR));
        delete buf;
        Beep(beepFreq, beepMs);
    }
    // 成功路径下，回调 WaveOutProc(WOM_DONE) 会回收 buf
}

static void PlayClickSound()    { PlaySamples(G_ClickSamples,  880,  70); }
// 移动脚步音加 50ms 最小间隔（120ms 音 + 50ms 静音 ≈ 170ms 节流 → 每秒最多 6 声），
// 防止快速连按时多声叠加成碎音
static DWORD G_LastMoveSoundTick = 0;
static void PlayMoveSound() {
    DWORD now = GetTickCount();
    if (now - G_LastMoveSoundTick < 50) return;
    G_LastMoveSoundTick = now;
    PlaySamples(G_MoveSamples,   660,  90);
}
static void PlayWinSound()      { PlaySamples(G_WinSamples,    880, 200); }
static void PlayWallSound()     { PlaySamples(G_WallSamples,   110,  60); }
static void PlayDiffSound()     { PlaySamples(G_DiffSamples,   660,  90); }
static void PlayHoverSound()    { PlaySamples(G_HoverSamples, 1500,  30); }

// 倒计时：n=0/1/2 对应 880/660/440 Hz 三声"滴"
static void PlayCountdownTick(int n) {
    if (n <= 0) PlaySamples(G_Count1Samples, 880, 60);
    else if (n == 1) PlaySamples(G_Count2Samples, 660, 60);
    else PlaySamples(G_Count3Samples, 440, 60);
}

//------------------------------------------------------------------------------
// 方向枚举与迷宫单元
//------------------------------------------------------------------------------
enum Dir { DIR_UP = 0, DIR_RIGHT = 1, DIR_DOWN = 2, DIR_LEFT = 3 };
enum Difficulty { DIFF_EASY = 0, DIFF_MEDIUM = 1, DIFF_HARD = 2 };

struct Cell {
    bool walls[4];
    bool visited;
    Cell() {
        walls[0] = walls[1] = walls[2] = walls[3] = true;
        visited = false;
    }
};

//------------------------------------------------------------------------------
// 玩家移动动画状态
//------------------------------------------------------------------------------
struct PlayerAnim {
    bool active = false;
    int fromR = 0, fromC = 0;
    int toR = 0, toC = 0;
    DWORD startTick = 0;
};

//------------------------------------------------------------------------------
// 自定义按钮状态（owner-drawn，subclass 实现 hover/active 过渡）
//------------------------------------------------------------------------------
struct ModernBtn {
    HWND hwnd;
    WNDPROC oldProc;
    bool accent = false;    // 是否强调色（开始游戏）
    bool hover = false;
    bool pressed = false;
    bool tracking = false;  // 是否正在 TrackMouseEvent
    double hoverT = 0.0;    // 0=idle，1=full hover
    double pressT = 0.0;    // 0=正常，1=pressed
};

//------------------------------------------------------------------------------
// 全局游戏状态
//------------------------------------------------------------------------------
static struct GameState {
    int rows, cols;
    int cellSize;
    int margin;
    int playerRow, playerCol;
    int goalRow, goalCol;
    int steps;
    DWORD startTick;
    DWORD elapsedMs;
    DWORD winTick;
    Difficulty difficulty;
    bool gameActive;
    bool gameWon;
    bool paused;             // 是否暂停
    DWORD pausedAtTick;      // 暂停开始时刻（仅 paused=true 时有意义）
    DWORD pausedTotalMs;     // 累计已暂停的毫秒数（用于从 elapsed 中扣除）
    DWORD lastCountdownSec;  // 通关倒计时最近一次播"滴"的秒数（防重复）
    PlayerAnim anim;
    std::vector<std::vector<Cell>> grid;
    HWND hWnd;
    HFONT hFontTitle, hFontBody, hFontCaption, hFontMono;
    std::vector<ModernBtn> btns;
} G;

//==============================================================================
// 通用工具：UTF-8 → UTF-16
//==============================================================================
static std::wstring W(const char* utf8) {
    if (!utf8 || !*utf8) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len <= 0) return L"";
    std::wstring ws(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &ws[0], len);
    return ws;
}

//==============================================================================
// 迷宫生成（深度优先递归回溯）
//==============================================================================
static void GenerateMaze() {
    G.grid.assign(G.rows, std::vector<Cell>(G.cols));
    std::stack<std::pair<int,int>> stk;
    std::mt19937 rng(static_cast<unsigned>(time(NULL)));

    G.grid[0][0].visited = true;
    stk.push({0, 0});
    const int dR[4] = {-1, 0, 1, 0};
    const int dC[4] = {0, 1, 0, -1};

    while (!stk.empty()) {
        int r = stk.top().first;
        int c = stk.top().second;
        int nbrs[4], nc = 0;
        for (int d = 0; d < 4; ++d) {
            int nr = r + dR[d], nc2 = c + dC[d];
            if (nr >= 0 && nr < G.rows && nc2 >= 0 && nc2 < G.cols && !G.grid[nr][nc2].visited) {
                nbrs[nc++] = d;
            }
        }
        if (nc == 0) {
            stk.pop();
        } else {
            int pick = nbrs[rng() % nc];
            int nr = r + dR[pick], nc2 = c + dC[pick];
            G.grid[r][c].walls[pick] = false;
            G.grid[nr][nc2].walls[(pick + 2) % 4] = false;
            G.grid[nr][nc2].visited = true;
            stk.push({nr, nc2});
        }
    }

    G.playerRow = G.playerCol = 0;
    G.goalRow = G.rows - 1;
    G.goalCol = G.cols - 1;
    G.steps = 0;
    G.startTick = GetTickCount();
    G.elapsedMs = 0;
    G.gameActive = true;
    G.gameWon = false;
    G.paused = false;        // 新游戏清掉上一局的暂停状态
    G.pausedAtTick = 0;
    G.pausedTotalMs = 0;
    G.anim = PlayerAnim();
}

//==============================================================================
// 玩家移动（含碰撞检测 + 缓动动画触发）
//==============================================================================
// 暂停相关辅助（TogglePause / EffectiveNow / GameElapsedMs 在文件下方定义，
// 此处提前声明避免 GetPlayerVisualPos 编译报未声明）
static void TogglePause();
static DWORD EffectiveNow();
static DWORD GameElapsedMs();

static void MovePlayer(Dir d) {
    if (!G.gameActive || G.gameWon) return;
    if (G.paused) return;                                       // 暂停时禁止移动
    if (G.grid[G.playerRow][G.playerCol].walls[d]) {
        PlayWallSound();                     // 撞墙反馈
        return;
    }
    if (G.anim.active) return;                                // 正在动画中，忽略输入

    const int dR[4] = {-1, 0, 1, 0};
    const int dC[4] = {0, 1, 0, -1};
    int nr = G.playerRow + dR[d];
    int nc = G.playerCol + dC[d];

    // 启动缓动动画（用 EffectiveNow 而非 GetTickCount，让 startTick 与暂停感知对齐，
    // 避免上次累积的 pausedTotalMs 让新动画被卡住数秒才能动起来）
    G.anim.fromR = G.playerRow;  G.anim.fromC = G.playerCol;
    G.anim.toR   = nr;            G.anim.toC   = nc;
    G.anim.startTick = EffectiveNow();
    G.anim.active = true;

    // 立即更新逻辑位置（用于步数累计与通关判定）
    G.playerRow = nr;
    G.playerCol = nc;
    G.steps++;

    // 启动 60fps 动画定时器并立刻重绘（不依赖按钮 hover 才开定时器）
    SetTimer(G.hWnd, IDT_ANIM, 16, NULL);
    InvalidateRect(G.hWnd, NULL, FALSE);

    // 播放脚步音：每次成功移动播一次柔和的"嗒"
    PlayMoveSound();

    if (G.playerRow == G.goalRow && G.playerCol == G.goalCol) {
        G.gameWon = true;
        G.elapsedMs = GameElapsedMs();   // 用时排除暂停时长，纯游戏时长
        G.winTick = GetTickCount();
        SetTimer(G.hWnd, IDT_AUTO_REGEN, 200, NULL);
        PlayWinSound();   // 通关提示音（C5-E5-G5-C6 上升琶音）
    }
}

//==============================================================================
// 计算当前动画帧的可视玩家坐标（行、列为 double，用于平滑插值）
//==============================================================================
static void GetPlayerVisualPos(double& outR, double& outC) {
    if (!G.anim.active) {
        outR = G.playerRow;
        outC = G.playerCol;
        return;
    }
    // 暂停感知：anim.startTick 已经在 MovePlayer 中用 EffectiveNow() 设置，
    // 此处直接 EffectiveNow() - startTick 即可。暂停时 EffectiveNow() 返回
    // pausedAtTick（时间冻结），动画进度 t 也冻结；继续后从冻结处继续推进。
    DWORD now = EffectiveNow();
    DWORD elapsed = (now > G.anim.startTick) ? (now - G.anim.startTick) : 0;
    double t = (double)elapsed / PLAYER_ANIM_MS;
    if (t >= 1.0) {
        t = 1.0;
        G.anim.active = false;
    }
    double ease = Theme::EaseOutQuad(t);
    outR = G.anim.fromR + (G.anim.toR - G.anim.fromR) * ease;
    outC = G.anim.fromC + (G.anim.toC - G.anim.fromC) * ease;
}

//==============================================================================
// 启动新游戏
//==============================================================================
static void CancelAutoRegen() { KillTimer(G.hWnd, IDT_AUTO_REGEN); }

static void StartGameWith(Difficulty d) {
    // 切换难度时给一个"叮咚"反馈（首次开默认简单时静默，避免一启动就响）
    if (G.gameActive && G.difficulty != d) PlayDiffSound();

    G.difficulty = d;
    static const int sizes[3] = {15, 25, 35};
    G.rows = G.cols = sizes[d];
    CancelAutoRegen();
    G.lastCountdownSec = 0;             // 重置倒计时音触发记录
    GenerateMaze();
    SetTimer(G.hWnd, IDT_ANIM, 16, NULL);  // 开局即开动画帧，保证按键立刻可动
    InvalidateRect(G.hWnd, NULL, TRUE);
    SetFocus(G.hWnd);
}

//==============================================================================
// 暂停 / 继续（按 P 触发；已通关或未开始时无效）
//==============================================================================
static void TogglePause() {
    if (!G.gameActive || G.gameWon) return;   // 仅在游戏中可暂停
    if (!G.paused) {
        // 进入暂停：记录此刻，后续动画与计时都冻结在 pausedAtTick
        G.pausedAtTick = GetTickCount();
        G.paused = true;
        PlayClickSound();   // 暂停 / 继续都播一下点击音，给玩家明确反馈
    } else {
        // 退出暂停：把本次暂停时长累加到 pausedTotalMs
        G.pausedTotalMs += GetTickCount() - G.pausedAtTick;
        G.paused = false;
        PlayClickSound();
    }
    InvalidateRect(G.hWnd, NULL, FALSE);
}

// 暂停感知的"当前时间"：暂停时返回 pausedAtTick（冻结时间），正常时返回实时 tick
static DWORD EffectiveNow() {
    return G.paused ? G.pausedAtTick : GetTickCount();
}

// 已扣除暂停时长的累计游戏时长（毫秒）
static DWORD GameElapsedMs() {
    DWORD now = EffectiveNow();
    DWORD raw = (now > G.startTick) ? (now - G.startTick) : 0;
    DWORD e   = (raw > G.pausedTotalMs) ? (raw - G.pausedTotalMs) : 0;
    return e;
}

//==============================================================================
// 计算迷宫卡片绘制区域（响应式：根据窗口尺寸计算格子像素 + 卡片位置）
//==============================================================================
static void CalcMazeRect(const RECT& client, RECT& card, RECT& maze) {
    int top    = Theme::PAD_WINDOW_Y + Theme::TOP_BAR_HEIGHT + Theme::HEADER_GAP;
    int bottom = client.bottom - Theme::STATUS_HEIGHT - Theme::PAD_WINDOW_Y;
    int availW = client.right  - 2 * Theme::PAD_WINDOW_X;
    int availH = bottom - top;

    card.left   = Theme::PAD_WINDOW_X;
    card.top    = top;
    card.right  = client.right - Theme::PAD_WINDOW_X;
    card.bottom = bottom;

    // 卡片内留出 20px 内边距
    int innerW = (card.right  - card.left) - 40;
    int innerH = (card.bottom - card.top)  - 40;
    int cs = innerW / G.cols;
    int cs2 = innerH / G.rows;
    if (cs2 < cs) cs = cs2;
    if (cs < Theme::CELL_MIN) cs = Theme::CELL_MIN;
    if (cs > Theme::CELL_MAX) cs = Theme::CELL_MAX;
    G.cellSize = cs;

    int mazeW = cs * G.cols;
    int mazeH = cs * G.rows;
    SetRect(&maze,
        card.left + (card.right  - card.left - mazeW) / 2,
        card.top  + (card.bottom - card.top  - mazeH) / 2,
        0, 0);
    maze.right  = maze.left + mazeW;
    maze.bottom = maze.top  + mazeH;
}

//==============================================================================
// Owner-drawn 按钮 Subclass —— 处理 hover/active 平滑过渡
//==============================================================================
static LRESULT CALLBACK ModernBtnProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    ModernBtn* btn = nullptr;
    for (auto& b : G.btns) if (b.hwnd == h) { btn = &b; break; }
    if (!btn) return DefWindowProc(h, m, w, l);

    switch (m) {
    case WM_MOUSEMOVE: {
        if (!btn->tracking) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            btn->tracking = true;
            btn->hover = true;
            PlayHoverSound();                       // 进入按钮时一个极轻的高频 pip
            // 启动动画定时器（全局 60fps）
            SetTimer(GetParent(h), IDT_ANIM, 16, NULL);
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        btn->tracking = false;
        btn->hover = false;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        btn->pressed = true;
        // 手动捕获鼠标，绕开默认按钮过程在 LBTNDOWN 中调用 SetFocus(h) 抢焦点
        SetCapture(h);
        InvalidateRect(h, NULL, FALSE);
        // 不调用原过程：让按钮彻底不参与焦点管理，焦点始终留在父窗口，
        // 这样点击后玩家按 W 不会被按钮的默认过程吞掉
        return 0;
    }
    case WM_LBUTTONUP: {
        HWND parent = GetParent(h);
        if (GetCapture() == h) ReleaseCapture();
        POINT pt; GetCursorPos(&pt);
        ScreenToClient(h, &pt);
        RECT rc; GetClientRect(h, &rc);
        bool over = PtInRect(&rc, pt);
        btn->pressed = false;
        InvalidateRect(h, NULL, FALSE);
        if (over) {
            // 手动派发 WM_COMMAND / BN_CLICKED 给父窗口（模拟按钮的默认行为）
            SendMessage(parent, WM_COMMAND,
                        MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h);
        }
        return 0;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);

        // 计算过渡颜色
        double ht = btn->hoverT;
        double pt = btn->pressT;

        COLORREF baseIdle    = btn->accent ? Theme::ACCENT        : Theme::BG_BTN_IDLE;
        COLORREF baseHover   = btn->accent ? Theme::ACCENT_HOVER  : Theme::BG_BTN_HOVER;
        COLORREF basePressed = btn->accent ? Theme::ACCENT_PRESSED: Theme::BG_BTN_ACTIVE;
        COLORREF textIdle    = Theme::TEXT_PRIMARY;
        COLORREF textHover   = Theme::TEXT_PRIMARY;

        COLORREF bg     = Theme::Blend(baseIdle, baseHover,   ht);
        COLORREF text   = Theme::Blend(textIdle, textHover,   ht);
        COLORREF border = Theme::Blend(Theme::BG_BORDER,
                                       Theme::Blend(Theme::ACCENT_SOFT, Theme::BG_BORDER, 1.0 - ht),
                                       ht);
        if (pt > 0.01) {
            bg     = Theme::Blend(bg,     basePressed, pt);
            border = Theme::Blend(border, Theme::BG_BORDER, pt);
        }

        int w = rc.right - rc.left, hh = rc.bottom - rc.top;
        int r = Theme::RADIUS_BTN;

        // 先用父窗口背景色把整个客户区铺满（包含按钮本体外圈 1px），
        // 避免 WS_CLIPCHILDREN 下父窗口不重绘按钮区，导致按钮四周露出
        // 系统默认按钮背景（COLOR_BTNFACE，白/浅灰），看起来像一圈"白框"
        HBRUSH bgParent = CreateSolidBrush(Theme::BG_PRIMARY);
        FillRect(hdc, &rc, bgParent);
        DeleteObject(bgParent);

        // 阴影（按钮按下时缩小）
        int shadowLayers = (pt > 0.01) ? 1 : 2;
        for (int i = shadowLayers; i >= 1; --i) {
            int off = 2 + 2 * i / shadowLayers;
            COLORREF sh = Theme::Blend(Theme::BG_DEEP, bg, 0.15);
            Theme::FillRoundRect(hdc, 1 + off, 1 + off, w - 2, hh - 2, r, sh);
        }
        // 按钮本体
        Theme::FillRoundRect(hdc, 1, 1, w - 2, hh - 2, r, bg);
        // 边框
        Theme::StrokeRoundRect(hdc, 1, 1, w - 2, hh - 2, r, border, 1);

        // 文字（按下时下沉 1px 营造触感）
        HFONT font = Theme::FontBody(14, FW_MEDIUM);
        HFONT oldF = (HFONT)SelectObject(hdc, font);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, text);
        RECT trc = { 0, (pt > 0.01) ? 2 : 1, w, hh };
        wchar_t txt[128];
        GetWindowTextW(h, txt, 128);
        DrawTextW(hdc, txt, -1, &trc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
        DeleteObject(font);

        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    // 屏蔽系统的 WM_NCPAINT：BS_FLAT 只去除了 3D 边框，Windows 仍会通过 WM_NCPAINT
    // 画一圈 1px 的细线边框；return 0 让系统彻底不再绘制非客户区
    case WM_NCPAINT:
        return 0;
    }
    return CallWindowProc(btn->oldProc, h, m, w, l);
}

//==============================================================================
// 创建自定义按钮
//==============================================================================
static HWND CreateModernButton(HWND parent, int x, int y, int w, int h, const char* text, int id, bool accent) {
    // BS_FLAT: 去除 3D 边框；BS_OWNERDRAW + 自绘 WM_PAINT 让系统完全不再画默认外观
    HWND hBtn = CreateWindowExW(0, L"BUTTON", W(text).c_str(),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        x, y, w, h, parent, (HMENU)(UINT_PTR)id, GetModuleHandle(NULL), NULL);

    // 关闭该按钮的"键盘焦点提示框"（UISF_HIDEFOCUS）
    SendMessage(hBtn, WM_UPDATEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);

    ModernBtn mb;
    mb.hwnd = hBtn;
    mb.accent = accent;
    mb.oldProc = (WNDPROC)SetWindowLongPtrW(hBtn, GWLP_WNDPROC, (LONG_PTR)ModernBtnProc);
    SetWindowLongPtrW(hBtn, GWLP_USERDATA, (LONG_PTR)&mb);
    G.btns.push_back(mb);
    return hBtn;
}

//==============================================================================
// 主绘制：双缓冲 + 深色主题
//==============================================================================
static void DrawMaze(HDC hdcDst, const RECT& rc) {
    int wBuf = rc.right  - rc.left;
    int hBuf = rc.bottom - rc.top;
    HDC hdc = CreateCompatibleDC(hdcDst);
    HBITMAP hbm = CreateCompatibleBitmap(hdcDst, wBuf, hBuf);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdc, hbm);

    // ========== 1. 底层背景（深色渐变）==========
    Theme::FillGradientH(hdc, 0, 0, wBuf, hBuf, Theme::BG_DEEP, Theme::BG_PRIMARY);

    // ========== 2. 顶部应用品牌区（细线品牌标题）==========
    {
        HFONT lf = Theme::FontCaption(11);
        HFONT oldF = (HFONT)SelectObject(hdc, lf);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TEXT_MUTED);
        RECT trc = { Theme::PAD_WINDOW_X, Theme::PAD_WINDOW_Y - 6, wBuf - Theme::PAD_WINDOW_X, Theme::PAD_WINDOW_Y + 14 };
        DrawTextW(hdc, L"MAZE  ·  v2.0  ·  Dark Theme", -1, &trc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
        DeleteObject(lf);
    }

    // ========== 3. 迷宫卡片（圆角 + 阴影）==========
    RECT cardRc, mazeRc;
    CalcMazeRect(rc, cardRc, mazeRc);
    int cardW = cardRc.right - cardRc.left;
    int cardH = cardRc.bottom - cardRc.top;
    Theme::DrawShadowedCard(hdc, cardRc.left, cardRc.top, cardW, cardH,
                             Theme::RADIUS_CARD, Theme::BG_CARD, Theme::BG_BORDER);

    // ========== 4. 顶部按钮栏（在卡片上方单独绘制，让按钮 owner-draw 与卡片不重叠）==========
    // 按钮已通过 CreateModernButton 创建为子窗口，会由系统绘制在背景之上，这里无需手绘

    if (!G.gameActive) {
        // ========== 5a. 欢迎界面（在卡片中央显示）==========
        SetBkMode(hdc, TRANSPARENT);
        HFONT big = Theme::FontTitle(34, FW_BOLD);
        HFONT oldF = (HFONT)SelectObject(hdc, big);
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        RECT brc = { cardRc.left, cardRc.top + cardH/2 - 80, cardRc.right, cardRc.top + cardH/2 - 10 };
        DrawTextW(hdc, W(TXT_MSG_WELCOME).c_str(), -1, &brc,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
        DeleteObject(big);

        HFONT small = Theme::FontBody(14);
        oldF = (HFONT)SelectObject(hdc, small);
        SetTextColor(hdc, Theme::TEXT_SECONDARY);
        RECT src = { cardRc.left, cardRc.top + cardH/2 + 4, cardRc.right, cardRc.top + cardH/2 + 40 };
        DrawTextW(hdc, W(TXT_MSG_WELCOME_HINT).c_str(), -1, &src,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
        DeleteObject(small);
    } else {
        // ========== 5b. 迷宫本体 ==========
        int ox = mazeRc.left, oy = mazeRc.top;
        int cs = G.cellSize;

        // 通道填充（在卡片内部再次重绘一块平整区域，让迷宫内底色与卡片保持轻微对比）
        RECT inner = { ox - 4, oy - 4, ox + cs * G.cols + 4, oy + cs * G.rows + 4 };
        Theme::FillRoundRect(hdc, inner.left, inner.top, inner.right - inner.left,
                              inner.bottom - inner.top, Theme::RADIUS_CELL, Theme::PATH_FILL);

        // 绘制墙线（带阴影 + 圆角端点）
        HPEN shadowPen = CreatePen(PS_SOLID, Theme::WALL_THICKNESS + 2, Theme::WALL_SHADOW);
        HPEN wallPen   = CreatePen(PS_SOLID, Theme::WALL_THICKNESS,     Theme::WALL);
        HPEN oldPen = (HPEN)SelectObject(hdc, shadowPen);

        // 先画阴影
        for (int r = 0; r < G.rows; ++r) {
            for (int c = 0; c < G.cols; ++c) {
                int x = ox + c * cs;
                int y = oy + r * cs;
                if (G.grid[r][c].walls[DIR_UP]) {
                    MoveToEx(hdc, x + 1, y + 2, NULL);
                    LineTo(hdc, x + cs + 1, y + 2);
                }
                if (G.grid[r][c].walls[DIR_LEFT]) {
                    MoveToEx(hdc, x + 1, y + 1, NULL);
                    LineTo(hdc, x + 1, y + cs + 1);
                }
            }
        }
        SelectObject(hdc, wallPen);
        DeleteObject(shadowPen);

        // 再画主线
        for (int r = 0; r < G.rows; ++r) {
            for (int c = 0; c < G.cols; ++c) {
                int x = ox + c * cs;
                int y = oy + r * cs;
                if (G.grid[r][c].walls[DIR_UP]) {
                    MoveToEx(hdc, x, y, NULL);
                    LineTo(hdc, x + cs, y);
                }
                if (G.grid[r][c].walls[DIR_LEFT]) {
                    MoveToEx(hdc, x, y, NULL);
                    LineTo(hdc, x, y + cs);
                }
            }
        }
        // 最右列与最下行
        for (int r = 0; r < G.rows; ++r) {
            int x = ox + G.cols * cs;
            int y = oy + r * cs;
            if (G.grid[r][G.cols - 1].walls[DIR_RIGHT]) {
                MoveToEx(hdc, x, y, NULL);
                LineTo(hdc, x, y + cs);
            }
        }
        for (int c = 0; c < G.cols; ++c) {
            int x = ox + c * cs;
            int y = oy + G.rows * cs;
            if (G.grid[G.rows - 1][c].walls[DIR_DOWN]) {
                MoveToEx(hdc, x, y, NULL);
                LineTo(hdc, x + cs, y);
            }
        }
        SelectObject(hdc, oldPen);
        DeleteObject(wallPen);

        // 起点方块（带光晕的圆角矩形）
        {
            int sx = ox, sy = oy;
            int pad = (cs >= 16) ? 5 : 3;
            RECT sr = { sx + pad, sy + pad, sx + cs - pad, sy + cs - pad };
            int rw = sr.right - sr.left, rh = sr.bottom - sr.top;
            // 光晕
            Theme::FillRoundRect(hdc, sr.left - 2, sr.top - 2, rw + 4, rh + 4,
                                  Theme::RADIUS_CELL + 2, Theme::Blend(Theme::SUCCESS_SOFT, Theme::BG_CARD, 0.5));
            // 主体
            Theme::FillRoundRect(hdc, sr.left, sr.top, rw, rh, Theme::RADIUS_CELL, Theme::SUCCESS);
            // 高光（左上小三角）
            RECT hl = { sr.left + 2, sr.top + 2, sr.left + rw/2, sr.top + rh/2 };
            Theme::FillRoundRect(hdc, hl.left, hl.top, hl.right - hl.left, hl.bottom - hl.top,
                                  Theme::RADIUS_CELL, Theme::Blend(Theme::SUCCESS, Theme::TEXT_PRIMARY, 0.25));
        }

        // 终点方块（脉冲式警示橙）
        {
            int gx = ox + G.goalCol * cs;
            int gy = oy + G.goalRow * cs;
            int pad = (cs >= 16) ? 5 : 3;
            RECT gr = { gx + pad, gy + pad, gx + cs - pad, gy + cs - pad };
            int rw = gr.right - gr.left, rh = gr.bottom - gr.top;
            // 脉冲光晕（随时间呼吸）
            double pulse = 0.5 + 0.5 * sin(GetTickCount() / 300.0);
            COLORREF glow = Theme::Blend(Theme::BG_CARD, Theme::WARNING_SOFT, 0.4 + 0.4 * pulse);
            Theme::FillRoundRect(hdc, gr.left - 3, gr.top - 3, rw + 6, rh + 6,
                                  Theme::RADIUS_CELL + 2, glow);
            Theme::FillRoundRect(hdc, gr.left, gr.top, rw, rh, Theme::RADIUS_CELL, Theme::WARNING);
            // 高光
            RECT hl = { gr.left + 2, gr.top + 2, gr.left + rw/2, gr.top + rh/2 };
            Theme::FillRoundRect(hdc, hl.left, hl.top, hl.right - hl.left, hl.bottom - hl.top,
                                  Theme::RADIUS_CELL, Theme::Blend(Theme::WARNING, Theme::TEXT_PRIMARY, 0.25));
        }

        // 玩家（圆 + 光晕 + 高光），位置为动画插值结果
        double visR, visC;
        GetPlayerVisualPos(visR, visC);
        {
            int px = ox + (int)(visC * cs);
            int py = oy + (int)(visR * cs);
            int pad = (cs >= 16) ? 4 : 2;
            int cx = px + cs / 2;
            int cy = py + cs / 2;
            int rad = (cs - pad * 2) / 2;

            // 外层光晕（径向渐变）
            int glowR = rad + 6;
            Theme::FillRadial(hdc, cx, cy, glowR,
                Theme::Blend(Theme::PLAYER_GLOW, Theme::BG_CARD, 0.0),
                Theme::Blend(Theme::PLAYER_GLOW, Theme::BG_CARD, 1.0));

            // 主体圆
            HBRUSH b = CreateSolidBrush(Theme::PLAYER_CORE);
            HPEN p = CreatePen(PS_SOLID, 2, Theme::PLAYER_RING);
            HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
            HPEN op = (HPEN)SelectObject(hdc, p);
            Ellipse(hdc, cx - rad, cy - rad, cx + rad, cy + rad);

            // 高光（小圆点）
            int hlR = rad / 3;
            if (hlR > 1) {
                HBRUSH hb = CreateSolidBrush(Theme::Blend(Theme::PLAYER_CORE, Theme::TEXT_PRIMARY, 0.45));
                HBRUSH ohb = (HBRUSH)SelectObject(hdc, hb);
                Ellipse(hdc, cx - rad/2 - hlR/2, cy - rad/2 - hlR/2,
                        cx - rad/2 + hlR/2, cy - rad/2 + hlR/2);
                SelectObject(hdc, ohb);
                DeleteObject(hb);
            }
            SelectObject(hdc, ob);
            SelectObject(hdc, op);
            DeleteObject(b);
            DeleteObject(p);
        }

        // ========== 6. 状态栏胶囊（在卡片外底部）==========
        RECT stRc = {
            Theme::PAD_WINDOW_X,
            rc.bottom - Theme::STATUS_HEIGHT - Theme::PAD_WINDOW_Y + 4,
            rc.right - Theme::PAD_WINDOW_X,
            rc.bottom - Theme::PAD_WINDOW_Y - 4
        };
        int sW = stRc.right - stRc.left;
        int sH = stRc.bottom - stRc.top;
        Theme::FillRoundRect(hdc, stRc.left, stRc.top, sW, sH, Theme::RADIUS_PILL, Theme::BG_PILL);
        Theme::StrokeRoundRect(hdc, stRc.left, stRc.top, sW, sH, Theme::RADIUS_PILL, Theme::BG_BORDER, 1);

        // 状态文字
        std::wstring status;
        COLORREF stColor = Theme::TEXT_SECONDARY;
        if (G.gameWon) {
            int remain = (int)((AUTO_REGEN_DELAY_MS - (GetTickCount() - G.winTick)) / 1000);
            if (remain < 1) remain = 1;
            status = L"\u2605 通关！步数 ";
            status += std::to_wstring(G.steps);
            status += L"，用时 ";
            status += std::to_wstring((int)(G.elapsedMs / 1000));
            status += L" 秒 — ";
            status += std::to_wstring(remain);
            status += L" 秒后自动开始新迷宫 \u2605";
            stColor = Theme::WARNING;
        } else {
            DWORD ms = GameElapsedMs();
            if (G.paused) {
                status = L"\u23F8 已暂停  ·  步数 ";
                status += std::to_wstring(G.steps);
                status += L"  ·  用时 ";
                status += std::to_wstring((int)(ms / 1000));
                status += L" 秒（按 P 继续）";
                stColor = Theme::WARNING;
            } else {
                status = L"游戏中  ·  步数 ";
                status += std::to_wstring(G.steps);
                status += L"  ·  用时 ";
                status += std::to_wstring((int)(ms / 1000));
                status += L" 秒";
                stColor = Theme::TEXT_SECONDARY;
            }
        }
        SetBkMode(hdc, TRANSPARENT);
        HFONT stFont = Theme::FontBody(14, FW_MEDIUM);
        HFONT oldF = (HFONT)SelectObject(hdc, stFont);
        SetTextColor(hdc, stColor);
        RECT trc = { stRc.left + 20, stRc.top, stRc.right - 20, stRc.bottom };
        DrawTextW(hdc, status.c_str(), -1, &trc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
        DeleteObject(stFont);

        // 进度提示（步数 / 总格数）
        int total = G.rows * G.cols;
        std::wstringstream ss;
        ss << L"已移动 " << G.steps << L" 步 / 共 " << total << L" 格";
        std::wstring right = ss.str();
        HFONT rf = Theme::FontMono(12);
        oldF = (HFONT)SelectObject(hdc, rf);
        SetTextColor(hdc, Theme::TEXT_MUTED);
        RECT rrc = { stRc.left + 20, stRc.top, stRc.right - 20, stRc.bottom };
        DrawTextW(hdc, right.c_str(), -1, &rrc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
        DeleteObject(rf);
    }

    // ========== 7. 一次性贴屏（双缓冲）==========
    BitBlt(hdcDst, rc.left, rc.top, wBuf, hBuf, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, hbmOld);
    DeleteObject(hbm);
    DeleteDC(hdc);
}

//==============================================================================
// 菜单
//==============================================================================
static HMENU CreateMenuBar() {
    HMENU hMenuBar = CreateMenu();
    HMENU hGame = CreatePopupMenu();
    AppendMenuW(hGame, MF_STRING, IDM_NEW,   W(TXT_ITEM_NEW).c_str());
    AppendMenuW(hGame, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hGame, MF_STRING, IDM_EXIT,  W(TXT_ITEM_EXIT).c_str());

    HMENU hDiff = CreatePopupMenu();
    AppendMenuW(hDiff, MF_STRING, IDM_EASY,   W(TXT_ITEM_EASY).c_str());
    AppendMenuW(hDiff, MF_STRING, IDM_MEDIUM, W(TXT_ITEM_MEDIUM).c_str());
    AppendMenuW(hDiff, MF_STRING, IDM_HARD,   W(TXT_ITEM_HARD).c_str());

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, IDM_HOWTO, W(TXT_ITEM_HOWTO).c_str());
    AppendMenuW(hHelp, MF_STRING, IDM_ABOUT, W(TXT_ITEM_ABOUT).c_str());

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hGame,  W(TXT_MENU_GAME).c_str());
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hDiff,  W(TXT_MENU_DIFFICULTY).c_str());
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hHelp,  W(TXT_MENU_HELP).c_str());
    return hMenuBar;
}

//==============================================================================
// 创建控件：使用自定义按钮
//==============================================================================
static void CreateControls(HWND hWnd) {
    int y = Theme::PAD_WINDOW_Y + 12;
    int x = Theme::PAD_WINDOW_X;

    CreateModernButton(hWnd, x, y, Theme::BTN_WIDTH_MAIN, Theme::BTN_HEIGHT,
                       TXT_BTN_START, IDB_START, /*accent=*/true);
    x += Theme::BTN_WIDTH_MAIN + Theme::PAD_CONTROL;

    CreateModernButton(hWnd, x, y, Theme::BTN_WIDTH_DIFF, Theme::BTN_HEIGHT,
                       TXT_DIFF_EASY, IDB_EASY, false);
    x += Theme::BTN_WIDTH_DIFF;
    CreateModernButton(hWnd, x, y, Theme::BTN_WIDTH_DIFF, Theme::BTN_HEIGHT,
                       TXT_DIFF_MEDIUM, IDB_MEDIUM, false);
    x += Theme::BTN_WIDTH_DIFF;
    CreateModernButton(hWnd, x, y, Theme::BTN_WIDTH_DIFF, Theme::BTN_HEIGHT,
                       TXT_DIFF_HARD, IDB_HARD, false);
}

//==============================================================================
// 窗口过程
//==============================================================================
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        G.hWnd = hWnd;
        G.hFontTitle   = Theme::FontTitle(22, FW_BOLD);
        G.hFontBody    = Theme::FontBody(14, FW_NORMAL);
        G.hFontCaption = Theme::FontCaption(12);
        G.hFontMono    = Theme::FontMono(13, FW_BOLD);

        CreateControls(hWnd);
        SetTimer(hWnd, IDT_STATUS, 250, NULL);
        // 全局关闭焦点框（包括菜单栏加速键下划线），与深色主题风格统一
        PostMessage(hWnd, WM_UPDATEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS | UISF_HIDEACCEL), 0);
        return 0;
    }

    case WM_TIMER: {
        if (wp == IDT_STATUS) {
            // 状态栏数字节流刷新（250ms）
            RECT rcClient, stRc;
            GetClientRect(hWnd, &rcClient);
            stRc.left   = Theme::PAD_WINDOW_X;
            stRc.top    = rcClient.bottom - Theme::STATUS_HEIGHT - Theme::PAD_WINDOW_Y + 4;
            stRc.right  = rcClient.right - Theme::PAD_WINDOW_X;
            stRc.bottom = rcClient.bottom - Theme::PAD_WINDOW_Y - 4;
            InvalidateRect(hWnd, &stRc, FALSE);
        } else if (wp == IDT_ANIM) {
            // 60fps 动画帧：更新按钮 hover/press 过渡 + 玩家移动 + 终点光晕脉冲
            bool stillNeed = G.anim.active;
            for (auto& b : G.btns) {
                double targetH = b.hover ? 1.0 : 0.0;
                double targetP = b.pressed ? 1.0 : 0.0;
                if (b.hoverT != targetH) {
                    double step = HOVER_ANIM_RATE * (b.hoverT < targetH ? 1.0 : -1.0);
                    b.hoverT += step;
                    if ((step > 0 && b.hoverT > targetH) || (step < 0 && b.hoverT < targetH)) b.hoverT = targetH;
                    InvalidateRect(b.hwnd, NULL, FALSE);
                }
                if (b.pressT != targetP) {
                    double step = HOVER_ANIM_RATE * 2 * (b.pressT < targetP ? 1.0 : -1.0);
                    b.pressT += step;
                    if ((step > 0 && b.pressT > targetP) || (step < 0 && b.pressT < targetP)) b.pressT = targetP;
                    InvalidateRect(b.hwnd, NULL, FALSE);
                }
                if (b.hoverT != 0.0 && b.hoverT != 1.0) stillNeed = true;
                if (b.pressT != 0.0 && b.pressT != 1.0) stillNeed = true;
            }
            // 玩家移动动画 + 终点光晕：触发整窗重绘
            if (G.anim.active || G.gameActive) {
                InvalidateRect(hWnd, NULL, FALSE);
                stillNeed = true;
            }
            if (!stillNeed) KillTimer(hWnd, IDT_ANIM);
            return 0;
        } else if (wp == IDT_AUTO_REGEN) {
            if (G.gameWon && (GetTickCount() - G.winTick) >= (DWORD)AUTO_REGEN_DELAY_MS) {
                KillTimer(hWnd, IDT_AUTO_REGEN);
                GenerateMaze();
                SetTimer(hWnd, IDT_ANIM, 16, NULL);
                InvalidateRect(hWnd, NULL, TRUE);
            } else if (G.gameWon) {
                // 倒计时：每跨过 1 秒播一个递减的"滴"（只播 3、2、1，最后一声落在即将自动开局的瞬间）
                DWORD sec = (GetTickCount() - G.winTick) / 1000;
                if (sec > G.lastCountdownSec && sec <= 3) {
                    PlayCountdownTick((int)sec - 1);   // sec=1→Count1, sec=2→Count2, sec=3→Count3
                    G.lastCountdownSec = sec;
                }
                RECT rcClient, stRc;
                GetClientRect(hWnd, &rcClient);
                stRc.left   = Theme::PAD_WINDOW_X;
                stRc.top    = rcClient.bottom - Theme::STATUS_HEIGHT - Theme::PAD_WINDOW_Y + 4;
                stRc.right  = rcClient.right - Theme::PAD_WINDOW_X;
                stRc.bottom = rcClient.bottom - Theme::PAD_WINDOW_Y - 4;
                InvalidateRect(hWnd, &stRc, FALSE);
            }
            return 0;
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        DrawMaze(hdc, rc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND: return 1;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        // 按钮 / 菜单命令前播放统一的点击音效
        if (id != IDM_EXIT) PlayClickSound();
        switch (id) {
        case IDM_NEW:    case IDB_START:    StartGameWith(G.difficulty);  break;
        case IDM_EASY:   case IDB_EASY:     StartGameWith(DIFF_EASY);    break;
        case IDM_MEDIUM: case IDB_MEDIUM:   StartGameWith(DIFF_MEDIUM);  break;
        case IDM_HARD:   case IDB_HARD:     StartGameWith(DIFF_HARD);    break;
        case IDM_HOWTO:
            MessageBoxW(hWnd, W(TXT_MSG_HOWTO).c_str(), W(TXT_ITEM_HOWTO).c_str(), MB_OK | MB_ICONINFORMATION);
            break;
        case IDM_ABOUT:
            MessageBoxW(hWnd, W(TXT_MSG_ABOUT).c_str(), W(TXT_ITEM_ABOUT).c_str(), MB_OK | MB_ICONINFORMATION);
            break;
        case IDM_EXIT:
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        }
        return 0;
    }

    case WM_KEYDOWN: {
        switch (wp) {
        case VK_UP:    case 'W': MovePlayer(DIR_UP);    break;
        case VK_RIGHT: case 'D': MovePlayer(DIR_RIGHT); break;
        case VK_DOWN:  case 'S': MovePlayer(DIR_DOWN);  break;
        case VK_LEFT:  case 'A': MovePlayer(DIR_LEFT);  break;
        case 'P':                TogglePause();         break;   // 暂停 / 继续
        case VK_RETURN: case 'N': case 'R':
            StartGameWith(G.difficulty);
            break;
        }
        return 0;
    }

    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        // 自定义按钮已自绘，这里拦截默认着色避免系统把按钮画白
        return (LRESULT)GetStockObject(NULL_BRUSH);

    case WM_SIZE: {
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;
    }

    case WM_DESTROY: {
        KillTimer(hWnd, IDT_STATUS);
        KillTimer(hWnd, IDT_ANIM);
        KillTimer(hWnd, IDT_AUTO_REGEN);
        CloseWaveOut();   // 关掉 waveOut 设备，避免退出时回调踩到已释放内存
        if (G.hFontTitle)   DeleteObject(G.hFontTitle);
        if (G.hFontBody)    DeleteObject(G.hFontBody);
        if (G.hFontCaption) DeleteObject(G.hFontCaption);
        if (G.hFontMono)    DeleteObject(G.hFontMono);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

//==============================================================================
// 程序入口
//==============================================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    std::srand(static_cast<unsigned>(std::time(NULL)));
    InitSounds();   // 启动时预生成内存 WAV（一次性）

    G.rows = G.cols = 15;
    G.cellSize = 28;
    G.margin = 20;
    G.playerRow = G.playerCol = 0;
    G.goalRow = G.goalCol = 14;
    G.steps = 0;
    G.startTick = 0;
    G.elapsedMs = 0;
    G.winTick = 0;
    G.difficulty = DIFF_EASY;
    G.gameActive = false;
    G.gameWon = false;
    G.paused = false;
    G.pausedAtTick = 0;
    G.pausedTotalMs = 0;
    G.lastCountdownSec = 0;
    G.anim = PlayerAnim();
    G.grid.clear();
    G.btns.clear();

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"MazeGameWnd";
    wc.hIconSm = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) {
        MessageBoxA(NULL, "注册窗口类失败", "错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    HMENU hMenu = CreateMenuBar();

    int winW = 880, winH = 700;
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    HWND hWnd = CreateWindowExW(0, L"MazeGameWnd", W(TXT_TITLE).c_str(),
        // WS_CLIPCHILDREN 让父窗口的 WM_PAINT 自动跳过子窗口（按钮）区域，
        // 避免 60fps 全窗重绘时父背景覆盖按钮后再被子按钮自绘造成的闪烁
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
        (scrW - winW) / 2, (scrH - winH) / 2, winW, winH,
        NULL, hMenu, hInst, NULL);
    if (!hWnd) {
        MessageBoxA(NULL, "创建窗口失败", "错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}