//==============================================================================
// 迷宫游戏安装器 —— 完整自包含
// 把 Maze.exe + MazeIcon.ico 作为嵌入资源编译进本程序
// 编译: csc /target:winexe /platform:x86 /win32manifest:app.manifest \
//       /out:MazeSetup.exe /resource:Maze.exe,Maze.exe \
//       /resource:MazeIcon.ico,MazeIcon.ico /reference:System.Windows.Forms.dll \
//       /reference:System.Drawing.dll /reference:Microsoft.Win32.Registry.dll \
//       setup.cs
//==============================================================================

using System;
using System.IO;
using System.Reflection;
using System.Diagnostics;
using System.Windows.Forms;
using Microsoft.Win32;

internal static class Program
{
    //------------------------------------------------------------------------------
    // 配置
    //------------------------------------------------------------------------------
    private const string APP_NAME           = "迷宫游戏";
    private const string APP_PUBLISHER      = "Maze";
    private const string APP_VERSION        = "1.0.0";
    private const string RESOURCES_EXE      = "Maze.ExeResources.Maze.exe";
    private const string RESOURCES_ICON     = "Maze.ExeResources.MazeIcon.ico";
    private const string UNINST_BAT_NAME    = "uninstall.cmd";

    private static readonly string DEST_DIR =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Maze");

    private static readonly string STARTMENU_DIR =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "迷宫游戏");

    private static readonly string DESKTOP_DIR =
        Environment.GetFolderPath(Environment.SpecialFolder.Desktop);

    //------------------------------------------------------------------------------
    // 入口
    //------------------------------------------------------------------------------
    [STAThread]
    private static int Main(string[] args)
    {
        bool silent = Array.IndexOf(args, "/S") >= 0 || Array.IndexOf(args, "/silent") >= 0;
        bool hasUI = !silent && Environment.UserInteractive && !IsRedirectedConsole();

        try
        {
            if (hasUI)
            {
                if (!ShowWelcome()) return 0;
            }
            else if (!IsRunAsAdmin())
            {
                // 静默模式下尝试自提权
                try { return RelaunchAsAdmin(args); } catch { /* 失败就继续,后面可能失败 */ }
            }

            DoInstall();

            if (hasUI)
            {
                ShowFinished();
            }
            else
            {
                Console.WriteLine("[OK] 迷宫游戏 已安装到 " + DEST_DIR);
            }
            return 0;
        }
        catch (Exception ex)
        {
            if (hasUI)
            {
                MessageBox.Show(
                    "安装失败:\n\n" + ex.Message +
                    "\n\n请右键本程序,以「以管理员身份运行」重试。",
                    APP_NAME + " 安装程序",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
            else
            {
                Console.Error.WriteLine("[FAIL] " + ex.Message);
            }
            return 1;
        }
    }

    //------------------------------------------------------------------------------
    // 欢迎对话框
    //------------------------------------------------------------------------------
    private static bool ShowWelcome()
    {
        string msg =
            "欢迎使用「迷宫游戏」安装程序。\r\n\r\n" +
            "本程序将执行以下操作:\r\n" +
            "  • 在 " + DEST_DIR + " 安装游戏程序\r\n" +
            "  • 在「开始菜单 > 迷宫游戏」创建快捷方式\r\n" +
            "  • 在桌面创建「迷宫游戏」快捷方式\r\n" +
            "  • 在「控制面板 > 程序和功能」注册卸载入口\r\n\r\n" +
            "需要管理员权限。\r\n\r\n" +
            "确定继续安装,取消退出。";
        return MessageBox.Show(
            msg, APP_NAME + " 安装程序",
            MessageBoxButtons.OKCancel, MessageBoxIcon.Information) == DialogResult.OK;
    }

    private static void ShowFinished()
    {
        string msg =
            "安装完成!\r\n\r\n" +
            "  • 程序文件: " + Path.Combine(DEST_DIR, "Maze.exe") + "\r\n" +
            "  • 开始菜单: 迷宫游戏 \\ 迷宫游戏.lnk\r\n" +
            "  • 桌    面: 迷宫游戏.lnk\r\n" +
            "  • 卸    载: 控制面板 \\ 程序和功能 \\ 迷宫游戏\r\n\r\n" +
            "是否立刻启动游戏?";
        if (MessageBox.Show(msg, APP_NAME + " 安装程序",
                MessageBoxButtons.YesNo, MessageBoxIcon.Information) == DialogResult.Yes)
        {
            try
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName        = Path.Combine(DEST_DIR, "Maze.exe"),
                    WorkingDirectory = DEST_DIR,
                    UseShellExecute = true,
                });
            }
            catch { /* 没启动也不致命 */ }
        }
    }

    //------------------------------------------------------------------------------
    // 安装主体
    //------------------------------------------------------------------------------
    private static void DoInstall()
    {
        Directory.CreateDirectory(DEST_DIR);

        ExtractResource(RESOURCES_EXE,  Path.Combine(DEST_DIR, "Maze.exe"));
        ExtractResource(RESOURCES_ICON, Path.Combine(DEST_DIR, "MazeIcon.ico"));
        WriteUninstallBatch();

        CreateShortcuts();
        RegisterUninstall();
    }

    private static void ExtractResource(string resourceName, string outPath)
    {
        var asm = Assembly.GetExecutingAssembly();
        using (Stream s = asm.GetManifestResourceStream(resourceName))
        {
            if (s == null)
                throw new InvalidOperationException("缺少嵌入资源: " + resourceName);
            using (FileStream fs = File.Create(outPath))
            {
                s.CopyTo(fs);
            }
        }
    }

    private static void WriteUninstallBatch()
    {
        string bat = Path.Combine(DEST_DIR, UNINST_BAT_NAME);
        // 用 .NET PowerShell 调用清理(不依赖 cmd 内嵌复杂转义)
        string ps = Path.Combine(DEST_DIR, "uninstall.ps1");
        File.WriteAllText(ps,
@"$ErrorActionPreference = 'Stop'
$dest = '" + DEST_DIR.Replace("'", "''") + @"'
$progDir = (New-Object -ComObject WScript.Shell).SpecialFolders.Item('AllUsersPrograms')
$folder  = Join-Path $progDir '迷宫游戏'
if (Test-Path -LiteralPath $folder) { Remove-Item -LiteralPath $folder -Recurse -Force }
$desk = [Environment]::GetFolderPath('Desktop')
$dlnk = Join-Path $desk '迷宫游戏.lnk'
if (Test-Path -LiteralPath $dlnk) { Remove-Item -LiteralPath $dlnk -Force }
$key  = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\迷宫游戏'
if (Test-Path $key) { Remove-Item -LiteralPath $key -Recurse -Force }
if (Test-Path -LiteralPath $dest) { cmd /c rd /s /q $dest }
Write-Host '[OK] 已卸载 " + APP_NAME + @"'
");
        File.WriteAllText(bat,
            "@echo off\r\n" +
            "powershell -NoProfile -ExecutionPolicy Bypass -File \"" + ps + "\"\r\n" +
            "exit /b %ERRORLEVEL%\r\n");
    }

    //------------------------------------------------------------------------------
    // 快捷方式(WScript.Shell COM,免引用 COM DLL)
    //------------------------------------------------------------------------------
    private static void CreateShortcuts()
    {
        Type shellType = Type.GetTypeFromProgID("WScript.Shell");
        if (shellType == null) return;
        dynamic shell = Activator.CreateInstance(shellType);

        string exe      = Path.Combine(DEST_DIR, "Maze.exe");
        string iconPath = Path.Combine(DEST_DIR, "MazeIcon.ico");
        string iconLoc  = iconPath + ",0";

        Directory.CreateDirectory(STARTMENU_DIR);

        SaveShortcut(shell, Path.Combine(STARTMENU_DIR, "迷宫游戏.lnk"), exe, iconLoc, DEST_DIR);
        SaveShortcut(shell, Path.Combine(STARTMENU_DIR, "卸载迷宫游戏.lnk"),
            "cmd.exe", iconLoc, DEST_DIR,
            "/c start /min \"\" powershell -NoProfile -ExecutionPolicy Bypass -File \"" +
            Path.Combine(DEST_DIR, "uninstall.ps1") + "\"");
        SaveShortcut(shell, Path.Combine(DESKTOP_DIR, "迷宫游戏.lnk"),   exe, iconLoc, DEST_DIR);
    }

    private static void SaveShortcut(dynamic shell, string lnkPath, string target, string icon, string workDir, string args = null)
    {
        dynamic s = shell.CreateShortcut(lnkPath);
        s.TargetPath       = target;
        s.WorkingDirectory = workDir;
        s.IconLocation     = icon;
        s.WindowStyle      = 1;
        s.Description      = APP_NAME + " - Win32 GDI 深色主题";
        if (args != null) s.Arguments = args;
        s.Save();
    }

    //------------------------------------------------------------------------------
    // 注册表(控制面板「程序和功能」能看到)
    //------------------------------------------------------------------------------
    private static void RegisterUninstall()
    {
        string keyPath = @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\迷宫游戏";
        using (RegistryKey k = Registry.LocalMachine.CreateSubKey(keyPath))
        {
            if (k == null) return;
            string bat = Path.Combine(DEST_DIR, UNINST_BAT_NAME);
            k.SetValue("DisplayName",    APP_NAME,                  RegistryValueKind.String);
            k.SetValue("DisplayIcon",    Path.Combine(DEST_DIR, "MazeIcon.ico") + ",0", RegistryValueKind.String);
            k.SetValue("Publisher",      APP_PUBLISHER,             RegistryValueKind.String);
            k.SetValue("DisplayVersion", APP_VERSION,               RegistryValueKind.String);
            k.SetValue("VersionMajor",   1,                         RegistryValueKind.DWord);
            k.SetValue("VersionMinor",   0,                         RegistryValueKind.DWord);
            k.SetValue("InstallLocation",DEST_DIR,                  RegistryValueKind.String);
            k.SetValue("EstimatedSize",  1371,                      RegistryValueKind.DWord);
            k.SetValue("NoModify",       1,                         RegistryValueKind.DWord);
            k.SetValue("NoRepair",       1,                         RegistryValueKind.DWord);
            k.SetValue("InstallDate",    DateTime.Now.ToString("yyyyMMdd"), RegistryValueKind.String);
            k.SetValue("UninstallString",
                "\"" + bat + "\"",                                   RegistryValueKind.String);
            k.SetValue("QuietUninstallString",
                "\"" + bat + "\" /Q",                                RegistryValueKind.String);
        }
    }

    //------------------------------------------------------------------------------
    // 工具
    //------------------------------------------------------------------------------
    private static bool IsRedirectedConsole()
    {
        try { return !Console.IsOutputRedirected ? false : true; } catch { return false; }
    }

    [System.Runtime.InteropServices.DllImport("shell32.dll")]
    private static extern bool IsUserAnAdmin();

    private static bool IsRunAsAdmin() { try { return IsUserAnAdmin(); } catch { return false; } }

    private static int RelaunchAsAdmin(string[] args)
    {
        var startInfo = new ProcessStartInfo
        {
            UseShellExecute = true,
            FileName        = Assembly.GetExecutingAssembly().Location,
            Verb            = "runas",
            Arguments       = string.Join(" ", args),
        };
        Process.Start(startInfo);
        return 0;
    }
}
