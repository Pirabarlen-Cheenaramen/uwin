/* FvwmBacker Module for Fvwm.
 *
 *  Copyright 1994,  Mike Finger (mfinger@mermaid.micro.umn.edu or
 *                               Mike_Finger@atk.com)
 *
 * The author makes not guarantees or warantees, either express or
 * implied.  Feel free to use any contained here for any purpose, as long
 * and this and any other applicible copyrights are kept intact.

 * The functions in this source file that are based on part of the FvwmIdent
 * module for Fvwm are noted by a small copyright atop that function, all others
 * are copyrighted by Mike Finger.  For those functions modified/used, here is
 *  the full, origonal copyright:
 *
 * Copyright 1994, Robert Nation and Nobutaka Suzuki.
 * No guarantees or warantees or anything
 * are provided or implied in any way whatsoever. Use this program at your
 * own risk. Permission to use this program for any purpose is given,
 * as long as the copyright is kept intact. */

/* Modified to directly manipulate the X server if a solid color
 * background is requested. To use this, use "-solid <color_name>"
 * as the command to be executed.
 *
 * A. Davison
 * Septmber 1994.
 */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <stdio.h>
#include <signal.h>
#include <string.h>

#if HAVE_SYS_BSDTYPES_H
#include <sys/bsdtypes.h> /* Saul */
#endif /* Saul */

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include "libs/FShape.h"
#include "libs/Module.h"
#include "libs/Colorset.h"
#include "libs/Parse.h"
#include "FvwmBacker.h"

/* migo (22-Nov-1999): Temporarily until fvwm_msg is moved to libs */
#define ERR
#define fvwm_msg(t,l,f) fprintf(stderr, "[FVWM][FvwmBacker]: <<ERROR>> %s\n", f)

unsigned long BackerGetColor(char *color);

#define EXEC_CHANGED_PAGE  0x01
#define EXEC_CHANGED_DESK  0x02
#define EXEC_ALWAYS        0x04

struct Command
{
  int desk;
  struct
  {
    unsigned do_ignore_desk : 1;
    unsigned do_ignore_page : 1;
    unsigned do_match_any_desk : 1;
  } flags;
  int x;	/* Page X coordinate; -1 for glob */
  int y;	/* Page Y coordinate; -1 for glob */
  /* The command type:
   * -1 = no command
   *  0 = command to be spawned
   *  1 = use a solid color background
   *  2 = use a colorset background
   */
  int type;
  char*	cmdStr; /* The command string (Type 0)   */
  unsigned long solidColor; /* A solid color after X parsing (Type 1) */
  int colorset; /* The colorset to be used (Type 2) */
  struct Command *next;
};

typedef struct Command Command;

typedef struct
{
  Command *first;
  Command *last;
} CommandChain;

CommandChain *commands;

int current_desk = 0;
int current_x = 0;
int current_y = 0;
int current_colorset = -1;  /* the last matched command colorset or -1 */

int Fvwm_fd[2];
int fd_width;

char *Module;        /* i.e. "FvwmBacker" */
char *configPrefix;  /* i.e. "*FvwmBacker" */


/* X Display information. */

Display* 	dpy;
Window		root;
int			screen;
int MyDisplayHeight;
int MyDisplayWidth;


int main(int argc, char **argv)
{
  char *temp, *s;
  char* displayName = NULL;

  commands = (CommandChain*)safemalloc(sizeof(CommandChain));
  commands->first = commands->last = NULL;

  /* Save the program name for error messages and config parsing */
  temp = argv[0];
  s = strrchr(argv[0], '/');
  if (s != NULL)
    temp = s + 1;

  Module = temp;
  configPrefix = CatString2("*", Module);

  if ((argc != 6) && (argc != 7))
  {
    fprintf(stderr, "%s Version %s should only be executed by fvwm!\n", Module,
	    VERSION);
    exit(1);
  }

  Fvwm_fd[0] = atoi(argv[1]);
  Fvwm_fd[1] = atoi(argv[2]);

  /* Grab the X display information now. */

  dpy = XOpenDisplay(displayName);
  if (!dpy)
  {
    fprintf(stderr, "%s:  unable to open display '%s'\n",
	    Module, XDisplayName (displayName));
    exit (2);
  }
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  MyDisplayHeight = DisplayHeight(dpy, screen);
  MyDisplayWidth = DisplayWidth(dpy, screen);

  /* allocate default colorset */
  AllocColorset(0);
  FShapeInit(dpy);

  signal (SIGPIPE, DeadPipe);

  /* Parse the config file */
  ParseConfig();

  fd_width = GetFdWidth();

  SetMessageMask(Fvwm_fd,
    M_NEW_PAGE | M_NEW_DESK | M_CONFIG_INFO | M_END_CONFIG_INFO | M_SENDCONFIG);

  /*
  ** we really only want the current desk/page, and window list sends it
  */
  SendInfo(Fvwm_fd, "Send_WindowList", 0);

  /* tell fvwm we're running */
  SendFinishedStartupNotification(Fvwm_fd);

  /* Recieve all messages from Fvwm */
  EndLessLoop();

  /* Should never get here! */
  return 1;
}

/******************************************************************************
  EndLessLoop - Read until we get killed, blocking when can't read
******************************************************************************/
void EndLessLoop()
{
  while(1)
  {
    ReadFvwmPipe();
  }
}

/******************************************************************************
  ReadFvwmPipe - Read a single message from the pipe from Fvwm
******************************************************************************/
void ReadFvwmPipe()
{
  FvwmPacket* packet = ReadFvwmPacket(Fvwm_fd[1]);
  if ( packet == NULL )
    exit(0);
  else
    ProcessMessage( packet->type, packet->body );
}


void SetDeskPageBackground(const Command *c)
{
  current_colorset = -1;
  switch (c->type)
  {
  case 1:
    /* Process a solid color request */
    XSetWindowBackground(dpy, root, c->solidColor);
    XClearWindow(dpy, root);
    XFlush(dpy);
    break;

  case 2:
    current_colorset = c->colorset;
    SetWindowBackground(
      dpy, root, MyDisplayWidth, MyDisplayHeight, &Colorset[c->colorset],
      DefaultDepth(dpy, screen), DefaultGC(dpy, screen), True);
    XFlush(dpy);
    break;

  case 0:
  case -1:
  default:
    if(c->cmdStr != NULL)
    {
      SendFvwmPipe(Fvwm_fd, c->cmdStr, (unsigned long)0);
    }
    break;
  }
}

/*
 * migo (23-Nov-1999): Maybe execute only first (or last?) matching command?
 * migo (03-Apr-2001): Yes, execute only the last matching command.
 */
void ExecuteMatchingCommands(int colorset, int changed)
{
  const Command *matching_command = NULL;
  const Command *command;

  if (!changed)
    return;
  for (command = commands->first; command; command = command->next)
  {
    if (!(changed & EXEC_ALWAYS) &&
	(command->flags.do_ignore_desk || !(changed & EXEC_CHANGED_DESK)) &&
	(command->flags.do_ignore_page || !(changed & EXEC_CHANGED_PAGE)))
    {
      continue;
    }
    if ((command->flags.do_match_any_desk || command->desk == current_desk) &&
	(command->x < 0 || command->x == current_x) &&
	(command->y < 0 || command->y == current_y) &&
	(colorset < 0 || (colorset == current_colorset &&
			  colorset == command->colorset)))
    {
      matching_command = command;
    }
  }
  if (matching_command)
    SetDeskPageBackground(matching_command);
}

/******************************************************************************
  ProcessMessage - Process the message coming from Fvwm
******************************************************************************/
void ProcessMessage(unsigned long type, unsigned long *body)
{
  char *tline;
  int change = 0;
  static Bool is_initial = True;

  switch (type)
  {
  case M_CONFIG_INFO:
    tline = (char*)&(body[3]);
    ExecuteMatchingCommands(ParseConfigLine(tline), EXEC_ALWAYS);
    break;

  case M_NEW_DESK:
    if (is_initial)
    {
      change = EXEC_ALWAYS;
      is_initial = False;
    }
    else if (current_desk != body[0])
    {
      current_desk = body[0];
      change |= EXEC_CHANGED_DESK;
    }
    ExecuteMatchingCommands(-1, change);
    break;

  case M_NEW_PAGE:
    if (is_initial)
    {
      change = EXEC_ALWAYS;
      is_initial = False;
    }
    if (current_desk != body[2])
    {
      current_desk = body[2];
      change |= EXEC_CHANGED_DESK;
    }
    if (current_x != body[0] / MyDisplayWidth ||
	current_y != body[1] / MyDisplayHeight)
    {
      current_x = body[0] / MyDisplayWidth;
      current_y = body[1] / MyDisplayHeight;
      change |= EXEC_CHANGED_PAGE;
    }
    ExecuteMatchingCommands(-1, change);
    break;

  }
}

/******************************************************************************
  Detected a broken pipe - time to exit
******************************************************************************/
void DeadPipe(int nonsense)
{
  exit(1);
}

/******************************************************************************
  ParseConfigLine - Parse the configuration line fvwm to us to use
******************************************************************************/
int ParseConfigLine(char *line)
{
  if (strlen(line) > 1)
  {
    if (strncasecmp(line, configPrefix, strlen(configPrefix)) == 0)
      AddCommand(line + strlen(configPrefix));
    else if (strncasecmp(line, "colorset", 8) == 0)
      return LoadColorset(line + 8);
  }
  return -1;
}

/******************************************************************************
  ParseConfig - Parse the configuration file fvwm to us to use
******************************************************************************/
void ParseConfig()
{
  char *line_start;
  char *tline;

  line_start = safemalloc(strlen(Module) + 2);
  strcpy(line_start, "*");
  strcat(line_start, Module);

  InitGetConfigLine(Fvwm_fd, line_start);
  GetConfigLine(Fvwm_fd, &tline);

  while(tline != (char *)0)
  {
    ParseConfigLine(tline);
    GetConfigLine(Fvwm_fd, &tline);
  }
  free(line_start);
}

/******************************************************************************
  AddCommand - Add a command to the correct spot on the dynamic array.
******************************************************************************/
void AddCommand(char *line)
{
  char *token;
  Command *this;
  Bool do_ignore_desk = True;
  Bool do_ignore_page = True;

  this = (Command*)safemalloc(sizeof(Command));
  this->desk = 0;
  memset(&(this->flags), 0, sizeof(this->flags));
  this->flags.do_match_any_desk = 1;
  this->x = -1;
  this->y = -1;
  this->type = -1;
  this->cmdStr = NULL;
  this->next = NULL;

  if (strncasecmp(line, "Desk", 4) == 0)
  {
    /* Old command style */

    line += 4;
    line = GetNextToken(line, &token);
    if (strcasecmp(token, "*") != 0)
    {
      this->flags.do_match_any_desk = 0;
      this->desk = atoi(token);
    }
    do_ignore_desk = False;
    free(token);
  }
  else if (strncasecmp(line, "Command", 7) == 0)
  {
    /* New command style */

    line += 7;
    while (*line && isspace(*line)) line++;
    if (*line == '(')
    {
      char *parens, *option;
      char *parens_run;
      line++;
      line = GetQuotedString(line, &parens, ")", NULL, NULL, NULL);
      if (line == NULL)
      {
	fvwm_msg(ERR, "FvwmBacker", "Syntax error: no closing brace");
	return;
      }
      parens_run = parens;

      while (*parens_run &&
	(parens_run = GetNextFullOption(parens_run, &option)) != NULL)
      {
	char *name, *value;
	char *option_val;

	if (!*option)
	{
	  free(option);
	  break;
	}
	option_val = GetNextToken(option, &name);

	if (StrEquals(name, "Desk")) {
	  if (GetNextToken(option_val, &value) == NULL)
	  {
	    fvwm_msg(ERR, "FvwmBacker", "Desk option requires a value");
	    return;
	  }
	  if (!StrEquals(value, "*"))
	  {
	    this->flags.do_match_any_desk = 0;
	    this->desk = atoi(value);
	  }
	  do_ignore_desk = False;
	  free(value);
	}
	else

	if (StrEquals(name, "Page")) {
	  if ((option_val = GetNextToken(option_val, &value)) == NULL)
	  {
	    fvwm_msg(ERR, "FvwmBacker", "Page option requires 2 values");
	    return;
	  }
	  if (!StrEquals(value, "*"))
	    this->x = atoi(value);
	  free(value);

	  if (GetNextToken(option_val, &value) == NULL)
	  {
	    fvwm_msg(ERR, "FvwmBacker", "Desk option requires 2 values");
	    return;
	  }
	  if (!StrEquals(value, "*"))
	    this->y = atoi(value);
	  do_ignore_page = False;
	  free(value);
	}

	free(name);
	free(option);
      }
      free(parens);
      line++;
    }
  }
  else
  {
    fvwm_msg(ERR, "FvwmBacker", CatString2("Unknown directive: ", line));
    return;
  }
  this->flags.do_ignore_desk = do_ignore_desk;
  this->flags.do_ignore_page = do_ignore_page;

  /* Now check the type of command... */

  while (*line && isspace(*line)) line++;
  if (strncasecmp(line, "-solid", 6) == 0)
  {
    /* Solid color command */

    line += 6;
    line = GetNextToken(line, &token);
    this->type = 1;
    this->solidColor = (!token || !*token) ?
      BlackPixel(dpy, screen) :
      BackerGetColor(token);
    free(token);
  }
  else if (strncasecmp(line, "colorset", 8) == 0)
  {
    /* Colorset command */

    if (sscanf(line + 8, "%d", &this->colorset) < 1)
    {
      this->colorset = 0;
    }
    AllocColorset(this->colorset);
    this->type = 2;
  }
  else
  {
    /* Plain fvwm command */

    this->type = 0;
    this->cmdStr = (char *)safemalloc(strlen(line) + 1);
    strcpy(this->cmdStr, line);
  }

  if (commands->first == NULL)
    commands->first = this;
  if (commands->last != NULL)
    commands->last->next = this;
  commands->last = this;
}
