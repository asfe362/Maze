//==============================================================================
// 主题设计规范 ——  深色主题
// 集中管理颜色、字体、间距、圆角等 UI 规范，确保全界面视觉一致
//==============================================================================
#pragma once
#pragma execution_character_set("utf-8")

// MinGW 4.8 部分头文件需要先定义版本才暴露径向渐变 API
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#include <windows.h>
#include <wingdi.h>

// 兼容 MinGW 老版本缺少的常量定义
#ifndef GRADIENT_FILL_RADIAL
#define GRADIENT_FILL_RADIAL 0x00000002
#endif

namespace Theme {

    // ========== 颜色调色板（深色主题）==========
    // 层级：底层背景 → 主背景 → 卡片背景 → 控件背景
    static const COLORREF BG_DEEP        = RGB(14, 16, 26);    // 窗口底层背景
    static const COLORREF BG_PRIMARY     = RGB(22, 26, 42);    // 主背景
    static const COLORREF BG_CARD        = RGB(34, 40, 60);    // 卡片背景（迷宫容器）
    static const COLORREF BG_CARD_HOVER  = RGB(40, 48, 72);    // 卡片悬停
    static const COLORREF BG_BORDER      = RGB(58, 68, 96);    // 边框
    static const COLORREF BG_BTN_IDLE    = RGB(58, 72, 112);   // 按钮默认
    static const COLORREF BG_BTN_HOVER   = RGB(86, 108, 158);  // 按钮悬停
    static const COLORREF BG_BTN_ACTIVE  = RGB(44, 58, 96);    // 按钮按下
    static const COLORREF BG_BTN_DISABLED= RGB(38, 44, 62);    // 按钮禁用
    static const COLORREF BG_PILL        = RGB(28, 32, 50);    // 胶囊/状态栏背景

    // 文字
    static const COLORREF TEXT_PRIMARY   = RGB(232, 238, 250); // 主文字
    static const COLORREF TEXT_SECONDARY = RGB(160, 172, 198); // 次文字
    static const COLORREF TEXT_MUTED     = RGB(110, 122, 148); // 弱化文字
    static const COLORREF TEXT_DISABLED  = RGB(86, 96, 118);   // 禁用文字

    // 强调色
    static const COLORREF ACCENT         = RGB(96, 148, 246);  // 主强调（蓝）
    static const COLORREF ACCENT_HOVER   = RGB(128, 174, 250); // 强调悬停
    static const COLORREF ACCENT_PRESSED = RGB(70, 110, 200);  // 强调按下
    static const COLORREF ACCENT_SOFT    = RGB(46, 70, 120);   // 强调弱化（描边等）

    // 语义色
    static const COLORREF SUCCESS        = RGB(86, 200, 130);  // 成功（起点）
    static const COLORREF SUCCESS_SOFT   = RGB(48, 116, 78);   // 成功弱化
    static const COLORREF WARNING        = RGB(255, 178, 88);  // 警告（终点）
    static const COLORREF WARNING_SOFT   = RGB(140, 96, 50);   // 警告弱化
    static const COLORREF DANGER         = RGB(255, 110, 110); // 危险（错误）

    // 迷宫元素
    static const COLORREF WALL           = RGB(118, 138, 184); // 墙（线）
    static const COLORREF WALL_SHADOW    = RGB(60, 72, 102);   // 墙阴影
    static const COLORREF WALL_GLOW      = RGB(170, 192, 240); // 墙悬停高亮（可拓展）
    static const COLORREF PATH_FILL      = RGB(28, 32, 50);    // 通道填充
    static const COLORREF PLAYER_CORE    = RGB(96, 148, 246);  // 玩家核心
    static const COLORREF PLAYER_RING    = RGB(160, 200, 255); // 玩家环
    static const COLORREF PLAYER_GLOW    = RGB(60, 100, 200);  // 玩家光晕（叠加层）

    // 阴影（半透明模拟用 —— 通过分层叠绘实现）
    static const COLORREF SHADOW_DEEP    = RGB(6, 8, 14);       // 深色阴影（半透明效果通过多层渐变）

    // ========== 字体 ==========
    // 标题字（粗体）
    inline HFONT FontTitle(int size = 22, int weight = FW_BOLD) {
        return CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
            GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    }
    // 正文
    inline HFONT FontBody(int size = 14, int weight = FW_NORMAL) {
        return CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
            GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    }
    // 标签/次要信息
    inline HFONT FontCaption(int size = 12) {
        return CreateFontW(size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    }
    // 等宽（数字/计时）
    inline HFONT FontMono(int size = 13, int weight = FW_BOLD) {
        return CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
            GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
    }
    // 兜底字体（中文系统没装微软雅黑时用系统默认）
    inline HFONT FontFallback(int size = 14) {
        return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    // ========== 度量（间距/圆角/阴影）==========
    static const int PAD_WINDOW_X   = 24;       // 窗口外边距
    static const int PAD_WINDOW_Y   = 20;       // 窗口上下边距
    static const int PAD_CONTROL    = 10;       // 控件之间间距
    static const int RADIUS_BTN     = 10;       // 按钮圆角半径
    static const int RADIUS_CARD    = 18;       // 卡片圆角半径
    static const int RADIUS_CELL    = 6;        // 单元格（起点/终点）圆角
    static const int RADIUS_PILL    = 14;       // 状态栏胶囊圆角
    static const int SHADOW_OFFSET  = 6;        // 卡片阴影偏移
    static const int SHADOW_LAYERS  = 3;        // 阴影层数（模拟模糊）
    static const int BTN_HEIGHT     = 40;       // 按钮高度
    static const int BTN_WIDTH_MAIN = 124;      // 主按钮宽度
    static const int BTN_WIDTH_DIFF = 96;       // 难度按钮宽度
    static const int TOP_BAR_HEIGHT = 56;       // 顶部按钮栏高度
    static const int STATUS_HEIGHT  = 44;       // 状态栏高度
    static const int HEADER_GAP     = 18;       // 顶部栏到迷宫卡片的间距

    // 单元格大小约束（与历史约定一致：2~64）
    static const int CELL_MIN       = 8;        // 实际渲染最小像素
    static const int CELL_MAX       = 48;       // 实际渲染最大像素
    static const int WALL_THICKNESS = 2;        // 墙线粗细

    // ========== 颜色插值（用于动画过渡）==========
    inline COLORREF Blend(COLORREF a, COLORREF b, double t) {
        if (t < 0) t = 0; if (t > 1) t = 1;
        int r  = (int)(GetRValue(a) + (GetRValue(b)  - GetRValue(a))  * t);
        int g  = (int)(GetGValue(a) + (GetGValue(b)  - GetGValue(a))  * t);
        int bl = (int)(GetBValue(a) + (GetBValue(b)  - GetBValue(a))  * t);
        return RGB(r, g, bl);
    }

    // 缓动函数（cubic ease-out，start fast → end slow）
    inline double EaseOut(double t) {
        if (t < 0) t = 0; if (t > 1) t = 1;
        double u = 1.0 - t;
        return 1.0 - u * u * u;
    }

    // 缓动函数（quadratic ease-out）
    inline double EaseOutQuad(double t) {
        if (t < 0) t = 0; if (t > 1) t = 1;
        return 1.0 - (1.0 - t) * (1.0 - t);
    }

    // ========== 绘图辅助 ==========

    // 用纯色填充一个圆角矩形
    inline void FillRoundRect(HDC hdc, int x, int y, int w, int h, int r, COLORREF color) {
        HBRUSH b = CreateSolidBrush(color);
        HPEN p = CreatePen(PS_NULL, 0, 0);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
        HPEN op = (HPEN)SelectObject(hdc, p);
        RoundRect(hdc, x, y, x + w, y + h, r, r);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(b);
        DeleteObject(p);
    }

    // 描边圆角矩形（不填充）
    inline void StrokeRoundRect(HDC hdc, int x, int y, int w, int h, int r, COLORREF color, int width = 1) {
        HPEN pen = CreatePen(PS_SOLID, width, color);
        HBRUSH old = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        HPEN op = (HPEN)SelectObject(hdc, pen);
        RoundRect(hdc, x, y, x + w, y + h, r, r);
        SelectObject(hdc, old);
        SelectObject(hdc, op);
        DeleteObject(pen);
    }

    // 绘制带阴影的卡片（多层半透明叠绘模拟模糊阴影）
    inline void DrawShadowedCard(HDC hdc, int x, int y, int w, int h, int r,
                                  COLORREF cardColor, COLORREF borderColor) {
        // 多层阴影（外深内浅，模拟模糊）
        for (int i = SHADOW_LAYERS; i >= 1; --i) {
            int off = SHADOW_OFFSET * i / SHADOW_LAYERS;
            int alpha = 80 / i;  // 强度递减
            COLORREF shadow = Blend(BG_DEEP, cardColor, (double)alpha / 255.0);
            FillRoundRect(hdc, x + off, y + off, w, h, r, shadow);
        }
        // 卡片本体
        FillRoundRect(hdc, x, y, w, h, r, cardColor);
        // 边框
        StrokeRoundRect(hdc, x, y, w, h, r, borderColor, 1);
    }

    // 简单水平渐变（左→右，从 c1 到 c2）
    inline void FillGradientH(HDC hdc, int x, int y, int w, int h, COLORREF c1, COLORREF c2) {
        TRIVERTEX v[2] = {
            { x, y,       (WORD)((GetRValue(c1) << 8)), (WORD)((GetGValue(c1) << 8)), (WORD)((GetBValue(c1) << 8)), 0xFF00 },
            { x + w, y + h, (WORD)((GetRValue(c2) << 8)), (WORD)((GetGValue(c2) << 8)), (WORD)((GetBValue(c2) << 8)), 0xFF00 }
        };
        GRADIENT_RECT g = {0, 1};
        GradientFill(hdc, v, 2, &g, 1, GRADIENT_FILL_RECT_H);
    }

    // 简单径向渐变（中心亮、外围暗）
    inline void FillRadial(HDC hdc, int cx, int cy, int radius, COLORREF center, COLORREF edge) {
        TRIVERTEX v[2] = {
            { cx - radius, cy - radius,
              (WORD)((GetRValue(center) << 8)), (WORD)((GetGValue(center) << 8)), (WORD)((GetBValue(center) << 8)), 0xFF00 },
            { cx + radius, cy + radius,
              (WORD)((GetRValue(edge)   << 8)), (WORD)((GetGValue(edge)   << 8)), (WORD)((GetBValue(edge)   << 8)), 0xFF00 }
        };
        GRADIENT_RECT g = {0, 1};
        GradientFill(hdc, v, 2, &g, 1, GRADIENT_FILL_RADIAL);
    }

} // namespace Theme