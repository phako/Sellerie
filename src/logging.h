/*
 *   This file is part of GtkTerm.
 *
 *   GtkTerm is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   GtkTerm is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with GtkTerm.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LOGGING_H_
#define LOGGING_H_

void logging_start(const char *file_name);
void logging_pause_resume(void);
void logging_stop(void);
void logging_clear(void);
void log_chars(const char *chars, size_t size);

gboolean gt_logging_get_active(void);
const char *gt_logging_get_default_file(void);

#endif /* LOGGING_H_ */
