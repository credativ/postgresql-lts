/*-------------------------------------------------------------------------
 *
 * kill.c
 *	  kill()
 *
 * Copyright (c) 1996-2009, PostgreSQL Global Development Group
 *
 *	This is a replacement version of kill for Win32 which sends
 *	signals that the backend can recognize.
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/port/kill.c,v 1.12 2009/02/15 13:58:18 mha Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "c.h"

#ifdef WIN32
/* signal sending */
int
pgkill(int pid, int sig)
{
	char		pipename[128];
	BYTE		sigData = sig;
	BYTE		sigRet = 0;
	DWORD		bytes;
	int			pipe_tries;

	/* we allow signal 0 here, but it will be ignored in pg_queue_signal */
	if (sig >= PG_SIGNAL_COUNT || sig < 0)
	{
		errno = EINVAL;
		return -1;
	}
	if (pid <= 0)
	{
		/* No support for process groups */
		errno = EINVAL;
		return -1;
	}
	snprintf(pipename, sizeof(pipename), "\\\\.\\pipe\\pgsignal_%u", pid);

	/*
	 * Writing data to the named pipe can fail for transient reasons.
	 * Therefore, it is useful to retry if it fails.  The maximum number of
	 * calls to make was empirically determined from a 90-hour notification
	 * stress test.
	 */
	for (pipe_tries = 0; pipe_tries < 3; pipe_tries++)
	{
		if (CallNamedPipe(pipename, &sigData, 1, &sigRet, 1, &bytes, 1000))
		{
			if (bytes != 1 || sigRet != sig)
			{
				errno = ESRCH;
				return -1;
			}
			return 0;
		}
	}

	switch (GetLastError())
	{
		case ERROR_BROKEN_PIPE:
		case ERROR_BAD_PIPE:

			/*
			 * These arise transiently as a process is exiting.  Treat them
			 * like POSIX treats a zombie process, reporting success.
			 */
			return 0;

		case ERROR_FILE_NOT_FOUND:
			/* pipe fully gone, so treat the process as gone */
			errno = ESRCH;
			return -1;
		case ERROR_ACCESS_DENIED:
			errno = EPERM;
			return -1;
		default:
			errno = EINVAL;		/* unexpected */
			return -1;
	}
}

#endif
