/*
** $Id: lua.c,v 1.230.1.1 2017/04/19 17:29:57 roberto Exp $
** Lua stand-alone interpreter
** See Copyright Notice in lua.h
*/

#define lua_c

#include "lprefix.h"


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


#if !defined(LUA_PROMPT)
#define LUA_PROMPT      "$ "
#define LUA_PROMPT2      "$> "
#endif

#if !defined(LUA_PROGNAME)
#define LUA_PROGNAME        "lua"
#endif

#if !defined(LUA_MAXINPUT)
#define LUA_MAXINPUT        512
#endif

#if !defined(LUA_INIT_VAR)
#define LUA_INIT_VAR        "LUA_INIT"
#endif

#define LUA_INITVARVERSION    LUA_INIT_VAR LUA_VERSUFFIX


/*
** lua_stdin_is_tty detects whether the standard input is a 'tty' (that
** is, whether we're running lua interactively).
*/
#if !defined(lua_stdin_is_tty)    /* { */

#if defined(LUA_USE_POSIX)    /* { */

#include <unistd.h>

#define lua_stdin_is_tty()    isatty(0)

#elif defined(LUA_USE_WINDOWS)    /* }{ */

#include <io.h>
#include <windows.h>

#define lua_stdin_is_tty()	_isatty(_fileno(stdin))

#else				/* }{ */

/* ISO C definition */
#define lua_stdin_is_tty()	1  /* assume stdin is a tty */

#endif                /* } */

#endif                /* } */


/*
** lua_readline defines how to show a prompt and then read a line from
** the standard input.
** lua_saveline defines how to "save" a read line in a "history".
** lua_freeline defines how to free a line read by lua_readline.
*/
#if !defined(lua_readline)    /* { */

#if defined(LUA_USE_READLINE)    /* { */

#include <readline/readline.h>
#include <readline/history.h>

#define lua_readline(L, b, p)    ((void)L, ((b)=readline(p)) != NULL)
#define lua_saveline(L, line)    ((void)L, add_history(line))
#define lua_freeline(L, b)    ((void)L, free(b))

#else				/* }{ */

#define lua_readline(L,b,p) \
        ((void)L, fputs(p, stdout), fflush(stdout),  /* show prompt */ \
        fgets(b, LUA_MAXINPUT, stdin) != NULL)  /* get line */
#define lua_saveline(L,line)	{ (void)L; (void)line; }
#define lua_freeline(L,b)	{ (void)L; (void)b; }

#endif                /* } */

#endif                /* } */


static lua_State *globalL = NULL;

static const char *progname = LUA_PROGNAME;


/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop(lua_State *L, lua_Debug *ar) {
    (void) ar;  /* unused arg. */
    lua_sethook(L, NULL, 0, 0);  /* reset hook */
    luaL_error(L, "interrupted!");
}


/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Lua state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction(int i) {
    signal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
    lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}


static void print_usage(const char *badoption) {
    lua_writestringerror("%s: ", progname);
    if (badoption[1] == 'e' || badoption[1] == 'l')
        lua_writestringerror("'%s' needs argument\n", badoption);
    else
        lua_writestringerror("unrecognized option '%s'\n", badoption);
    lua_writestringerror(
            "usage: %s [options] [script [args]]\n"
            "Available options are:\n"
            "  -e stat  execute string 'stat'\n"
            "  -i       enter interactive mode after executing 'script'\n"
            "  -l name  require library 'name' into global 'name'\n"
            "  -v       show version information\n"
            "  -E       ignore environment variables\n"
            "  --       stop handling options\n"
            "  -        stop handling options and execute stdin\n",
            progname);
}


/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message(const char *pname, const char *msg) {
    if (pname) lua_writestringerror("%s: ", pname);
    lua_writestringerror("%s\n", msg);
}


/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Lua or by 'msghandler'.
*/
static int report(lua_State *L, int status) {
    if (status != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        l_message(progname, msg);
        lua_pop(L, 1);  /* remove message */
    }
    return status;
}


/*
** Message handler used to run all chunks
*/
static int msghandler(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == NULL) {  /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
            lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
            return 1;  /* that is the message */
        else
            msg = lua_pushfstring(L, "(error object is a %s value)",
                                  luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
    return 1;  /* return the traceback */
}


/*
** Interface to 'lua_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall(lua_State *L, int narg, int nres) {
    int status;
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler);  /* push message handler */
    lua_insert(L, base);  /* put it under function and args */
    globalL = L;  /* to be available to 'laction' */
    signal(SIGINT, laction);  /* set C-signal handler */
    status = lua_pcall(L, narg, nres, base);
    signal(SIGINT, SIG_DFL); /* reset C-signal handler */
    lua_remove(L, base);  /* remove message handler from the stack */
    return status;
}


static void print_version(void) {
    lua_writestring(LUA_APART, strlen(LUA_APART));
    lua_writeline();
}


/*
** Create the 'arg' table, which stores all arguments from the
** command line ('argv'). It should be aligned so that, at index 0,
** it has 'argv[script]', which is the script name. The arguments
** to the script (everything after 'script') go to positive indices;
** other arguments (before the script name) go to negative indices.
** If there is no script name, assume interpreter's name as base.
*/
static void createargtable(lua_State *L, char **argv, int argc, int script) {
    int i, narg;
    if (script == argc) script = 0;  /* no script name? */
    narg = argc - (script + 1);  /* number of positive indices */
    lua_createtable(L, narg, script + 1);
    for (i = 0; i < argc; i++) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i - script);
    }
    lua_setglobal(L, "arg");
}


static int dochunk(lua_State *L, int status) {
    if (status == LUA_OK) status = docall(L, 0, 0);
    return report(L, status);
}


static int dofile(lua_State *L, const char *name) {
    return dochunk(L, luaL_loadfile(L, name));
}


static int dostring(lua_State *L, const char *s, const char *name) {
    return dochunk(L, luaL_loadbuffer(L, s, strlen(s), name));
}


/*
** Calls 'require(name)' and stores the result in a global variable
** with the given name.
*/
static int dolibrary(lua_State *L, const char *name) {
    int status;
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    status = docall(L, 1, 1);  /* call 'require(name)' */
    if (status == LUA_OK)
        lua_setglobal(L, name);  /* global[name] = require return */
    return report(L, status);
}


/* ------------------------------------------------------------------------ */

#ifdef LUA_USE_READLINE

#include <ctype.h>

static char *lua_rl_hist;
static int lua_rl_histsize;

static lua_State *lua_rl_L;  /* User data is not passed to rl callbacks. */

/* Reserved keywords. */
static const char *const lua_rl_keywords[] = {
          "and", "break", "do", "else", "elseif", "end", "false",
         "for", "function", "goto", "if", "in", "local", "nil", "not", "or",
         "repeat", "return", "then", "true", "until", "while", NULL
                                                                };

static int valididentifier(const char *s)
{
  if (!(isalpha(*s) || *s == '_')) return 0;
  for (s++; *s; s++) if (!(isalpha(*s) || isdigit(*s) || *s == '_')) return 0;
  return 1;
}

/* Dynamically resizable match list. */
typedef struct {
     char **list;
     size_t idx, allocated, matchlen;
   } dmlist;

/* Add prefix + string + suffix to list and compute common prefix. */
static int lua_rl_dmadd(dmlist *ml, const char *p, size_t pn, const char *s,
			int suf)
{
  char *t = NULL;

  if (ml->idx+1 >= ml->allocated &&
      !(ml->list = realloc(ml->list, sizeof(char *)*(ml->allocated += 32))))
    return -1;

  if (s) {
    size_t n = strlen(s);
    if (!(t = (char *)malloc(sizeof(char)*(pn+n+(suf?2:1))))) return 1;
    memcpy(t, p, pn);
    memcpy(t+pn, s, n);
    n += pn;
    t[n] = suf;
    if (suf) t[++n] = '\0';

    if (ml->idx == 0) {
      ml->matchlen = n;
    } else {
      size_t i;
      for (i = 0; i < ml->matchlen && i < n && ml->list[1][i] == t[i]; i++) ;
      ml->matchlen = i;  /* Set matchlen to common prefix. */
    }
  }

  ml->list[++ml->idx] = t;
  return 0;
}

/* Get __index field of metatable of object on top of stack. */
static int lua_rl_getmetaindex(lua_State *L)
{
  if (!lua_getmetatable(L, -1)) { lua_pop(L, 1); return 0; }
  lua_pushstring(L, "__index");
  lua_rawget(L, -2);
  lua_replace(L, -2);
  if (lua_isnil(L, -1) || lua_rawequal(L, -1, -2)) { lua_pop(L, 2); return 0; }
  lua_replace(L, -2);
  return 1;
}  /* 1: obj -- val, 0: obj -- */

/* Get field from object on top of stack. Avoid calling metamethods. */
static int lua_rl_getfield(lua_State *L, const char *s, size_t n)
{
  int i = 20;  /* Avoid infinite metatable loops. */
  do {
    if (lua_istable(L, -1)) {
      lua_pushlstring(L, s, n);
      lua_rawget(L, -2);
      if (!lua_isnil(L, -1)) { lua_replace(L, -2); return 1; }
      lua_pop(L, 1);
    }
  } while (--i > 0 && lua_rl_getmetaindex(L));
  lua_pop(L, 1);
  return 0;
}  /* 1: obj -- val, 0: obj -- */

/* Completion callback. */
static char **lua_rl_complete(const char *text, int start, int end)
{
  lua_State *L = lua_rl_L;
  dmlist ml;
  const char *s;
  size_t i, n, dot, loop;
  int savetop;

  if (!(text[0] == '\0' || isalpha(text[0]) || text[0] == '_')) return NULL;

  ml.list = NULL;
  ml.idx = ml.allocated = ml.matchlen = 0;

  savetop = lua_gettop(L);
  lua_pushglobaltable(L);
  for (n = (size_t)(end-start), i = dot = 0; i < n; i++)
    if (text[i] == '.' || text[i] == ':') {
      if (!lua_rl_getfield(L, text+dot, i-dot))
	goto error;  /* Invalid prefix. */
      dot = i+1;  /* Points to first char after dot/colon. */
    }

  /* Add all matches against keywords if there is no dot/colon. */
  if (dot == 0)
    for (i = 0; (s = lua_rl_keywords[i]) != NULL; i++)
      if (!strncmp(s, text, n) && lua_rl_dmadd(&ml, NULL, 0, s, ' '))
	goto error;

  /* Add all valid matches from all tables/metatables. */
  loop = 0;  /* Avoid infinite metatable loops. */
  lua_pushglobaltable(L);
  lua_insert(L, -2);
  do {
    if (lua_istable(L, -1) &&
	(loop == 0 || !lua_rawequal(L, -1, -2)))
      for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
	if (lua_type(L, -2) == LUA_TSTRING) {
	  s = lua_tostring(L, -2);
	  /* Only match names starting with '_' if explicitly requested. */
	  if (!strncmp(s, text+dot, n-dot) && valididentifier(s) &&
	      (*s != '_' || text[dot] == '_')) {
	    int suf = ' ';  /* Default suffix is a space. */
	    switch (lua_type(L, -1)) {
	    case LUA_TTABLE:	suf = '.'; break;  /* No way to guess ':'. */
	    case LUA_TFUNCTION:	suf = '('; break;
	    case LUA_TUSERDATA:
	      if (lua_getmetatable(L, -1)) { lua_pop(L, 1); suf = ':'; }
	      break;
	    }
	    if (lua_rl_dmadd(&ml, text, dot, s, suf)) goto error;
	  }
	}
  } while (++loop < 20 && lua_rl_getmetaindex(L));
  lua_pop(L, 1);

  if (ml.idx == 0) {
error:
    lua_settop(L, savetop);
    return NULL;
  } else {
    /* list[0] holds the common prefix of all matches (may be ""). */
    /* If there is only one match, list[0] and list[1] will be the same. */
    if (!(ml.list[0] = (char *)malloc(sizeof(char)*(ml.matchlen+1))))
      goto error;
    memcpy(ml.list[0], ml.list[1], ml.matchlen);
    ml.list[0][ml.matchlen] = '\0';
    /* Add the NULL list terminator. */
    if (lua_rl_dmadd(&ml, NULL, 0, NULL, 0)) goto error;
  }

  lua_settop(L, savetop);
#if RL_READLINE_VERSION >= 0x0600
  rl_completion_suppress_append = 1;
#endif
  return ml.list;
}

/* Initialize readline library. */
static void lua_rl_init(lua_State *L)
{
  char *s;

  lua_rl_L = L;

  /* This allows for $if lua ... $endif in ~/.inputrc. */
  rl_readline_name = "lua";
  /* Break words at every non-identifier character except '.' and ':'. */
  rl_completer_word_break_characters =
    "\t\r\n !\"#$%&'()*+,-/;<=>?@[\\]^`{|}~";
  rl_completer_quote_characters = "\"'";
#if RL_READLINE_VERSION < 0x0600
  rl_completion_append_character = '\0';
#endif
  rl_attempted_completion_function = lua_rl_complete;
  rl_initialize();

  /* Start using history, optionally set history size and load history file. */
  using_history();
  if ((s = getenv("LUA_HISTSIZE")) &&
      (lua_rl_histsize = atoi(s))) stifle_history(lua_rl_histsize);
  if ((lua_rl_hist = getenv("LUA_HISTORY"))) read_history(lua_rl_hist);
}

/* Finalize readline library. */
static void lua_rl_exit(lua_State *L)
{
  (void)L; /* Avoid 'unused parameter' warning. */
  /* Optionally save history file. */
  if (lua_rl_hist) write_history(lua_rl_hist);
}
#else
#define lua_rl_init(L)		((void)L)
#define lua_rl_exit(L)		((void)L)
#endif

/* ------------------------------------------------------------------------ */


/*
** Returns the string to be used as a prompt by the interpreter.
*/
static const char *get_prompt(lua_State *L, int firstline) {
    const char *p;
    lua_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2");
    p = lua_tostring(L, -1);
    if (p == NULL) p = (firstline ? LUA_PROMPT : LUA_PROMPT2);
    return p;
}

/* mark in error messages for incomplete statements */
#define EOFMARK        "<eof>"
#define marklen        (sizeof(EOFMARK)/sizeof(char) - 1)


/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int incomplete(lua_State *L, int status) {
    if (status == LUA_ERRSYNTAX) {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
            lua_pop(L, 1);
            return 1;
        }
    }
    return 0;  /* else... */
}


/*
** Prompt the user, read a line, and push it into the Lua stack.
*/
static int pushline(lua_State *L, int firstline) {
    char buffer[LUA_MAXINPUT];
    char *b = buffer;
    size_t l;
    const char *prmt = get_prompt(L, firstline);
    int readstatus = lua_readline(L, b, prmt);
    if (readstatus == 0)
        return 0;  /* no input (prompt will be popped by caller) */
    lua_pop(L, 1);  /* remove prompt */
    l = strlen(b);
    if (l > 0 && b[l - 1] == '\n')  /* line ends with newline? */
        b[--l] = '\0';  /* remove it */
    if (firstline && b[0] == '=')  /* for compatibility with 5.2, ... */
        lua_pushfstring(L, "return %s", b + 1);  /* change '=' to 'return' */
    else
        lua_pushlstring(L, b, l);
    lua_freeline(L, b);
    return 1;
}


/*
** Try to compile line on the stack as 'return <line>;'; on return, stack
** has either compiled chunk or original line (if compilation failed).
*/
static int addreturn(lua_State *L) {
    const char *line = lua_tostring(L, -1);  /* original line */
    const char *retline = lua_pushfstring(L, "return %s;", line);
    int status = luaL_loadbuffer(L, retline, strlen(retline), "=stdin");
    if (status == LUA_OK) {
        lua_remove(L, -2);  /* remove modified line */
        if (line[0] != '\0')  /* non empty? */
            lua_saveline(L, line);  /* keep history */
    } else
        lua_pop(L, 2);  /* pop result from 'luaL_loadbuffer' and modified line */
    return status;
}


/*
** Read multiple lines until a complete Lua statement
*/
static int multiline(lua_State *L) {
    for (;;) {  /* repeat until gets a complete statement */
        size_t len;
        const char *line = lua_tolstring(L, 1, &len);  /* get what it has */
        int status = luaL_loadbuffer(L, line, len, "=stdin");  /* try it */
        if (!incomplete(L, status) || !pushline(L, 0)) {
            lua_saveline(L, line);  /* keep history */
            return status;  /* cannot or should not try to add continuation line */
        }
        lua_pushliteral(L, "\n");  /* add newline... */
        lua_insert(L, -2);  /* ...between the two lines */
        lua_concat(L, 3);  /* join them */
    }
}


/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement. Return
** the final status of load/call with the resulting function (if any)
** in the top of the stack.
*/
static int loadline(lua_State *L) {
    int status;
    lua_settop(L, 0);
    if (!pushline(L, 1))
        return -1;  /* no input */
    if ((status = addreturn(L)) != LUA_OK)  /* 'return ...' did not work? */
        status = multiline(L);  /* try as command, maybe with continuation lines */
    lua_remove(L, 1);  /* remove line from the stack */
    lua_assert(lua_gettop(L) == 1);
    return status;
}


/*
** Prints (calling the Lua 'print' function) any values on the stack
*/
static void l_print(lua_State *L) {
    int n = lua_gettop(L);
    if (n > 0) {  /* any result to be printed? */
        luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
        lua_getglobal(L, "print");
        lua_insert(L, 1);
        if (lua_pcall(L, n, 0, 0) != LUA_OK)
            l_message(progname, lua_pushfstring(L, "error calling 'print' (%s)",
                                                lua_tostring(L, -1)));
    }
}


/*
** Do the REPL: repeatedly read (load) a line, evaluate (call) it, and
** print any results.
*/
static void doREPL(lua_State *L) {
    int status;
    const char *oldprogname = progname;
    progname = NULL;  /* no 'progname' on errors in interactive mode */
    lua_rl_init(L);
    while ((status = loadline(L)) != -1) {
        if (status == LUA_OK)
            status = docall(L, 0, LUA_MULTRET);
        if (status == LUA_OK) l_print(L);
        else report(L, status);
    }
    lua_settop(L, 0);  /* clear stack */
    lua_writeline();
    lua_rl_exit(L);
    progname = oldprogname;
}


/*
** Push on the stack the contents of table 'arg' from 1 to #arg
*/
static int pushargs(lua_State *L) {
    int i, n;
    if (lua_getglobal(L, "arg") != LUA_TTABLE)
        luaL_error(L, "'arg' is not a table");
    n = (int) luaL_len(L, -1);
    luaL_checkstack(L, n + 3, "too many arguments to script");
    for (i = 1; i <= n; i++)
        lua_rawgeti(L, -i, i);
    lua_remove(L, -i);  /* remove table from the stack */
    return n;
}


static int handle_script(lua_State *L, char **argv) {
    int status;
    const char *fname = argv[0];
    if (strcmp(fname, "-") == 0 && strcmp(argv[-1], "--") != 0)
        fname = NULL;  /* stdin */
    status = luaL_loadfile(L, fname);
    if (status == LUA_OK) {
        int n = pushargs(L);  /* push arguments to script */
        status = docall(L, n, LUA_MULTRET);
    }
    return report(L, status);
}



/* bits of various argument indicators in 'args' */
#define has_error    1    /* bad option */
#define has_i        2    /* -i */
#define has_v        4    /* -v */
#define has_e        8    /* -e */
#define has_E        16    /* -E */

/*
** Traverses all arguments from 'argv', returning a mask with those
** needed before running any Lua code (or an error code if it finds
** any invalid argument). 'first' returns the first not-handled argument
** (either the script name or a bad argument in case of error).
*/
static int collectargs(char **argv, int *first) {
    int args = 0;
    int i;
    for (i = 1; argv[i] != NULL; i++) {
        *first = i;
        if (argv[i][0] != '-')  /* not an option? */
            return args;  /* stop handling options */
        switch (argv[i][1]) {  /* else check option */
            case '-':  /* '--' */
                if (argv[i][2] != '\0')  /* extra characters after '--'? */
                    return has_error;  /* invalid option */
                *first = i + 1;
                return args;
            case '\0':  /* '-' */
                return args;  /* script "name" is '-' */
            case 'E':
                if (argv[i][2] != '\0')  /* extra characters after 1st? */
                    return has_error;  /* invalid option */
                args |= has_E;
                break;
            case 'i':
                args |= has_i;  /* (-i implies -v) *//* FALLTHROUGH */
            case 'v':
                if (argv[i][2] != '\0')  /* extra characters after 1st? */
                    return has_error;  /* invalid option */
                args |= has_v;
                break;
            case 'e':
                args |= has_e;  /* FALLTHROUGH */
            case 'l':  /* both options need an argument */
                if (argv[i][2] == '\0') {  /* no concatenated argument? */
                    i++;  /* try next 'argv' */
                    if (argv[i] == NULL || argv[i][0] == '-')
                        return has_error;  /* no next argument or it is another option */
                }
                break;
            default:  /* invalid option */
                return has_error;
        }
    }
    *first = i;  /* no script name */
    return args;
}


/*
** Processes options 'e' and 'l', which involve running Lua code.
** Returns 0 if some code raises an error.
*/
static int runargs(lua_State *L, char **argv, int n) {
    int i;
    for (i = 1; i < n; i++) {
        int option = argv[i][1];
        lua_assert(argv[i][0] == '-');  /* already checked */
        if (option == 'e' || option == 'l') {
            int status;
            const char *extra = argv[i] + 2;  /* both options need an argument */
            if (*extra == '\0') extra = argv[++i];
            lua_assert(extra != NULL);
            status = (option == 'e')
                     ? dostring(L, extra, "=(command line)")
                     : dolibrary(L, extra);
            if (status != LUA_OK) return 0;
        }
    }
    return 1;
}


static int handle_luainit(lua_State *L) {
    const char *name = "=" LUA_INITVARVERSION;
    const char *init = getenv(name + 1);
    if (init == NULL) {
        name = "=" LUA_INIT_VAR;
        init = getenv(name + 1);  /* try alternative name */
    }
    if (init == NULL) return LUA_OK;
    else if (init[0] == '@')
        return dofile(L, init + 1);
    else
        return dostring(L, init, name);
}


/*
** Main body of stand-alone interpreter (to be called in protected mode).
** Reads the options and handles them all.
*/
static int pmain(lua_State *L) {
    int argc = (int) lua_tointeger(L, 1);
    char **argv = (char **) lua_touserdata(L, 2);
    int script;
    int args = collectargs(argv, &script);
    luaL_checkversion(L);  /* check that interpreter has correct version */
    if (argv[0] && argv[0][0]) progname = argv[0];
    if (args == has_error) {  /* bad arg? */
        print_usage(argv[script]);  /* 'script' has index of bad arg. */
        return 0;
    }
    if (args & has_v)  /* option '-v'? */
        print_version();
    if (args & has_E) {  /* option '-E'? */
        lua_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
        lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
    }
    luaL_openlibs(L);  /* open standard libraries */
    createargtable(L, argv, argc, script);  /* create table 'arg' */
    if (!(args & has_E)) {  /* no option '-E'? */
        if (handle_luainit(L) != LUA_OK)  /* run LUA_INIT */
            return 0;  /* error running LUA_INIT */
    }
    if (!runargs(L, argv, script))  /* execute arguments -e and -l */
        return 0;  /* something failed */
    if (script < argc &&  /* execute main script (if there is one) */
        handle_script(L, argv + script) != LUA_OK)
        return 0;
    if (args & has_i)  /* -i option? */
        doREPL(L);  /* do read-eval-print loop */
    else if (script == argc && !(args & (has_e | has_v))) {  /* no arguments? */
        if (lua_stdin_is_tty()) {  /* running in interactive mode? */
            print_version();
            doREPL(L);  /* do read-eval-print loop */
        } else dofile(L, NULL);  /* executes stdin as a file */
    }
    lua_pushboolean(L, 1);  /* signal no errors */
    return 1;
}


int main(int argc, char **argv) {
    int status, result;
    lua_State *L = luaL_newstate();  /* create state */
    if (L == NULL) {
        l_message(argv[0], "cannot create state: not enough memory");
        return EXIT_FAILURE;
    }
    lua_pushcfunction(L, &pmain);  /* to call 'pmain' in protected mode */
    lua_pushinteger(L, argc);  /* 1st argument */
    lua_pushlightuserdata(L, argv); /* 2nd argument */
    status = lua_pcall(L, 2, 1, 0);  /* do the call */
    result = lua_toboolean(L, -1);  /* get result */
    report(L, status);
    lua_close(L);
    return (result && status == LUA_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

