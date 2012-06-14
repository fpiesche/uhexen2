/*
	sys_main.c
	main loop and system interface

	$Id$
*/

#include "q_stdinc.h"
#include "compiler.h"
#include "arch_def.h"

#include "defs.h"

#include <proto/exec.h>
#include <proto/dos.h>

#include <devices/timer.h>
#include <proto/timer.h>

#include <time.h>

static double		starttime;
static qboolean		first = true;

static BPTR		amiga_stdin, amiga_stdout;
#define	MODE_RAW	1
#define	MODE_NORMAL	0

struct timerequest	*timerio;
struct MsgPort		*timerport;
struct Device		*TimerBase;


//=============================================================================

void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr,error);
	q_vsnprintf (text, sizeof (text), error,argptr);
	va_end (argptr);

	printf ("\nFATAL ERROR: %s\n\n", text);

	exit (1);
}

void Sys_Quit (void)
{
	exit (0);
}


//=============================================================================

char *Sys_ConsoleInput (void)
{
	static char	con_text[256];
	static int	textlen;
	char		c;

	while (WaitForChar(amiga_stdin,10))
	{
		Read (amiga_stdin, &c, 1);
		if (c == '\n' || c == '\r')
		{
			Write(amiga_stdout, "\n", 1);
			con_text[textlen] = '\0';
			textlen = 0;
			return con_text;
		}
		else if (c == 8)
		{
			if (textlen)
			{
				Write(amiga_stdout, "\b \b", 3);
				textlen--;
				con_text[textlen] = '\0';
			}
			continue;
		}
		con_text[textlen] = c;
		textlen++;
		if (textlen < (int) sizeof(con_text))
		{
			Write(amiga_stdout, &c, 1);
			con_text[textlen] = '\0';
		}
		else
		{
		// buffer is full
			textlen = 0;
			con_text[0] = '\0';
			printf("\nConsole input too long!\n");
			break;
		}
	}

	return NULL;
}

double Sys_DoubleTime (void)
{
	struct timeval	tp;
	double		now;

	GetSysTime(&tp);

	now = tp.tv_sec + tp.tv_usec / 1e6;

	if (first)
	{
		first = false;
		starttime = now;
		return 0.0;
	}

	return now - starttime;
}

//=============================================================================

static void Sys_Init (void)
{
	if ((timerport = CreateMsgPort()))
	{
		if ((timerio = CreateIORequest(timerport, sizeof(timerio))))
		{
			if (OpenDevice((STRPTR) TIMERNAME, UNIT_MICROHZ,
					(struct IORequest *) timerio, 0) == 0)
			{
				TimerBase = timerio->tr_node.io_Device;
			}
			else
			{
				DeleteIORequest(timerio);
				DeleteMsgPort(timerport);
			}
		}
		else
		{
			DeleteMsgPort(timerport);
		}
	}

	if (!TimerBase)
		Sys_Error("Can't open timer.device");

	/* 1us wait, for timer cleanup success */
	timerio->tr_node.io_Command = TR_ADDREQUEST;
	timerio->tr_time.tv_secs = 0;
	timerio->tr_time.tv_micro = 1;
	SendIO((struct IORequest *) timerio);
	WaitIO((struct IORequest *) timerio);

	amiga_stdout = Output();
	amiga_stdin = Input();
	SetMode(amiga_stdin, MODE_RAW);
}

static void Sys_AtExit (void)
{
	if (amiga_stdin)
		SetMode(amiga_stdin, MODE_NORMAL);
	if (TimerBase)
	{
		/*
		if (!CheckIO((struct IORequest *) timerio)
		{
			AbortIO((struct IORequest *) timerio);
			WaitIO((struct IORequest *) timerio);
		}
		*/
		WaitIO((struct IORequest *) timerio);
		CloseDevice((struct IORequest *) timerio);
		DeleteIORequest((struct IORequest *) timerio);
		DeleteMsgPort(timerport);
		TimerBase = NULL;
	}
}

int main (int argc, char **argv)
{
	int t;

	if (argc > 1)
	{
		for (t = 1; t < argc; t++)
		{
			if ( !(strcmp(argv[t], "-h")) || !(strcmp(argv[t], "-help")) ||
			     !(strcmp(argv[t], "--help")) || !(strcmp(argv[t], "-?")) )
			{
				printf("HexenWorld master server %s\n\n", VER_HWMASTER_STR);
				printf("Usage:     hwmaster [-port xxxxx]\n");
				printf("See the documentation for details\n\n");
				exit(0);
			}
			else if ( !(strcmp(argv[t], "-v")) || !(strcmp(argv[t], "-version")) ||
				  !(strcmp(argv[t], "--version")) )
			{
				printf("hwmaster %d.%d.%d\n", VER_HWMASTER_MAJ, VER_HWMASTER_MID, VER_HWMASTER_MIN);
				exit(0);
			}
		}
	}

	com_argv = argv;
	com_argc = argc;

	atexit (Sys_AtExit);
	Sys_Init ();

	Cbuf_Init();
	Cmd_Init ();
	SV_InitNet();

	printf ("Exe: "__TIME__" "__DATE__"\n");
	printf("======== HW master %d.%d.%d initialized ========\n\n",
		VER_HWMASTER_MAJ, VER_HWMASTER_MID, VER_HWMASTER_MIN);

	while (1)
	{
		SV_Frame();
	}

	return 0;
}
