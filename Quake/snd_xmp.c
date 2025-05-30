/* tracker music (module file) decoding support using libxmp >= v4.5.0
 * https://sourceforge.net/projects/xmp/
 * https://github.com/libxmp/libxmp.git
 *
 * Copyright (C) 2016 O.Sezer <sezero@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "quakedef.h"

#if defined(USE_CODEC_XMP)
#include "snd_codec.h"
#include "snd_codeci.h"
#include "snd_xmp.h"
#if defined(_WIN32) && defined(LIBXMP_STATIC)
#define BUILDING_STATIC
#endif
#include <xmp.h>
#if ((XMP_VERCODE+0) < 0x040500)
#error libxmp version 4.5 or newer is required
#endif

static qboolean S_XMP_CodecInitialize (void)
{
	return true;
}

static void S_XMP_CodecShutdown (void)
{
}

static unsigned long xmp_fread(void *dest, unsigned long len, unsigned long nmemb, void *f)
{
	return (unsigned long)QFS_ReadFile((qfshandle_t*)f, dest, (size_t)len);
}
static int xmp_fseek(void *f, long offset, int whence)
{
	return QFS_Seek((qfshandle_t*)f, (qfileofs_t)offset, whence);
}
static long xmp_ftell(void *f)
{
	return (long)QFS_Tell((qfshandle_t*)f);
}


static qboolean S_XMP_CodecOpenStream (snd_stream_t *stream)
{
/* libxmp >= 4.5 introduces file callbacks, we now require this feature. */
	xmp_context c;
	struct xmp_callbacks file_callbacks = {
		xmp_fread, xmp_fseek, xmp_ftell, NULL
	};

	int fmt;

	c = xmp_create_context();
	if (c == NULL)
		return false;

	if (xmp_load_module_from_callbacks(c, stream->fh, file_callbacks) < 0) {
		Con_DPrintf("Could not load module %s\n", stream->name);
		goto err1;
	}

	stream->priv = c;
	if (shm->speed > XMP_MAX_SRATE)
		stream->info.rate = 44100;
	else if (shm->speed < XMP_MIN_SRATE)
		stream->info.rate = 11025;
	else	stream->info.rate = shm->speed;
	stream->info.bits = shm->samplebits;
	stream->info.width = stream->info.bits / 8;
	stream->info.channels = shm->channels;

	fmt = 0;
	if (stream->info.channels == 1)
		fmt |= XMP_FORMAT_MONO;
	if (stream->info.width == 1)
		fmt |= XMP_FORMAT_8BIT|XMP_FORMAT_UNSIGNED;
	if (xmp_start_player(c, stream->info.rate, fmt) < 0)
		goto err2;

	/* interpolation type, default is XMP_INTERP_LINEAR */
	xmp_set_player(c, XMP_PLAYER_INTERP, XMP_INTERP_SPLINE);

	return true;

err2:	xmp_release_module(c);
err1:	xmp_free_context(c);
	return false;
}

static int S_XMP_CodecReadStream (snd_stream_t *stream, int bytes, void *buffer)
{
	int r;
	/* xmp_play_buffer() requires libxmp >= 4.1.  it will write
	 * native-endian pcm data to the buffer.  if the data write
	 * is partial, the rest of the buffer will be zero-filled.
	 * the last param is the max number that the current sequence
	 * of song will be looped, or 0 to disable loop checking.  */
	r = xmp_play_buffer((xmp_context)stream->priv, buffer, bytes, !stream->loop);
	if (r == 0) {
		return bytes;
	}
	if (r == -XMP_END) {
		Con_DPrintf("XMP EOF\n");
		return 0;
	}
	return -1;
}

static void S_XMP_CodecCloseStream (snd_stream_t *stream)
{
	xmp_context c = (xmp_context)stream->priv;
	xmp_end_player(c);
	xmp_release_module(c);
	xmp_free_context(c);
	S_CodecUtilClose(&stream);
}

static int S_XMP_CodecJumpToOrder (snd_stream_t *stream, int to)
{
	return xmp_set_position((xmp_context)stream->priv, to);
}

static int S_XMP_CodecRewindStream (snd_stream_t *stream)
{
	int ret = xmp_seek_time((xmp_context)stream->priv, 0);
	if (ret < 0) return ret;
	xmp_play_buffer((xmp_context)stream->priv, NULL, 0, 0); /* reset internal state */
	return 0;
}

snd_codec_t xmp_codec =
{
	CODECTYPE_MOD,
	true,	/* always available. */
	"s3m",
	S_XMP_CodecInitialize,
	S_XMP_CodecShutdown,
	S_XMP_CodecOpenStream,
	S_XMP_CodecReadStream,
	S_XMP_CodecRewindStream,
	S_XMP_CodecJumpToOrder,
	S_XMP_CodecCloseStream,
	NULL
};

#endif	/* USE_CODEC_XMP */
