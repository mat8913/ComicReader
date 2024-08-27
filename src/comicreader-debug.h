/* comicreader-debug.h
 *
 * Copyright 2024 Matthew Harm Bekkema
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

static inline void debug_printf(const char *fmt, ...) G_GNUC_PRINTF(1, 2);

static inline void debug_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "DEBUG: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
}

static inline void abort_printf(const char *fmt, ...) G_GNUC_PRINTF(1, 2);

static inline void abort_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "FATAL: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	abort();
}

static inline void debug_init(const char *name, const void *p)
{
	debug_printf("init %s (%p)\n", name, p);
}

static inline void debug_free(const char *name, const void *p)
{
	debug_printf("free %s (%p)\n", name, p);
}
