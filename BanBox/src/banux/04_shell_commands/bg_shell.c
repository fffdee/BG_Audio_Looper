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

// LCD console related
static const ShellLCD_t    *g_LCD = NULL;
static bool                 g_ConsoleEnabled = FALSE;
static bool                 g_DbgToLcdEnabled = FALSE;  /* DBG output to LCD */
static char                 g_ConsoleLines[SHELL_CONSOLE_MAX_LINES][SHELL_CONSOLE_LINE_WIDTH + 1];
static uint16_t             g_ConsoleLineColors[SHELL_CONSOLE_MAX_LINES];  /* RGB565 color per line */
static uint8_t              g_ConsoleLineCount = 0;
static uint8_t              g_ConsoleDirty = 0;
static uint16_t             g_ConsoleBlinkTimer = 0;
static uint8_t              g_ConsoleCursorBlink = 0;
static char                 g_ConsoleInputLine[SHELL_CONSOLE_LINE_WIDTH + 1];  /* Current input line */
static uint8_t              g_ConsoleInputLen = 0;
static uint8_t              g_ConsoleInputDirty = 0;

// Console color definitions
#define CONSOLE_BG_COLOR     0x0000  /* Black background */
#define CONSOLE_TEXT_COLOR   0x07E0  /* Green text - for output */
#define CONSOLE_CMD_COLOR    0x07FF  /* Cyan - for commands */
#define CONSOLE_INPUT_COLOR  0xFFFF  /* White - for current input */
#define CONSOLE_TITLE_COLOR  0xFFFF  /* White title */
#define CONSOLE_CURSOR_COLOR 0xFFE0  /* Yellow cursor */
#define CONSOLE_HEADER_COLOR 0x001F  /* Blue header bar */
#define CONSOLE_START_Y      10      /* Text start Y coordinate (after title) */
#define CONSOLE_LINE_HEIGHT  9       /* Line height for 6x8 font */

/* ANSI color to RGB565 mapping */
#define ANSI_COLOR_BLACK     0x0000
#define ANSI_COLOR_RED       0xF800
#define ANSI_COLOR_GREEN     0x07E0
#define ANSI_COLOR_YELLOW    0xFFE0
#define ANSI_COLOR_BLUE      0x001F
#define ANSI_COLOR_MAGENTA   0xF81F
#define ANSI_COLOR_CYAN      0x07FF
#define ANSI_COLOR_WHITE     0xFFFF

/* Line color types */
#define LINE_COLOR_OUTPUT    0       /* Output - green */
#define LINE_COLOR_COMMAND   1       /* Command - cyan */

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

static void Console_AddLine(const char* str);
static void Console_AddLineWithColor(const char* str, uint16_t colorValue);
static void Console_UpdateInputLine(const char* input, uint8_t len);
static void Shell_SendRaw(const char *str);  /* Send to CDC only, no LCD */

/*******************************************************************************
 * Internal command processing
 ******************************************************************************/
static int Opt_HelpAll(int argc, char *argv[]);
static int Opt_HelpMod(int argc, char *argv[]);
static int Opt_List(int argc, char *argv[]);
static int Opt_Version(int argc, char *argv[]);
static int Opt_Clear(int argc, char *argv[]);
static int Opt_IO(int argc, char *argv[]);

// Help module options
static const ShellOpt_t g_HelpOpts[] = {
    OPT("a", "all",     NULL,       "Show all modules",     Opt_HelpAll),
    OPT("m", "module",  "<name>",   "Show module help",     Opt_HelpMod),
    OPT("l", "list",    NULL,       "List by category",     Opt_List),
    OPT("v", "version", NULL,       "Show version",         Opt_Version),
    OPT("c", "clear",   NULL,       "Clear screen",         Opt_Clear),
    OPT("i", "io",      NULL,       "Show current IO",      Opt_IO),

    OPT_END()
};

static const ShellModule_t g_HelpModule = {
    "help", "Help and system info", MOD_CAT_SYSTEM, g_HelpOpts, 6
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

/* Helper function: Parse ANSI color codes and extract RGB565 color */
static uint16_t Parse_ANSI_Color(const char **p)
{
    const char *s = *p;
    int codes[10] = {0};  /* Store multiple codes */
    int code_count = 0;
    int current_code = 0;
    uint16_t color = CONSOLE_TEXT_COLOR;  /* Default color */
    int i;
    
    /* Skip ESC[ */
    s += 2;
    
    /* Parse all codes separated by ';' */
    while (*s && *s != 'm') {
        if (*s >= '0' && *s <= '9') {
            current_code = current_code * 10 + (*s - '0');
        } else if (*s == ';') {
            if (code_count < 10) {
                codes[code_count++] = current_code;
            }
            current_code = 0;
        }
        s++;
    }
    
    /* Store last code */
    if (code_count < 10) {
        codes[code_count++] = current_code;
    }
    
    /* Process codes - look for color codes (30-37 for foreground) */
    for (i = 0; i < code_count; i++) {
        if (codes[i] == 0) color = CONSOLE_TEXT_COLOR;      /* Reset */
        else if (codes[i] == 30) color = ANSI_COLOR_BLACK;
        else if (codes[i] == 31) color = ANSI_COLOR_RED;
        else if (codes[i] == 32) color = ANSI_COLOR_GREEN;
        else if (codes[i] == 33) color = ANSI_COLOR_YELLOW;
        else if (codes[i] == 34) color = ANSI_COLOR_BLUE;
        else if (codes[i] == 35) color = ANSI_COLOR_MAGENTA;
        else if (codes[i] == 36) color = ANSI_COLOR_CYAN;
        else if (codes[i] == 37) color = ANSI_COLOR_WHITE;
        /* Ignore attribute codes like 1 (bold), 4 (underline), etc. */
    }
    
    if (*s == 'm') s++;
    *p = s;
    return color;
}

/* Helper function: Strip ANSI codes and extract color segments */
static void Strip_ANSI_With_Color(const char *src, char *dst, uint16_t maxLen, uint16_t *color)
{
    uint16_t i = 0;
    const char *p = src;
    *color = CONSOLE_TEXT_COLOR;  /* Default color */
    
    while (*p && i < maxLen - 1) {
        if (*p == '\033' && *(p + 1) == '[') {
            /* Parse color and skip escape sequence */
            *color = Parse_ANSI_Color(&p);
        } else {
            /* Normal character, copy it */
            dst[i++] = *p++;
        }
    }
    dst[i] = '\0';
}

/* Print to CDC and LCD console (for command output) */
void Shell_Print(const char *str)
{
    Shell_SendRaw(str);
    /* Also output to LCD console (with ANSI codes parsed for color) */
    if (g_ConsoleEnabled && str) {
        char cleanBuf[256];
        uint16_t color;
        Strip_ANSI_With_Color(str, cleanBuf, sizeof(cleanBuf), &color);
        Console_AddLineWithColor(cleanBuf, color);
    }
}

/* Internal function: Add a line to LCD console buffer with color */
static void Console_AddLineWithColor(const char* str, uint16_t colorValue)
{
    uint8_t i;
    const char* p;
    char* dst;
    uint16_t len;
    uint16_t remaining;
    
    if (!g_ConsoleEnabled || !g_LCD || !str) return;
    
    /* Skip empty strings or strings with only whitespace/newlines */
    p = str;
    while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') p++;
    if (*p == '\0') return;  /* Skip empty lines */
    
    /* Calculate string length (until newline or null) */
    len = 0;
    while (p[len] && p[len] != '\n' && p[len] != '\r') {
        len++;
    }
    
    /* Handle long strings by splitting into multiple lines */
    remaining = len;
    while (remaining > 0) {
        /* If lines are full, scroll all lines up */
        if (g_ConsoleLineCount >= SHELL_CONSOLE_MAX_LINES) {
            for (i = 0; i < SHELL_CONSOLE_MAX_LINES - 1; i++) {
                memcpy(g_ConsoleLines[i], g_ConsoleLines[i + 1], SHELL_CONSOLE_LINE_WIDTH + 1);
                g_ConsoleLineColors[i] = g_ConsoleLineColors[i + 1];
            }
            /* Clear the last line before reusing */
            memset(g_ConsoleLines[SHELL_CONSOLE_MAX_LINES - 1], 0, SHELL_CONSOLE_LINE_WIDTH + 1);
            g_ConsoleLineCount = SHELL_CONSOLE_MAX_LINES - 1;
        }
        
        /* Copy up to SHELL_CONSOLE_LINE_WIDTH characters */
        dst = g_ConsoleLines[g_ConsoleLineCount];
        i = 0;
        while (*p && i < SHELL_CONSOLE_LINE_WIDTH && *p != '\n' && *p != '\r') {
            dst[i++] = *p++;
            remaining--;
        }
        dst[i] = '\0';
        
        /* Add this line if it has content */
        if (i > 0) {
            g_ConsoleLineColors[g_ConsoleLineCount] = colorValue;
            g_ConsoleLineCount++;
            g_ConsoleDirty = 1;
        }
    }
}

/* Internal function: Add a line to LCD console buffer (default output color) */
static void Console_AddLine(const char* str)
{
    Console_AddLineWithColor(str, CONSOLE_TEXT_COLOR);
}

/* Internal function: Update the input line display on LCD (no newline) */
static void Console_UpdateInputLine(const char* input, uint8_t len)
{
    if (!g_ConsoleEnabled || !g_LCD) return;
    
    /* Copy input to buffer */
    if (len > SHELL_CONSOLE_LINE_WIDTH - 2) {  /* Reserve space for "$ " */
        len = SHELL_CONSOLE_LINE_WIDTH - 2;
    }
    memcpy(g_ConsoleInputLine, input, len);
    g_ConsoleInputLine[len] = '\0';
    g_ConsoleInputLen = len;
    g_ConsoleInputDirty = 1;
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
    switch(c)
    {
        case '\r':
        case '\n':
            Shell_SendRaw("\r\n");
            if(g_CmdLen > 0) {
                /* Add command to LCD console with command color before executing */
                Console_AddLineWithColor(g_CmdLine, CONSOLE_CMD_COLOR);
                /* Clear input line */
                Console_UpdateInputLine("", 0);
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
                /* Update LCD input line */
                Console_UpdateInputLine(g_CmdLine, g_CmdLen);
            }
            break;
            
        case 0x03:  // Ctrl+C
            Shell_SendRaw("\r\n");
            g_CmdLen = 0;
            g_CmdLine[0] = '\0';
            /* Clear LCD input line */
            Console_UpdateInputLine("", 0);
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
                /* Update LCD input line */
                Console_UpdateInputLine(g_CmdLine, g_CmdLen);
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
/*******************************************************************************
 * LCD Console Implementation
 ******************************************************************************/

bool Shell_SetLCD(const ShellLCD_t *lcd)
{
    if (!lcd || !lcd->clear || !lcd->fillRect || !lcd->drawString || !lcd->getSize) {
        return FALSE;
    }
    g_LCD = lcd;
    return TRUE;
}

void Shell_ConsoleEnable(bool enable)
{
    if (enable && !g_LCD) {
        return;  /* LCD interface not set, cannot enable */
    }
    
    if (enable && !g_ConsoleEnabled) {
        /* First enable, initialize console */
        uint8_t i;
        for (i = 0; i < SHELL_CONSOLE_MAX_LINES; i++) {
            memset(g_ConsoleLines[i], 0, SHELL_CONSOLE_LINE_WIDTH + 1);
            g_ConsoleLineColors[i] = LINE_COLOR_OUTPUT;
        }
        g_ConsoleLineCount = 0;
        g_ConsoleDirty = 1;
        g_ConsoleBlinkTimer = 0;
        g_ConsoleCursorBlink = 0;
        /* Initialize input line */
        memset(g_ConsoleInputLine, 0, SHELL_CONSOLE_LINE_WIDTH + 1);
        g_ConsoleInputLen = 0;
        g_ConsoleInputDirty = 1;
        
        /* Set enabled flag BEFORE drawing so Console_AddLine works */
        g_ConsoleEnabled = TRUE;
        
        /* Clear screen and draw title bar */
        if (g_LCD) {
            uint16_t w, h;
            g_LCD->getSize(&w, &h);
            g_LCD->clear(CONSOLE_BG_COLOR);
            g_LCD->fillRect(0, 0, w, 12, CONSOLE_HEADER_COLOR);
            g_LCD->drawString(2, 2, "SHELL CONSOLE", CONSOLE_TITLE_COLOR);
        }
        
        /* Show welcome message */
        Console_AddLine("Shell Console Ready");
        Console_AddLine("Enter:Menu Exit:Clear");
    } else {
        g_ConsoleEnabled = enable;
    }
}

bool Shell_ConsoleIsEnabled(void)
{
    return g_ConsoleEnabled;
}

void Shell_ConsoleUpdate(void)
{
    uint8_t i;
    uint16_t y;
    uint16_t w, h;
    uint16_t max_lines;
    uint16_t input_y;
    uint16_t text_color;
    
    if (!g_ConsoleEnabled || !g_LCD) return;
    
    g_LCD->getSize(&w, &h);
    /* Reserve last line for input */
    max_lines = (h - CONSOLE_START_Y) / CONSOLE_LINE_HEIGHT - 1;
    if (max_lines > SHELL_CONSOLE_MAX_LINES) {
        max_lines = SHELL_CONSOLE_MAX_LINES;
    }
    
    /* Calculate input line Y position (bottom of screen) */
    input_y = h - CONSOLE_LINE_HEIGHT - 2;
    
    /* If content updated, redraw */
    if (g_ConsoleDirty) {
        /* Clear text area (not input line) */
        g_LCD->fillRect(0, CONSOLE_START_Y, w, input_y - CONSOLE_START_Y, CONSOLE_BG_COLOR);
        
        /* Display console content with colors */
        y = CONSOLE_START_Y;
        for (i = 0; i < g_ConsoleLineCount && i < max_lines; i++) {
            if (g_ConsoleLines[i][0] != '\0') {
                /* Use stored RGB565 color value */
                text_color = g_ConsoleLineColors[i];
                g_LCD->drawString(0, y, g_ConsoleLines[i], text_color);
            }
            y += CONSOLE_LINE_HEIGHT;
        }
        
        g_ConsoleDirty = 0;
    }
    
    /* If input line updated, redraw input area */
    if (g_ConsoleInputDirty) {
        /* Clear input line area */
        g_LCD->fillRect(0, input_y, w, CONSOLE_LINE_HEIGHT + 2, CONSOLE_BG_COLOR);
        
        /* Draw input prompt and content */
        g_LCD->drawString(0, input_y, "$", CONSOLE_CURSOR_COLOR);
        if (g_ConsoleInputLen > 0) {
            g_LCD->drawString(6, input_y, g_ConsoleInputLine, CONSOLE_INPUT_COLOR);
        }
        g_ConsoleInputDirty = 0;
        g_ConsoleBlinkTimer = 0;  /* Reset blink timer on input change */
        g_ConsoleCursorBlink = 1; /* Show cursor immediately */
    }
    
    /* Cursor blink effect at end of input line */
    g_ConsoleBlinkTimer++;
    if (g_ConsoleBlinkTimer >= 25) {  /* About 500ms blink interval */
        g_ConsoleBlinkTimer = 0;
        g_ConsoleCursorBlink = !g_ConsoleCursorBlink;
        
        /* Draw blinking cursor after input text */
        uint16_t cursor_x = 6 + g_ConsoleInputLen * 6;  /* 6 pixels per char */
        if (cursor_x + 6 <= w) {
            if (g_ConsoleCursorBlink) {
                g_LCD->drawString(cursor_x, input_y, "_", CONSOLE_CURSOR_COLOR);
            } else {
                g_LCD->fillRect(cursor_x, input_y, 6, 8, CONSOLE_BG_COLOR);
            }
        }
    }
}

void Shell_ConsoleClear(void)
{
    uint8_t i;
    uint16_t w, h;
    
    if (!g_ConsoleEnabled || !g_LCD) return;
    
    /* Clear all line buffers */
    for (i = 0; i < SHELL_CONSOLE_MAX_LINES; i++) {
        memset(g_ConsoleLines[i], 0, SHELL_CONSOLE_LINE_WIDTH + 1);
    }
    g_ConsoleLineCount = 0;

    /* Immediately clear the screen area */
    g_LCD->getSize(&w, &h);
    g_LCD->fillRect(0, CONSOLE_START_Y, w, h - CONSOLE_START_Y, CONSOLE_BG_COLOR);

    /* Add cleared message and mark dirty for next update */
    g_ConsoleDirty = 1;
    Console_AddLine("Cleared");
}

void Shell_ConsolePrint(const char* str)
{
    if (g_ConsoleEnabled) {
        Console_AddLine(str);
    }
}

void Shell_ConsolePrintf(const char* fmt, ...)
{
    if (!g_ConsoleEnabled) return;
    
    char buf[SHELL_CONSOLE_LINE_WIDTH + 1];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, SHELL_CONSOLE_LINE_WIDTH + 1, fmt, args);
    va_end(args);
    Console_AddLine(buf);
}

void Shell_DbgToLcdEnable(bool enable)
{
    g_DbgToLcdEnabled = enable;
}

bool Shell_DbgToLcdIsEnabled(void)
{
    return g_DbgToLcdEnabled;
}

void Shell_DbgToLcd(const char* str)
{
    if (g_ConsoleEnabled && g_DbgToLcdEnabled && str) {
        Console_AddLine(str);
    }
}

/*

#define DBG_LCD(format, ...) do { \
    printf(format, ##__VA_ARGS__); \
    if (Shell_DbgToLcdIsEnabled()) { \
        char buf[128]; \
        snprintf(buf, sizeof(buf), format, ##__VA_ARGS__); \
        Shell_DbgToLcd(buf); \
    } \
} while(0)
*/
