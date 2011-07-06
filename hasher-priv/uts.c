#include <errno.h>
#include <error.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "priv.h"

void
unshare_uts(void)
{
#ifdef CLONE_NEWUTS
	const char *name = "localhost.localdomain";

	if (unshare(CLONE_NEWUTS) < 0)
	{
		if (errno == ENOSYS || errno == EINVAL || errno == EPERM) {
			error(share_uts ? EXIT_SUCCESS : EXIT_FAILURE, errno,
			      "UTS namespace isolation is not supported by the kernel");
			return;
		}
		error(EXIT_FAILURE, errno, "unshare CLONE_NEWUTS");
	}

	if (sethostname(name, strlen(name)) < 0)
		error(EXIT_FAILURE, errno, "sethostname: %s", name);
#else
# warning "unshare(CLONE_NEWUTS) is not available on this system"
#endif
}
