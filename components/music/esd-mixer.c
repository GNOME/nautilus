
#include "config.h"
#include "esd-audio.h"

#include <sys/ioctl.h>
#if defined(HAVE_SYS_SOUNDCARD_H)
#include <sys/soundcard.h>
#elif defined(HAVE_MACHINE_SOUNDCARD_H)
#include <machine/soundcard.h>
#endif

void 
esdout_get_volume(int *l, int *r)
{
#if defined(HAVE_SYS_SOUNDCARD_H) || defined(HAVE_MACHINE_SOUNDCARD_H)
	int fd, v, cmd, devs;

	if (esd_cfg.use_remote)
	{
		*l = 100;
		*r = 100;
		return;
	}

	fd = open(DEV_MIXER, O_RDONLY);
	if (fd != -1)
	{
		ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
		if (devs & SOUND_MASK_PCM)
			cmd = SOUND_MIXER_READ_PCM;
		else if (devs & SOUND_MASK_VOLUME)
			cmd = SOUND_MIXER_READ_VOLUME;
		else
		{
			close(fd);
			return;
		}
		ioctl(fd, cmd, &v);
		*r = (v & 0xFF00) >> 8;
		*l = (v & 0x00FF);
		close(fd);
	}
#else
	*l = 100;
	*r = 100;
#endif
}

void 
esdout_set_volume(int l, int r)
{
#if defined(HAVE_SYS_SOUNDCARD_H) || defined(HAVE_MACHINE_SOUNDCARD_H)

	int fd, v, cmd, devs;

	if (esd_cfg.use_remote)
		return;

	fd = open(DEV_MIXER, O_RDONLY);

	if (fd != -1)
	{
		ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
		if (devs & SOUND_MASK_PCM)
			cmd = SOUND_MIXER_WRITE_PCM;
		else if (devs & SOUND_MASK_VOLUME)
			cmd = SOUND_MIXER_WRITE_VOLUME;
		else
		{
			close(fd);
			return;
		}
		v = (r << 8) | l;
		ioctl(fd, cmd, &v);
		close(fd);
	}
#endif
}
