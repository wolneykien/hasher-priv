#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>

#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>

#if defined (__GLIBC__) && defined (_SEM_SEMUN_UNDEFINED)
union semun
{
	int     val;
	struct semid_ds *buf;
	unsigned short *array;
};
#endif /* __GLIBC__ && _SEM_SEMUN_UNDEFINED */

static void
purge_sem (uid_t uid)
{
	int     maxid, id;
	struct seminfo seminfo;
	union semun arg;

	arg.array = (unsigned short *) &seminfo;
	maxid = semctl (0, 0, SEM_INFO, arg);
	if (maxid < 0)
	{
		error (0, errno, "semctl: SEM_INFO");
		return;
	}

	for (id = 0; id <= maxid; ++id)
	{
		int     semid;
		struct semid_ds buf;

		arg.buf = (struct semid_ds *) &buf;
		if ((semid = semctl (id, 0, SEM_STAT, arg)) < 0)
			continue;

		if (uid != buf.sem_perm.uid)
			continue;

		arg.val = 0;
		semctl (semid, 0, IPC_RMID, arg);	/* ignore return code */
	}
}

static void
purge_shm (uid_t uid)
{
	int     maxid, id;
	struct shm_info shm_info;

	maxid = shmctl (0, SHM_INFO, (struct shmid_ds *) &shm_info);
	if (maxid < 0)
	{
		error (0, errno, "shmctl: SHM_INFO");
		return;
	}

	for (id = 0; id <= maxid; ++id)
	{
		int     shmid;
		struct shmid_ds buf;

		if ((shmid = shmctl (id, SHM_STAT, &buf)) < 0)
			continue;

		if (uid != buf.shm_perm.uid)
			continue;

		shmctl (shmid, IPC_RMID, 0);	/* ignore return code */
	}
}

static void
purge_msg (uid_t uid)
{
	int     maxid, id;
	struct msginfo msginfo;

	maxid = msgctl (0, MSG_INFO, (struct msqid_ds *) &msginfo);
	if (maxid < 0)
	{
		error (0, errno, "msgctl: MSG_INFO");
		return;
	}

	for (id = 0; id <= maxid; ++id)
	{
		int     msqid;
		struct msqid_ds buf;

		if ((msqid = msgctl (id, MSG_STAT, &buf)) < 0)
			continue;

		if (uid != buf.msg_perm.uid)
			continue;

		msgctl (msqid, IPC_RMID, 0);	/* ignore return code */
	}
}

void
purge_ipc (uid_t uid)
{
	purge_sem (uid);
	purge_shm (uid);
	purge_msg (uid);
}
