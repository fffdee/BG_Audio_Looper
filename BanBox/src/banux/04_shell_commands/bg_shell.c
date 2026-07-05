/**
 *****************************************************************************
 * @file     bg_shell.c
 * @author   BG Card Team
 * @version  V2.0.0
 * @date     16-December-2025
 * @brief    Universal shell command implementation (with input/output console support)
 *****************************************************************************
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "bg_shell.h"
#include "shell_io_ble.h"
#include "debug.h"

/*******************************************************************************
 * Static variables
 ******************************************************************************/
static const ShellModule_t *g_Modules[SHELL_MODULE_MAX];
static uint8_t              g_ModuleCount = 0;

static char                 g_CmdLine[SHELL_CMD_MAX_LEN];
static uint16_t             g_CmdLen = 0;

static char                 g_OutBuf[SHELL_OUT_BUF_SIZE];
static bool                 g_Init = FALSE;
static bool                 g_WelcomeShown = FALSE;

// Current IO interface
static const ShellIO_t     *g_IO = NULL;

/*******************************************************************************
 * Command History
 ******************************************************************************/
#define SHELL_HISTORY_MAX       10      /* 最多记录 10 条命令 */
static char     g_History[SHELL_HISTORY_MAX][SHELL_CMD_MAX_LEN];
static uint8_t  g_HistoryCount = 0;     /* 已存储条数 (0..SHELL_HISTORY_MAX) */
static uint8_t  g_HistoryHead  = 0;     /* 环形写入位置 */
static int8_t   g_HistoryNav   = -1;    /* 上下键浏览位置 (-1=当前输入) */
static char     g_SavedInput[SHELL_CMD_MAX_LEN]; /* 浏览历史时暂存当前输入 */
/* ESC序列状态机: 0=正常, 1=收到ESC, 2=收到ESC[ */
static uint8_t  g_EscState = 0;

static const char *g_CatNames[MOD_CAT_MAX] = {
    "System", "Hardware", "Parameter", "Debug"
};

/*******************************************************************************
 * Static function declarations
 ******************************************************************************/
static void Shell_ProcessChar(char c);
static void Shell_Execute(void);
static int  Shell_ParseArgs(char *line, char *argv[], int max);
static void Shell_Prompt(void);
static void Shell_Welcome(void);
static void Shell_ShowModuleHelp(const ShellModule_t *mod);
static void Shell_HistoryAdd(const char *cmd);
static void Shell_HistoryRecall(int8_t direction);  /* +1=older, -1=newer */

static void Shell_SendRaw(const char *str);

/**
 * @brief  Send raw binary data
 * @param  data: Binary data
 * @param  len: Data length
 */
void Shell_WriteRaw(const uint8_t *data, uint16_t len)
{
    if(data && len > 0 && g_IO && g_IO->send)
    {
        g_IO->send((uint8_t*)data, len);
    }
}

/**
 * @brief  Receive raw binary data from current IO interface (non-blocking)
 */
uint16_t Shell_RecvRaw(uint8_t *buf, uint16_t maxLen)
{
    if(!buf || maxLen == 0 || !g_IO || !g_IO->recv)
        return 0;

    /* Check if data is available first */
    if(g_IO->available)
    {
        if(g_IO->available() == 0)
            return 0;
    }

    return g_IO->recv(buf, maxLen);
}

/*******************************************************************************
 * Internal command processing
 ******************************************************************************/
static int Opt_HelpAll(int argc, char *argv[]);
static int Opt_HelpMod(int argc, char *argv[]);
static int Opt_List(int argc, char *argv[]);
static int Opt_Version(int argc, char *argv[]);
static int Opt_Clear(int argc, char *argv[]);
static int Opt_IO(int argc, char *argv[]);
static int Opt_History(int argc, char *argv[]);

// Help module options
static const ShellOpt_t g_HelpOpts[] = {
    OPT("a", "all",     NULL,       "Show all modules",     Opt_HelpAll),
    OPT("m", "module",  "<name>",   "Show module help",     Opt_HelpMod),
    OPT("l", "list",    NULL,       "List by category",     Opt_List),
    OPT("v", "version", NULL,       "Show version",         Opt_Version),
    OPT("c", "clear",   NULL,       "Clear screen",         Opt_Clear),
    OPT("i", "io",      NULL,       "Show current IO",      Opt_IO),
    OPT("h", "history", NULL,       "Show command history",  Opt_History),

    OPT_END()
};

static const ShellModule_t g_HelpModule = {
    "help", "Help and system info", MOD_CAT_SYSTEM, g_HelpOpts, 7
};

/*******************************************************************************
 * Common functionsMTU
 ******************************************************************************/

bool Shell_Init(void)
{
    if(g_Init) return TRUE;
    
    memset(g_Modules, 0, sizeof(g_Modules));
    g_ModuleCount = 0;
    g_CmdLen = 0;
    g_CmdLine[0] = '\0';
    g_IO = NULL;
    g_WelcomeShown = FALSE;
    
    // Register default help module
    Shell_RegisterModule(&g_HelpModule);
    
    g_Init = TRUE;
    
    return TRUE;
}

bool Shell_SetIO(const ShellIO_t *io)
{
    if(io == NULL || io->send == NULL || io->recv == NULL)
        return FALSE;
    
    g_IO = io;
    g_WelcomeShown = FALSE;  // Reset welcome message after IO switch
    
    return TRUE;
}

const char* Shell_GetIOName(void)
{
    if(g_IO && g_IO->name)
        return g_IO->name;
    return "None";
}

bool Shell_RegisterModule(const ShellModule_t *module)
{
    if(module == NULL || g_ModuleCount >= SHELL_MODULE_MAX)
        return FALSE;
    uint8_t i;
    // Check module name uniqueness
    for(i = 0; i < g_ModuleCount; i++)
    {
        if(strcmp(g_Modules[i]->name, module->name) == 0)
            return FALSE;
    }
    
    g_Modules[g_ModuleCount++] = module;
    return TRUE;
}

void Shell_Process(void)
{
    if(!g_Init || !g_IO) return;
    
    // Show welcome message if not already done
    if(!g_WelcomeShown)
    {
        Shell_Welcome();
        Shell_Prompt();
        g_WelcomeShown = TRUE;
    }
    
    // Read data from IO interface
    uint8_t buf[64];
    uint16_t len = 0;
    uint16_t i;
    if(g_IO->available)
    {
        if(g_IO->available() > 0)
        {
            len = g_IO->recv(buf, sizeof(buf));
        }
    }
    else
    {
        // No available function, read directly
        len = g_IO->recv(buf, sizeof(buf));
    }
    
    // Process received data
    for(i = 0; i < len; i++)
    {
        Shell_ProcessChar((char)buf[i]);
    }
}

void Shell_InputChar(char c)
{
    if(!g_Init) return;
    
    // First input, show welcome message
    if(!g_WelcomeShown && g_IO)
    {
        Shell_Welcome();
        Shell_Prompt();
        g_WelcomeShown = TRUE;
    }
    
    Shell_ProcessChar(c);
}

void Shell_InputData(uint8_t *data, uint16_t len)
{
    if(!g_Init || !data) return;
    uint16_t i;
    for(i = 0; i < len; i++)
    {
        Shell_InputChar((char)data[i]);
    }
}

/* Send to CDC only (for echo, prompt, etc.) - no LCD output */
extern void BLE_ShellEcho(const char *str);
static void Shell_SendRaw(const char *str)
{
    if(str && g_IO && g_IO->send)
    {
        g_IO->send((uint8_t*)str, strlen(str));
//        // 濡傛灉褰撳墠 IO 鏄�BLE锛屽垯閫氳繃 notify 鍥炴樉
//        if(g_IO-00>name && strstr(g_IO->name, "BLE"))
//        {
//            BLE_Send(str);
//        }
    }
}

/* Print to CDC (for command output) */
void Shell_Print(const char *str)
{
    Shell_SendRaw(str);
}

void Shell_Printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_OutBuf, sizeof(g_OutBuf), fmt, args);
    va_end(args);
    
    /* Check if this is a BLE sync command response that needs buffering */
    if (g_is_sync_command && g_IO && g_IO->name && strstr(g_IO->name, "BLE")) {
        BLE_BufferSyncResponse(g_OutBuf, strlen(g_OutBuf));
        DBG("[BLE_SYNC] Response buffered for sync command\n");
    } else {
        Shell_Print(g_OutBuf);  /* Shell_Print already outputs to LCD console */
    }
}

void Shell_NewLine(void)
{
    Shell_SendRaw("\r\n");
}

/*******************************************************************************
 * Static functions
 ******************************************************************************/

static void Shell_ProcessChar(char c)
{
    /* ESC 序列状态机 (处理方向键: ESC [ A/B) */
    if (g_EscState == 1) {
        if (c == '[') { g_EscState = 2; return; }
        g_EscState = 0;  /* 非 '[', 放弃 */
        return;
    }
    if (g_EscState == 2) {
        g_EscState = 0;
        if (c == 'A') { Shell_HistoryRecall(1); return; }  /* Up: 更早的命令 */
        if (c == 'B') { Shell_HistoryRecall(-1); return; } /* Down: 更近的命令 */
        return;  /* 其他 ESC[ 序列忽略 */
    }

    switch(c)
    {
        case 0x1B:  /* ESC */
            g_EscState = 1;
            break;

        case '\r':
        case '\n':
            Shell_SendRaw("\r\n");
            if(g_CmdLen > 0) {
                /* 保存到历史记录 */
                Shell_HistoryAdd(g_CmdLine);
                g_HistoryNav = -1;  /* 重置浏览位置 */
                Shell_Execute();
            }
            Shell_Prompt();
            break;
            
        case '\b':
        case 0x7F:
            if(g_CmdLen > 0)
            {
                g_CmdLen--;
                g_CmdLine[g_CmdLen] = '\0';
                Shell_SendRaw("\b \b");
            }
            break;
            
        case 0x03:  // Ctrl+C
            Shell_SendRaw("\r\n");
            g_CmdLen = 0;
            g_CmdLine[0] = '\0';
            g_HistoryNav = -1;
            Shell_Prompt();
            break;
            
        default:
            if(c >= 0x20 && c < 0x7F && g_CmdLen < SHELL_CMD_MAX_LEN - 1)
            {
                g_CmdLine[g_CmdLen++] = c;
                g_CmdLine[g_CmdLen] = '\0';
                // Echo to CDC only
                char echo[2] = {c, '\0'};
                Shell_SendRaw(echo);
            }
            break;
    }
}

static void Shell_Execute(void)
{
    char *argv[SHELL_CMD_MAX_ARGS];
    int argc = Shell_ParseArgs(g_CmdLine, argv, SHELL_CMD_MAX_ARGS);
    
    if(argc == 0) goto done;
    uint16_t i;
    // Find module
    const ShellModule_t *mod = NULL;
    for(i = 0; i < g_ModuleCount; i++)
    {
        if(strcmp(argv[0], g_Modules[i]->name) == 0)
        {
            mod = g_Modules[i];
            break;
        }
    }
    
    if(mod == NULL)
    {
        Shell_Printf("Unknown module: %s\r\n", argv[0]);
        Shell_Print("Type 'help -a' for available modules\r\n");
        goto done;
    }
    
    // Check if module has a default option (opt == "" means direct command like 'ls')
    const ShellOpt_t *defaultOpt = NULL;
    if(mod->optCount > 0 && mod->options[0].opt != NULL && mod->options[0].opt[0] == '\0')
    {
        defaultOpt = &mod->options[0];
    }
    
    // No option provided
    if(argc < 2)
    {
        // If has default option, call it directly (like 'ls', 'pwd')
        if(defaultOpt && defaultOpt->handler)
        {
            int ret = defaultOpt->handler(0, NULL);
            if(ret != 0)
            {
                Shell_Printf("Error: %d\r\n", ret);
            }
            goto done;
        }
        // Otherwise show module help
        Shell_ShowModuleHelp(mod);
        goto done;
    }
    
    // Parse option
    char *optStr = argv[1];
    
    // If not starting with '-', treat as argument to default option (like 'ls /path', 'cd /dir')
    if(optStr[0] != '-')
    {
        if(defaultOpt && defaultOpt->handler)
        {
            int ret = defaultOpt->handler(argc - 1, &argv[1]);
            if(ret != 0)
            {
                Shell_Printf("Error: %d\r\n", ret);
            }
            goto done;
        }
        // No default option, this is invalid
        Shell_Printf("Invalid option: %s\r\n", optStr);
        Shell_Printf("Use '%s' to see options\r\n", mod->name);
        goto done;
    }
    
    optStr++;
    bool isLong = FALSE;
    if(optStr[0] == '-')
    {
        optStr++;
        isLong = TRUE;
    }

    // Find option
    const ShellOpt_t *opt = NULL;
    for(i = 0; i < mod->optCount; i++)
    {
        if(isLong)
        {
            if(mod->options[i].longOpt && strcmp(optStr, mod->options[i].longOpt) == 0)
            {
                opt = &mod->options[i];
                break;
            }
        }
        else
        {
            if(mod->options[i].opt && strcmp(optStr, mod->options[i].opt) == 0)
            {
                opt = &mod->options[i];
                break;
            }
        }
    }
    
    if(opt == NULL)
    {
        Shell_Printf("Unknown option: %s\r\n", argv[1]);
        Shell_ShowModuleHelp(mod);
        goto done;
    }
    
    // Call handler function
    if(opt->handler)
    {
        int ret = opt->handler(argc - 2, &argv[2]);
        if(ret != 0)
        {
            Shell_Printf("Error: %d\r\n", ret);
        }
    }
    
done:
    g_CmdLen = 0;
    g_CmdLine[0] = '\0';
}

static int Shell_ParseArgs(char *line, char *argv[], int max)
{
    int argc = 0;
    char *p = line;
    
    while(*p && argc < max)
    {
        while(*p == ' ') p++;
        if(*p == '\0') break;
        
        argv[argc++] = p;
        while(*p && *p != ' ') p++;
        if(*p) *p++ = '\0';
    }
    
    return argc;
}

static void Shell_Prompt(void)
{
    Shell_SendRaw("$ ");
}

static void Shell_Welcome(void)
{
    Shell_SendRaw("\r\nBG Card Shell v2.0\r\n");
    Shell_SendRaw("IO:");
    Shell_SendRaw(Shell_GetIOName());
    Shell_SendRaw("\r\n");
    Shell_SendRaw("'help -a' for cmds\r\n");
}

static void Shell_ShowModuleHelp(const ShellModule_t *mod)
{
    Shell_Printf("[%s] %s\r\n", mod->name, mod->desc);
    uint16_t i;
    for(i = 0; i < mod->optCount; i++)
    {
        const ShellOpt_t *opt = &mod->options[i];
        
        Shell_Print(" ");
        if(opt->opt)
        {
            Shell_Printf("-%s", opt->opt);
            if(opt->longOpt) Shell_Print("/");
        }
        if(opt->longOpt)
        {
            Shell_Printf("--%s", opt->longOpt);
        }
        if(opt->args)
        {
            Shell_Printf(" %s", opt->args);
        }
        Shell_Printf(": %s\r\n", opt->help);
    }
}


static int Opt_HelpAll(int argc, char *argv[])
{
    (void)argc; (void)argv;
    uint16_t i;
    Shell_Print("Modules:\r\n");
    for(i = 0; i < g_ModuleCount; i++)
    {
        Shell_Printf(" %s: %s\r\n", g_Modules[i]->name, g_Modules[i]->desc);
    }
    return 0;
}

static int Opt_HelpMod(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Print("Usage: help -m <mod>\r\n");
        return -1;
    }
    uint16_t i;
    for(i = 0; i < g_ModuleCount; i++)
    {
        if(strcmp(argv[0], g_Modules[i]->name) == 0)
        {
            Shell_ShowModuleHelp(g_Modules[i]);
            return 0;
        }
    }

    Shell_Printf("Unknown: %s\r\n", argv[0]);
    return -1;
}

static int Opt_List(int argc, char *argv[])
{
    (void)argc; (void)argv;
    uint16_t i,cat;
    for(cat = 0; cat < MOD_CAT_MAX; cat++)
    {
        bool has = FALSE;
        for(i = 0; i < g_ModuleCount; i++)
        {
            if(g_Modules[i]->category == cat)
            {
                if(!has)
                {
                    Shell_Printf("[%s]\r\n", g_CatNames[cat]);
                    has = TRUE;
                }
                Shell_Printf(" %s: %s\r\n", g_Modules[i]->name, g_Modules[i]->desc);
            }
        }
    }
    return 0;
}

static int Opt_Version(int argc, char *argv[])
{
    (void)argc; (void)argv;

    Shell_Print("BG Card v1.0.0\r\n");
    Shell_Printf("%s %s\r\n", __DATE__, __TIME__);
    Shell_Printf("IO:%s\r\n", Shell_GetIOName());
    return 0;
}

static int Opt_Clear(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("\033[2J\033[H");
    return 0;
}

static int Opt_IO(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Printf("Current IO: %s\r\n", Shell_GetIOName());
    return 0;
}

static int Opt_History(int argc, char *argv[])
{
    uint8_t i, idx;
    (void)argc; (void)argv;

    if (g_HistoryCount == 0) {
        Shell_Printf("(no history)\r\n");
        return 0;
    }

    Shell_Printf("=== Command History (last %u) ===\r\n", g_HistoryCount);
    for (i = 0; i < g_HistoryCount; i++) {
        /* 从最早到最近输出 */
        if (g_HistoryCount >= SHELL_HISTORY_MAX) {
            idx = (g_HistoryHead + i) % SHELL_HISTORY_MAX;
        } else {
            idx = i;
        }
        Shell_Printf("  [%u] %s\r\n", (unsigned)(i + 1), g_History[idx]);
    }
    return 0;
}

/*******************************************************************************
 * Command History Implementation
 ******************************************************************************/

/**
 * @brief  将命令添加到历史记录 (环形缓冲区)
 * @param  cmd: 命令字符串
 */
static void Shell_HistoryAdd(const char *cmd)
{
    if (!cmd || cmd[0] == '\0') return;

    /* 跳过与最近一条完全相同的命令 (避免重复记录) */
    if (g_HistoryCount > 0) {
        uint8_t last = (g_HistoryHead + SHELL_HISTORY_MAX - 1) % SHELL_HISTORY_MAX;
        if (strcmp(g_History[last], cmd) == 0) return;
    }

    strncpy(g_History[g_HistoryHead], cmd, SHELL_CMD_MAX_LEN - 1);
    g_History[g_HistoryHead][SHELL_CMD_MAX_LEN - 1] = '\0';
    g_HistoryHead = (g_HistoryHead + 1) % SHELL_HISTORY_MAX;
    if (g_HistoryCount < SHELL_HISTORY_MAX) g_HistoryCount++;
}

/**
 * @brief  通过上下方向键浏览命令历史
 * @param  direction: +1=向更早的命令(Up), -1=向更近的命令(Down)
 */
static void Shell_HistoryRecall(int8_t direction)
{
    const char *recall;
    uint16_t i;
    uint8_t idx;

    if (g_HistoryCount == 0) return;

    if (direction > 0) {
        /* Up: 向更早的命令 */
        if (g_HistoryNav < 0) {
            /* 首次按Up: 暂存当前输入, 跳到最近一条 */
            strncpy(g_SavedInput, g_CmdLine, SHELL_CMD_MAX_LEN - 1);
            g_SavedInput[SHELL_CMD_MAX_LEN - 1] = '\0';
            g_HistoryNav = 0;
        } else if (g_HistoryNav < (int8_t)(g_HistoryCount - 1)) {
            g_HistoryNav++;
        } else {
            return;  /* 已到最早 */
        }
    } else {
        /* Down: 向更近的命令 */
        if (g_HistoryNav < 0) return;  /* 已在当前输入 */
        g_HistoryNav--;
    }

    /* 获取要回显的内容 */
    if (g_HistoryNav < 0) {
        /* 回到用户原始输入 */
        recall = g_SavedInput;
    } else {
        /* 从环形缓冲区取: nav=0 是最近, nav=count-1 是最早 */
        idx = (g_HistoryHead + SHELL_HISTORY_MAX - 1 - (uint8_t)g_HistoryNav) % SHELL_HISTORY_MAX;
        recall = g_History[idx];
    }

    /* 清除当前行显示 */
    for (i = 0; i < g_CmdLen; i++) {
        Shell_SendRaw("\b \b");
    }

    /* 设置新命令行 */
    strncpy(g_CmdLine, recall, SHELL_CMD_MAX_LEN - 1);
    g_CmdLine[SHELL_CMD_MAX_LEN - 1] = '\0';
    g_CmdLen = (uint16_t)strlen(g_CmdLine);

    /* 回显 */
    Shell_SendRaw(g_CmdLine);
}
